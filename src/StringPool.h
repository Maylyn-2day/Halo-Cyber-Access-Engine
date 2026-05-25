// StringPool.h
#ifndef STRING_POOL_H
#define STRING_POOL_H

#include <cstdint>
#include <string>

#include "DynamicArray.h"

/*
 * StringPool dictionary-encodes repeated strings into compact uint32_t IDs.
 *
 * Forward lookup:
 *   std::string -> uint32_t ID
 *
 * Reverse lookup:
 *   uint32_t ID -> std::string
 *
 * No STL maps, unordered_maps, vectors, or sets are used.
 */
class StringPool {
private:
    struct Node {
        std::string key;
        uint32_t id;
        Node* next;

        Node(const std::string& nodeKey, uint32_t nodeId)
            : key(nodeKey),
              id(nodeId),
              next(nullptr) {}
    };

    Node** buckets;
    uint32_t bucketCount;
    uint32_t keyCount;
    DynamicArray<std::string> strings;

    unsigned long long hashString(const std::string& str) const {
        unsigned long long hash = 5381ULL;

        for (uint32_t i = 0; i < str.length(); ++i) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(str[i]);
        }

        return hash;
    }

public:
    explicit StringPool(uint32_t bucketSize = 262147)
        : buckets(nullptr),
          bucketCount(bucketSize),
          keyCount(0),
          strings(8) {
        if (bucketCount == 0) {
            bucketCount = 1;
        }

        buckets = new Node*[bucketCount];

        for (uint32_t i = 0; i < bucketCount; ++i) {
            buckets[i] = nullptr;
        }
    }

    /*
     * Deletes every linked-list node from every bucket.
     *
     * The DynamicArray<std::string> destructor automatically releases the
     * reverse lookup array and each owned std::string.
     */
    ~StringPool() {
        for (uint32_t i = 0; i < bucketCount; ++i) {
            Node* current = buckets[i];

            while (current != nullptr) {
                Node* next = current->next;
                delete current;
                current = next;
            }

            buckets[i] = nullptr;
        }

        delete[] buckets;
        buckets = nullptr;
        bucketCount = 0;
        keyCount = 0;
    }

    StringPool(const StringPool& other) = delete;
    StringPool& operator=(const StringPool& other) = delete;

    /*
     * Pre-allocates reverse lookup storage.
     *
     * Use this before large CSV ingestion to avoid repeated resizing of the
     * DynamicArray<std::string> backing store.
     */
    void reserve(uint32_t capacity) {
        strings.reserve(capacity);
    }

    /*
     * Returns the existing ID for str, or creates a new sequential ID.
     *
     * The ID is exactly the index where the string is stored in the reverse
     * lookup array.
     */
    uint32_t getOrCreateId(const std::string& str) {
        unsigned long long hash = hashString(str);
        uint32_t index = static_cast<uint32_t>(hash % bucketCount);

        Node* current = buckets[index];

        while (current != nullptr) {
            if (current->key == str) {
                return current->id;
            }

            current = current->next;
        }

        uint32_t newId = strings.size();
        strings.pushBack(str);

        Node* created = new Node(str, newId);
        created->next = buckets[index];
        buckets[index] = created;
        ++keyCount;

        return newId;
    }

    /*
     * Returns a copy of the string for the given ID.
     *
     * Returns an empty string when the ID is outside the reverse lookup array.
     */
    std::string getString(uint32_t id) const {
        if (id >= strings.size()) {
            return "";
        }

        return strings[id];
    }

    uint32_t size() const {
        return keyCount;
    }

    uint32_t getBucketCount() const {
        return bucketCount;
    }
};

#endif