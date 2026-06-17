/**
 * @file SearchEngine.cpp
 * @brief Implementation of the SearchEngine class.
 *
 * Populates and maintains hash-based indices over the LogStore,
 * enabling O(1) average time complexity lookups for user and resource access histories.
 */
#include "SearchEngine.h"

#include "../storage/LogStore.h"

/**
 * @brief Constructs a SearchEngine with specified bucket capacities.
 * @param userBuckets Number of buckets for the user index.
 * @param resourceBuckets Number of buckets for the resource index.
 */
SearchEngine::SearchEngine(uint32_t userBuckets, uint32_t resourceBuckets)
    : userIndex(userBuckets), resourceIndex(resourceBuckets) {}

/**
 * @brief Scans the LogStore and populates the hash indices.
 *
 * Iterates through all LogChunks sequentially and inserts pointers to each
 * LogEntry into the corresponding user and resource indices. Finally, it sorts
 * the resulting timelines chronologically to enable fast binary search querying.
 *
 * @param store The LogStore containing the data to index.
 */
void SearchEngine::buildIndices(const LogStore &store) {
  for (uint32_t chunkIndex = 0; chunkIndex < store.chunkCount(); ++chunkIndex) {
    const LogChunk *chunk = store.getChunk(chunkIndex);

    if (chunk == nullptr) {
      continue;
    }

    const LogEntry *entries = chunk->raw();
    uint32_t entryCount = chunk->size();

    for (uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
      const LogEntry *entry = &entries[entryIndex];

      userIndex.insert(entry->userId, entry);
      resourceIndex.insert(entry->resourceId, entry);
    }
  }

  userIndex.sortAllTimelines();
  resourceIndex.sortAllTimelines();
}

/**
 * @brief Retrieves the chronological timeline of events for a specific user.
 * @param userId The hashed ID of the user.
 * @return Pointer to a dynamic array of LogEntry pointers, or nullptr if not found.
 */
const DynamicArray<const LogEntry *> *
SearchEngine::searchByUser(uint32_t userId) const {
  return userIndex.get(userId);
}

/**
 * @brief Clears all internal indices.
 *
 * Safely releases dynamically allocated memory in both the user and resource indices,
 * preparing the engine for a fresh buildIndices() call (e.g., after reloading data).
 */
void SearchEngine::reset() {
  userIndex.reset();
  resourceIndex.reset();
}

/**
 * @brief Retrieves the chronological timeline of events for a specific resource.
 * @param resourceId The hashed ID of the resource.
 * @return Pointer to a dynamic array of LogEntry pointers, or nullptr if not found.
 */
const DynamicArray<const LogEntry *> *
SearchEngine::searchByResource(uint32_t resourceId) const {
  return resourceIndex.get(resourceId);
}