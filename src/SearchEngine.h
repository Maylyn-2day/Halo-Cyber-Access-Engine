// SearchEngine.h
#ifndef SEARCH_ENGINE_H
#define SEARCH_ENGINE_H

#include <cstdint>

#include "HashIndex.h"
#include "LogStore.h"

/*
 * SearchEngine builds read-only query indexes over LogStore.
 *
 * LogStore owns the actual LogEntry memory. SearchEngine keeps fast lookup
 * structures that point into that stable chunk memory.
 */
class SearchEngine {
private:
    HashIndex userIndex;
    HashIndex resourceIndex;

public:
    SearchEngine(uint32_t userBuckets, uint32_t resourceBuckets)
        : userIndex(userBuckets),
          resourceIndex(resourceBuckets) {}

    SearchEngine(const SearchEngine& other) = delete;
    SearchEngine& operator=(const SearchEngine& other) = delete;

    /*
     * Builds user and resource indexes from every stored LogEntry.
     *
     * Call this after DataLoader finishes inserting rows into LogStore.
     * The LogStore must outlive SearchEngine because indexes store pointers
     * to entries owned by LogStore.
     *
     * After all insertions are complete, every per-key timeline is sorted by
     * timestamp for fast chronological scans and future binary-search queries.
     */
    void buildIndices(const LogStore& store) {
        for (uint32_t chunkIndex = 0; chunkIndex < store.chunkCount(); ++chunkIndex) {
            const LogChunk* chunk = store.getChunk(chunkIndex);

            if (chunk == nullptr) {
                continue;
            }

            const LogEntry* entries = chunk->raw();
            uint32_t entryCount = chunk->size();

            for (uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
                const LogEntry* entry = &entries[entryIndex];

                userIndex.insert(entry->userId, entry);
                resourceIndex.insert(entry->resourceId, entry);
            }
        }

        userIndex.sortAllTimelines();
        resourceIndex.sortAllTimelines();
    }

    const DynamicArray<const LogEntry*>* searchByUser(uint32_t userId) const {
        return userIndex.get(userId);
    }

    const DynamicArray<const LogEntry*>* searchByResource(uint32_t resourceId) const {
        return resourceIndex.get(resourceId);
    }
};

#endif