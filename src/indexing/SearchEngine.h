#ifndef SEARCH_ENGINE_H
#define SEARCH_ENGINE_H

/**
 * @file SearchEngine.h
 * @brief Core indexing and retrieval engine for log data.
 *
 * This file defines the SearchEngine class, which maintains hash-based indices
 * over the log data to enable fast O(1) average time complexity lookups by
 * user ID or resource ID.
 */

#include <cstdint>

#include "../core/DynamicArray.h"
#include "../core/HashIndex.h"
#include "../core/LogEntry.h"

class LogStore;

/**
 * @class SearchEngine
 * @brief Manages fast lookup indices for users and resources.
 *
 * The SearchEngine builds and stores hash indices (using HashIndex) mapping
 * hashed IDs (user or resource) to a dynamic array of pointers to the
 * corresponding LogEntry objects.
 */
class SearchEngine {
private:
  HashIndex userIndex;     ///< Index mapping user IDs to their log entries.
  HashIndex resourceIndex; ///< Index mapping resource IDs to their log entries.

public:
  /**
   * @brief Constructs a SearchEngine with specified bucket counts.
   * @param userBuckets Number of buckets for the user index.
   * @param resourceBuckets Number of buckets for the resource index.
   */
  SearchEngine(uint32_t userBuckets, uint32_t resourceBuckets);

  SearchEngine(const SearchEngine &other) = delete;
  SearchEngine &operator=(const SearchEngine &other) = delete;

  /**
   * @brief Builds the indices by iterating over the provided log store.
   * @param store The LogStore containing all parsed log entries.
   */
  void buildIndices(const LogStore &store);

  /**
   * @brief Clears all indices. Call before buildIndices() for safe re-indexing.
   */
  void reset();

  /**
   * @brief Retrieves all log entries associated with a specific user.
   * @param userId The hashed ID of the user.
   * @return A pointer to a DynamicArray of LogEntry pointers, or nullptr if not found.
   */
  const DynamicArray<const LogEntry *> *searchByUser(uint32_t userId) const;

  /**
   * @brief Retrieves all log entries associated with a specific resource.
   * @param resourceId The hashed ID of the resource.
   * @return A pointer to a DynamicArray of LogEntry pointers, or nullptr if not found.
   */
  const DynamicArray<const LogEntry *> *
  searchByResource(uint32_t resourceId) const;
};

#endif