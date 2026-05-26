#ifndef HASH_INDEX_H
#define HASH_INDEX_H

#include <cstdint>

#include "DynamicArray.h"
#include "LogEntry.h"

class HashIndex {
private:
    struct Node {
        uint32_t key;
        DynamicArray<const LogEntry*> entries;
        Node* next;

        explicit Node(uint32_t nodeKey);
    };

    Node** buckets;
    uint32_t bucketCount;
    uint32_t keyCount;

    uint32_t hashKey(uint32_t key) const;

public:
    explicit HashIndex(uint32_t bucketSize);
    ~HashIndex();

    HashIndex(const HashIndex& other) = delete;
    HashIndex& operator=(const HashIndex& other) = delete;

    void insert(uint32_t key, const LogEntry* entry);
    const DynamicArray<const LogEntry*>* get(uint32_t key) const;
    void sortAllTimelines();
    uint32_t size() const;
    uint32_t getBucketCount() const;
};

#endif