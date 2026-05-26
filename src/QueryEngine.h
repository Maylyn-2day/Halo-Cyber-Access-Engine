#ifndef QUERY_ENGINE_H
#define QUERY_ENGINE_H

#include <cstdint>

class SearchEngine;
class LogStore;
class StringPool;

class QueryEngine {
private:
    struct TopResource {
        uint32_t resourceId;
        uint32_t count;

        TopResource();
    };

    static bool isWithinRange(int64_t timestamp, int64_t startTime, int64_t endTime);
    static void insertIntoTop10(TopResource topResources[10], uint32_t resourceId, uint32_t count);

public:
    static void printUserJourney(
        uint32_t userId,
        int64_t startTime,
        int64_t endTime,
        const SearchEngine& engine,
        const StringPool& pool
    );

    static void printResourceJourney(
        uint32_t resourceId,
        int64_t startTime,
        int64_t endTime,
        const SearchEngine& engine,
        const StringPool& pool
    );

    static void printTop10Resources(
        int64_t startTime,
        int64_t endTime,
        const LogStore& store
    );
};

#endif