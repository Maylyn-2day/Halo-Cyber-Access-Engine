// LogStore.h
#ifndef LOG_STORE_H
#define LOG_STORE_H

#include <cstdint>

#include "DynamicArray.h"
#include "LogChunk.h"
#include "LogEntry.h"
#include "StringPool.h"

/*
 * LogStore owns all LogChunk objects.
 *
 * Each LogChunk owns a contiguous block of LogEntry objects. LogStore grows
 * by adding new chunks instead of allocating one LogEntry at a time.
 */
class LogStore {
private:
    static const uint32_t DEFAULT_CHUNK_SIZE = 8192;

    DynamicArray<LogChunk*> chunks;
    LogChunk* currentChunk;
    uint64_t totalEntries;

    bool allocateChunk() {
        LogChunk* chunk = new LogChunk(DEFAULT_CHUNK_SIZE);

        chunks.pushBack(chunk);
        currentChunk = chunk;
        return true;
    }

public:
    StringPool stringPool;
    LogStore()
        : chunks(8),
          currentChunk(nullptr),
          totalEntries(0) {}

    explicit LogStore(uint32_t estimatedChunks)
        : chunks(estimatedChunks),
          currentChunk(nullptr),
          totalEntries(0) {}

    /*
     * Deletes every owned LogChunk.
     *
     * DynamicArray only owns the pointer array. LogStore owns the objects
     * pointed to by that array, so it must delete each LogChunk explicitly.
     */
    ~LogStore() {
        for (uint32_t i = 0; i < chunks.size(); ++i) {
            delete chunks[i];
            chunks[i] = nullptr;
        }

        currentChunk = nullptr;
        totalEntries = 0;
    }

    LogStore(const LogStore& other) = delete;
    LogStore& operator=(const LogStore& other) = delete;

    /*
     * Reserves space for chunk pointers only.
     *
     * This does not allocate all LogEntry chunks immediately. It avoids
     * repeated resizing of the DynamicArray<LogChunk*> metadata array.
     */
    void reserveChunks(uint32_t estimatedChunks) {
        chunks.reserve(estimatedChunks);
    }

    /*
     * Inserts one LogEntry into the active chunk.
     *
     * If no chunk exists or the current chunk is full, a new LogChunk(8192)
     * is allocated and appended to the chunk pointer array.
     */
    LogEntry* insert(const LogEntry& entry) {
        if (currentChunk == nullptr || !currentChunk->hasSpace()) {
            if (!allocateChunk()) {
                return nullptr;
            }
        }

        LogEntry* stored = currentChunk->append(entry);

        if (stored != nullptr) {
            ++totalEntries;
        }

        return stored;
    }

    uint64_t size() const {
        return totalEntries;
    }

    uint32_t chunkCount() const {
        return chunks.size();
    }

    LogChunk* getChunk(uint32_t index) {
        return chunks[index];
    }

    const LogChunk* getChunk(uint32_t index) const {
        return chunks[index];
    }
};

#endif