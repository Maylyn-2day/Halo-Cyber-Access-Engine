// DataLoader.h
#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include <string>

#include "DuplicateHashSet.h"
#include "LogStore.h"

/*
 * DataLoader performs high-speed CSV ingestion.
 *
 * It uses FILE* + fread instead of std::getline and parses each line through
 * raw character pointers to avoid stringstream and per-column std::string
 * allocations.
 */
class DataLoader {
public:
    static bool load(
        const std::string& filename,
        LogStore& store,
        DuplicateHashSet& gatekeeper
    );
};

#endif