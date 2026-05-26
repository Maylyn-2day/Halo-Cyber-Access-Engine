#include "QueryEngine.h"

#include <iostream>

#include "SearchEngine.h"
#include "LogStore.h"
#include "StringPool.h"

QueryEngine::TopResource::TopResource()
    : resourceId(0),
      count(0) {}

bool QueryEngine::isWithinRange(int64_t timestamp, int64_t startTime, int64_t endTime) {
    return timestamp >= startTime && timestamp <= endTime;
}

void QueryEngine::insertIntoTop10(TopResource topResources[10], uint32_t resourceId, uint32_t count) {
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

void QueryEngine::printUserJourney(
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

void QueryEngine::printResourceJourney(
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

void QueryEngine::printTop10Resources(
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