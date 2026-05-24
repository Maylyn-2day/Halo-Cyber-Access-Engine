// LogChunk.h
#ifndef LOG_CHUNK_H
#define LOG_CHUNK_H

#include <cstdint>

#include "LogEntry.h"

/*
 * LogChunk owns one contiguous block of LogEntry objects.
 *
 * Instead of allocating one LogEntry at a time, the engine allocates large
 * chunks. This reduces heap fragmentation and improves cache locality during
 * scans, sorting, and index construction.
 */
class LogChunk {
private:
    LogEntry* entries;
    uint32_t capacity;
    uint32_t count;

public:
    explicit LogChunk(uint32_t chunkCapacity)
        : entries(nullptr),
          capacity(chunkCapacity),
          count(0) {
        if (capacity > 0) {
            entries = new LogEntry[capacity];
        }
    }

    /*
     * Releases the entire contiguous LogEntry block.
     * delete[] also calls destructors for all LogEntry objects.
     */
    ~LogChunk() {
        delete[] entries;
        entries = nullptr;
        capacity = 0;
        count = 0;
    }

    /*
     * Copying is disabled because this class owns raw heap memory.
     * Allowing default copies would cause double-free bugs.
     */
    LogChunk(const LogChunk& other) = delete;
    LogChunk& operator=(const LogChunk& other) = delete;

    bool hasSpace() const {
        return count < capacity;
    }

    /*
     * Appends a LogEntry into this chunk.
     *
     * Returns:
     * - pointer to the stored LogEntry when successful
     * - nullptr when the chunk is full
     */
    LogEntry* append(const LogEntry& entry) {
        if (!hasSpace()) {
            return nullptr;
        }

        entries[count] = entry;
        LogEntry* stored = &entries[count];
        ++count;

        return stored;
    }

    LogEntry* raw() {
        return entries;
    }

    const LogEntry* raw() const {
        return entries;
    }

    uint32_t size() const {
        return count;
    }

    uint32_t getCapacity() const {
        return capacity;
    }
};

#endif