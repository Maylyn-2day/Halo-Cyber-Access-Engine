#include "SearchEngine.h"

#include "LogStore.h"

SearchEngine::SearchEngine(uint32_t userBuckets, uint32_t resourceBuckets)
    : userIndex(userBuckets),
      resourceIndex(resourceBuckets) {}

void SearchEngine::buildIndices(const LogStore& store) {
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

const DynamicArray<const LogEntry*>* SearchEngine::searchByUser(uint32_t userId) const {
    return userIndex.get(userId);
}

const DynamicArray<const LogEntry*>* SearchEngine::searchByResource(uint32_t resourceId) const {
    return resourceIndex.get(resourceId);
}