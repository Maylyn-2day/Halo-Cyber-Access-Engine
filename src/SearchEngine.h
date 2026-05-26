#ifndef SEARCH_ENGINE_H
#define SEARCH_ENGINE_H

#include <cstdint>

#include "HashIndex.h"
#include "DynamicArray.h"
#include "LogEntry.h"

class LogStore;

class SearchEngine {
private:
    HashIndex userIndex;
    HashIndex resourceIndex;

public:
    SearchEngine(uint32_t userBuckets, uint32_t resourceBuckets);

    SearchEngine(const SearchEngine& other) = delete;
    SearchEngine& operator=(const SearchEngine& other) = delete;

    void buildIndices(const LogStore& store);

    const DynamicArray<const LogEntry*>* searchByUser(uint32_t userId) const;
    const DynamicArray<const LogEntry*>* searchByResource(uint32_t resourceId) const;
};

#endif