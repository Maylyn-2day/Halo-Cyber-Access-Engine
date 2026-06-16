#define _CRT_SECURE_NO_WARNINGS
#include "QueryEngine.h"

#include <cstdio>
#include <ctime>
#include <iostream>

#include "../core/SortUtils.h"
#include "../core/StringPool.h"
#include "../indexing/SearchEngine.h"
#include "../storage/LogStore.h"

QueryEngine::TopResource::TopResource() : resourceId(0), count(0) {}

bool QueryEngine::isWithinRange(int64_t timestamp, int64_t startTime,
                                int64_t endTime) {
  return timestamp >= startTime && timestamp <= endTime;
}

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

void QueryEngine::printUserJourney(uint32_t userId, int64_t startTime,
                                   int64_t endTime, const SearchEngine &engine,
                                   const StringPool &pool) {
  const DynamicArray<const LogEntry *> *timeline = engine.searchByUser(userId);

  if (timeline == nullptr) {
    std::cout << "No events found for user: " << pool.getString(userId) << '\n';
    return;
  }

  std::cout << "User Journey: " << pool.getString(userId) << '\n';

  uint32_t lo = SortUtils::lowerBound(*timeline, startTime);
  uint32_t hi = SortUtils::upperBound(*timeline, endTime);

  for (uint32_t i = lo; i < hi; ++i) {
    const LogEntry *entry = (*timeline)[i];

    if (entry == nullptr) {
      continue;
    }

    char timeStr[32];
    formatTimestamp(entry->timestamp, timeStr, sizeof(timeStr));

    std::cout << "  - " << timeStr << " | ["
              << eventTypeToString(entry->eventType) << "] "
              << pool.getString(entry->deviceId) << " -> "
              << pool.getString(entry->appId) << " -> "
              << pool.getString(entry->resourceId) << " ("
              << locationToString(entry->location) << ")" << '\n';
  }
}

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

  std::cout << "Resource Journey: " << pool.getString(resourceId) << '\n';

  uint32_t lo = SortUtils::lowerBound(*timeline, startTime);
  uint32_t hi = SortUtils::upperBound(*timeline, endTime);

  for (uint32_t i = lo; i < hi; ++i) {
    const LogEntry *entry = (*timeline)[i];

    if (entry == nullptr) {
      continue;
    }

    char timeStr[32];
    formatTimestamp(entry->timestamp, timeStr, sizeof(timeStr));

    std::cout << "  - " << timeStr << " | ["
              << eventTypeToString(entry->eventType) << "] "
              << pool.getString(entry->userId) << " -> "
              << pool.getString(entry->deviceId) << " -> "
              << pool.getString(entry->appId) << " ("
              << locationToString(entry->location) << ")" << '\n';
  }
}

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

    // Bỏ qua toàn bộ Chunk nếu time-range của nó không giao với query window.
    // Chi phí: 2 phép so sánh int64, tiết kiệm tới 8192 phép kiểm tra entry.
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