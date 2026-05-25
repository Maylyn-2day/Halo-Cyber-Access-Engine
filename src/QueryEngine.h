// QueryEngine.h
#ifndef QUERY_ENGINE_H
#define QUERY_ENGINE_H

#include <cstdint>
#include <iostream>

#include "SearchEngine.h"
#include "LogStore.h"
#include "StringPool.h"

/*
 * QueryEngine encapsulates Phase 1 business queries and analytics.
 *
 * It is intentionally stateless: all methods are static and operate on the
 * already-loaded LogStore, SearchEngine indexes, and StringPool dictionary.
 */
class QueryEngine {
private:
    struct TopResource {
        uint32_t resourceId;
        uint32_t count;

        TopResource()
            : resourceId(0),
              count(0) {}
    };

    static bool isWithinRange(int64_t timestamp, int64_t startTime, int64_t endTime) {
        return timestamp >= startTime && timestamp <= endTime;
    }

    static void insertIntoTop10(TopResource topResources[10], uint32_t resourceId, uint32_t count) {
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

public:
    /*
     * Prints a user's chronological journey inside [startTime, endTime].
     *
     * Output format:
     *   - T: {timestamp} | {Device} -> {App} -> {Resource}
     */
    static void printUserJourney(
        uint32_t userId,
        int64_t startTime,
        int64_t endTime,
        const SearchEngine& engine,
        const StringPool& pool
    ) {
        const DynamicArray<const LogEntry*>* timeline = engine.searchByUser(userId);

        if (timeline == nullptr) {
            std::cout << "No events found for user: " << pool.getString(userId) << '\n';
            return;
        }

        std::cout << "User Journey: " << pool.getString(userId) << '\n';

        for (uint32_t i = 0; i < timeline->size(); ++i) {
            const LogEntry* entry = (*timeline)[i];

            if (entry == nullptr) {
                continue;
            }

            if (entry->timestamp < startTime) {
                continue;
            }

            if (entry->timestamp > endTime) {
                break;
            }

            std::cout << "  - T: " << entry->timestamp
                      << " | " << pool.getString(entry->deviceId)
                      << " -> " << pool.getString(entry->appId)
                      << " -> " << pool.getString(entry->resourceId)
                      << '\n';
        }
    }

    /*
     * Prints a resource's chronological access journey inside [startTime, endTime].
     *
     * Output format:
     *   - T: {timestamp} | {User} -> {Device} -> {App}
     */
    static void printResourceJourney(
        uint32_t resourceId,
        int64_t startTime,
        int64_t endTime,
        const SearchEngine& engine,
        const StringPool& pool
    ) {
        const DynamicArray<const LogEntry*>* timeline = engine.searchByResource(resourceId);

        if (timeline == nullptr) {
            std::cout << "No events found for resource: " << pool.getString(resourceId) << '\n';
            return;
        }

        std::cout << "Resource Journey: " << pool.getString(resourceId) << '\n';

        for (uint32_t i = 0; i < timeline->size(); ++i) {
            const LogEntry* entry = (*timeline)[i];

            if (entry == nullptr) {
                continue;
            }

            if (entry->timestamp < startTime) {
                continue;
            }

            if (entry->timestamp > endTime) {
                break;
            }

            std::cout << "  - T: " << entry->timestamp
                      << " | " << pool.getString(entry->userId)
                      << " -> " << pool.getString(entry->deviceId)
                      << " -> " << pool.getString(entry->appId)
                      << '\n';
        }
    }

    /*
     * Prints the top 10 resources by event frequency inside [startTime, endTime].
     *
     * Counting is O(N) over stored logs. Top-10 maintenance uses a fixed-size
     * array and insertion shifting, avoiding STL containers entirely.
     */
    static void printTop10Resources(
        int64_t startTime,
        int64_t endTime,
        const LogStore& store
    ) {
        uint32_t resourceCapacity = store.stringPool.size();

        if (resourceCapacity == 0) {
            std::cout << "No resources available.\n";
            return;
        }

        uint32_t* counts = new uint32_t[resourceCapacity]();

        for (uint32_t chunkIndex = 0; chunkIndex < store.chunkCount(); ++chunkIndex) {
            const LogChunk* chunk = store.getChunk(chunkIndex);

            if (chunk == nullptr) {
                continue;
            }

            const LogEntry* entries = chunk->raw();
            uint32_t entryCount = chunk->size();

            for (uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
                const LogEntry& entry = entries[entryIndex];

                if (!isWithinRange(entry.timestamp, startTime, endTime)) {
                    continue;
                }

                if (entry.resourceId < resourceCapacity) {
                    ++counts[entry.resourceId];
                }
            }
        }

        TopResource topResources[10];

        for (uint32_t resourceId = 0; resourceId < resourceCapacity; ++resourceId) {
            insertIntoTop10(topResources, resourceId, counts[resourceId]);
        }

        std::cout << "Top 10 Resources\n";

        for (uint32_t i = 0; i < 10; ++i) {
            if (topResources[i].count == 0) {
                break;
            }

            std::cout << "  " << (i + 1)
                      << ". " << store.stringPool.getString(topResources[i].resourceId)
                      << " | Count: " << topResources[i].count
                      << '\n';
        }

        delete[] counts;
        counts = nullptr;
    }
};

#endif