/**
 * @file QueryEngine.cpp
 * @brief Implementation of the QueryEngine class.
 *
 * Provides analytical query executions, utilizing manual memory management
 * and optimized algorithms (like binary search via SortUtils) for performance.
 */
#define _CRT_SECURE_NO_WARNINGS
#include "QueryEngine.h"

#include <cstdio>
#include <ctime>
#include <iostream>

#include "../core/SortUtils.h"
#include "../core/StringPool.h"
#include "../indexing/SearchEngine.h"
#include "../storage/LogStore.h"
#include "../ConsoleColor.h"

/**
 * @brief Default constructor for TopResource.
 * Initializes resourceId and count to 0.
 */
QueryEngine::TopResource::TopResource() : resourceId(0), count(0) {}

/**
 * @brief Checks if a given timestamp falls within a specified time range.
 * @param timestamp The timestamp to check.
 * @param startTime The start of the time range.
 * @param endTime The end of the time range.
 * @return True if timestamp is within [startTime, endTime], false otherwise.
 */
bool QueryEngine::isWithinRange(int64_t timestamp, int64_t startTime,
                                int64_t endTime) {
  return timestamp >= startTime && timestamp <= endTime;
}

/**
 * @brief Converts an epoch timestamp to a formatted string (UTC).
 * @param epoch The timestamp in seconds since epoch.
 * @param buffer The buffer to write the formatted string to.
 * @param bufferSize The size of the buffer.
 */
void QueryEngine::formatTimestamp(int64_t epoch, char *buffer,
                                  uint32_t bufferSize) {
  time_t rawTime = static_cast<time_t>(epoch);
  struct tm *timeInfo = gmtime(&rawTime);

  if (timeInfo == nullptr) {
    std::snprintf(buffer, bufferSize, "%lld", static_cast<long long>(epoch));
    return;
  }

  std::snprintf(buffer, bufferSize, "%04d-%02d-%02d %02d:%02d:%02d",
                timeInfo->tm_year + 1900, timeInfo->tm_mon + 1,
                timeInfo->tm_mday, timeInfo->tm_hour, timeInfo->tm_min,
                timeInfo->tm_sec);
}

/**
 * @brief Inserts a resource into the top 10 array, shifting others down.
 * @param topResources Array of the current top 10 resources.
 * @param resourceId The ID of the resource to consider.
 * @param count The access count of the resource.
 */
void QueryEngine::insertIntoTop10(TopResource topResources[10],
                                  uint32_t resourceId, uint32_t count) {
  if (count == 0) {
    return;
  }

  for (uint32_t position = 0; position < 10; ++position) {
    if (count > topResources[position].count) {
      for (uint32_t shift = 9; shift > position; --shift) {
        topResources[shift] = topResources[shift - 1];
      }

      topResources[position].resourceId = resourceId;
      topResources[position].count = count;
      return;
    }
  }
}

/**
 * @brief Prints the chronological access history of a specific user.
 * 
 * Uses binary search (SortUtils::lowerBound/upperBound) to quickly find the
 * target time range within the user's pre-sorted LogEntry array. Includes
 * an interactive console pager.
 * 
 * @param userId The hashed ID of the user.
 * @param startTime The start of the time window to consider.
 * @param endTime The end of the time window to consider.
 * @param engine Reference to the SearchEngine containing the indices.
 * @param pool Reference to the StringPool to resolve hashed IDs to strings.
 */
void QueryEngine::printUserJourney(uint32_t userId, int64_t startTime,
                                   int64_t endTime, const SearchEngine &engine,
                                   const StringPool &pool) {
  const DynamicArray<const LogEntry *> *timeline = engine.searchByUser(userId);

  if (timeline == nullptr) {
    std::cout << "No events found for user: " << pool.getString(userId) << '\n';
    return;
  }

  uint32_t lo = SortUtils::lowerBound(*timeline, startTime);
  uint32_t hi = SortUtils::upperBound(*timeline, endTime);
  uint32_t totalResults = hi - lo;

  std::cout << "User Journey: " << pool.getString(userId)
            << " (" << totalResults << " events found)\n";

  if (totalResults == 0) {
    std::cout << "  No events in the specified time range.\n";
    return;
  }

  const uint32_t PAGE_SIZE = 50;
  uint32_t printed = 0;

  for (uint32_t i = lo; i < hi; ++i) {
    const LogEntry *entry = (*timeline)[i];

    if (entry == nullptr) {
      continue;
    }

    char timeStr[32];
    formatTimestamp(entry->timestamp, timeStr, sizeof(timeStr));

    std::printf("  - %s | %-13s  %s -> %s -> %s (%s)\n", timeStr,
                eventTypeToString(entry->eventType),
                pool.getString(entry->deviceId).c_str(),
                pool.getString(entry->appId).c_str(),
                pool.getString(entry->resourceId).c_str(),
                locationToString(entry->location));

    ++printed;

    // Pager: pause after every PAGE_SIZE rows
    if (printed % PAGE_SIZE == 0 && i + 1 < hi) {
      std::cout << ConsoleColor::GRAY
                << "--- Showing " << printed << " / " << totalResults
                << ". Press [Enter] to continue, [Q] to quit ---"
                << ConsoleColor::RESET << '\n';
      std::string input;
      std::getline(std::cin, input);
      if (!input.empty() && (input[0] == 'q' || input[0] == 'Q')) {
        std::cout << ConsoleColor::GRAY << "  (Stopped at " << printed
                  << " / " << totalResults << ")" << ConsoleColor::RESET
                  << '\n';
        return;
      }
    }
  }
}

