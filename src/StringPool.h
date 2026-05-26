#ifndef STRING_POOL_H
#define STRING_POOL_H

#include <cstdint>
#include <string>

#include "DynamicArray.h"

class StringPool {
private:
    struct Node {
        std::string key;
        uint32_t id;
        Node* next;

        Node(const std::string& nodeKey, uint32_t nodeId);
    };

    Node** buckets;
    uint32_t bucketCount;
    uint32_t keyCount;
    DynamicArray<std::string> strings;

    unsigned long long hashString(const std::string& str) const;

public:
    explicit StringPool(uint32_t bucketSize = 262147);
    ~StringPool();

    StringPool(const StringPool& other) = delete;
    StringPool& operator=(const StringPool& other) = delete;

    void reserve(uint32_t capacity);
    uint32_t getOrCreateId(const std::string& str);
    std::string getString(uint32_t id) const;
    uint32_t size() const;
    uint32_t getBucketCount() const;
};

#endif