/**
 * @brief Prints the chronological access history of a specific resource.
 *
 * Similar to printUserJourney, uses binary search for O(log N) time filtering
 * and provides an interactive pager for viewing large datasets.
 *
 * @param resourceId The hashed ID of the resource.
 * @param startTime The start of the time window to consider.
 * @param endTime The end of the time window to consider.
 * @param engine Reference to the SearchEngine containing the indices.
 * @param pool Reference to the StringPool to resolve hashed IDs to strings.
 */
void QueryEngine::printResourceJourney(uint32_t resourceId, int64_t startTime,
                                       int64_t endTime,
                                       const SearchEngine &engine,
                                       const StringPool &pool) {
  const DynamicArray<const LogEntry *> *timeline =
      engine.searchByResource(resourceId);

  if (timeline == nullptr) {
    std::cout << "No events found for resource: " << pool.getString(resourceId)
              << '\n';
    return;
  }

  uint32_t lo = SortUtils::lowerBound(*timeline, startTime);
  uint32_t hi = SortUtils::upperBound(*timeline, endTime);
  uint32_t totalResults = hi - lo;

  std::cout << "Resource Journey: " << pool.getString(resourceId)
            << " (" << totalResults << " events found)\n";

  if (totalResults == 0) {
    std::cout << "  No events in the specified time range.\n";
    return;
  }

  const uint32_t PAGE_SIZE = 50;
  uint32_t printed = 0;

  for (uint32_t i = lo; i < hi; ++i) {
    const LogEntry *entry = (*timeline)[i];

    if (entry == nullptr) {
      continue;
    }

    char timeStr[32];
    formatTimestamp(entry->timestamp, timeStr, sizeof(timeStr));

    std::printf("  - %s | %-13s  %s -> %s -> %s (%s)\n", timeStr,
                eventTypeToString(entry->eventType),
                pool.getString(entry->userId).c_str(),
                pool.getString(entry->deviceId).c_str(),
                pool.getString(entry->appId).c_str(),
                locationToString(entry->location));

    ++printed;

    // Pager: pause after every PAGE_SIZE rows
    if (printed % PAGE_SIZE == 0 && i + 1 < hi) {
      std::cout << ConsoleColor::GRAY
                << "--- Showing " << printed << " / " << totalResults
                << ". Press [Enter] to continue, [Q] to quit ---"
                << ConsoleColor::RESET << '\n';
      std::string input;
      std::getline(std::cin, input);
      if (!input.empty() && (input[0] == 'q' || input[0] == 'Q')) {
        std::cout << ConsoleColor::GRAY << "  (Stopped at " << printed
                  << " / " << totalResults << ")" << ConsoleColor::RESET
                  << '\n';
        return;
      }
    }
  }
}

/**
 * @brief Calculates and prints the top 10 most accessed resources within a time frame.
 *
 * Employs a custom manual hash map (CountNode) to aggregate counts without STL overhead.
 * Additionally features Chunk-level skipping (O(1) bounds checking) to dramatically
 * reduce memory scanning.
 *
 * @param startTime The start of the time window to consider.
 * @param endTime The end of the time window to consider.
 * @param store Reference to the LogStore containing all log entries.
 */
void QueryEngine::printTop10Resources(int64_t startTime, int64_t endTime,
                                      const LogStore &store) {
  if (store.size() == 0) {
    std::cout << "No resources available.\n";
    return;
  }

  struct CountNode {
    uint32_t resourceId;
    uint32_t count;
    CountNode *next;
    explicit CountNode(uint32_t id) : resourceId(id), count(1), next(nullptr) {}
  };

  uint32_t bucketCount = 100003;
  CountNode **buckets = new CountNode *[bucketCount]();

  for (uint32_t chunkIndex = 0; chunkIndex < store.chunkCount(); ++chunkIndex) {
    const LogChunk *chunk = store.getChunk(chunkIndex);

    if (chunk == nullptr) {
      continue;
    }

    // Skip the entire Chunk if its time-range doesn't overlap with query window.
    // Cost: 2 int64 comparisons, saving up to 8192 entry checks.
    if (chunk->getMaxTimestamp() < startTime ||
        chunk->getMinTimestamp() > endTime) {
      continue;
    }

    const LogEntry *entries = chunk->raw();
    uint32_t entryCount = chunk->size();

    for (uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
      const LogEntry &entry = entries[entryIndex];

      if (!isWithinRange(entry.timestamp, startTime, endTime)) {
        continue;
      }

      uint32_t index = entry.resourceId % bucketCount;
      CountNode *current = buckets[index];
      bool found = false;

      while (current != nullptr) {
        if (current->resourceId == entry.resourceId) {
          current->count++;
          found = true;
          break;
        }
        current = current->next;
      }

      if (!found) {
        CountNode *newNode = new CountNode(entry.resourceId);
        newNode->next = buckets[index];
        buckets[index] = newNode;
      }
    }
  }

  TopResource topResources[10];

  for (uint32_t i = 0; i < bucketCount; ++i) {
    CountNode *current = buckets[i];
    while (current != nullptr) {
      insertIntoTop10(topResources, current->resourceId, current->count);
      CountNode *next = current->next;
      delete current;
      current = next;
    }
  }

  delete[] buckets;

  std::cout << "Top 10 Resources\n";

  for (uint32_t i = 0; i < 10; ++i) {
    if (topResources[i].count == 0) {
      break;
    }

    std::cout << "  " << (i + 1) << ". "
              << store.stringPool.getString(topResources[i].resourceId)
              << " | Count: " << topResources[i].count << '\n';
  }
}