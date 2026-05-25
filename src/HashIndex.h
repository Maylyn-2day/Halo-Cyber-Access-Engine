// HashIndex.h
#ifndef HASH_INDEX_H
#define HASH_INDEX_H

#include <cstdint>

#include "DynamicArray.h"
#include "LogEntry.h"
#include "SortUtils.h"

/*
 * HashIndex maps a compact uint32_t key to a timeline of LogEntry pointers.
 *
 * Example:
 * - userId     -> all logs for that user
 * - resourceId -> all logs for that resource
 *
 * The LogEntry objects are NOT owned by HashIndex. They are owned by LogStore.
 * HashIndex only stores const pointers to those existing entries.
 */
class HashIndex {
private:
    struct Node {
        uint32_t key;
        DynamicArray<const LogEntry*> entries;
        Node* next;

        explicit Node(uint32_t nodeKey)
            : key(nodeKey),
              entries(8),
              next(nullptr) {}
    };

    Node** buckets;
    uint32_t bucketCount;
    uint32_t keyCount;

    uint32_t hashKey(uint32_t key) const {
        /*
         * Integer mixing hash.
         * Cheap and effective for compact dictionary IDs.
         */
        key ^= key >> 16;
        key *= 0x7feb352dU;
        key ^= key >> 15;
        key *= 0x846ca68bU;
        key ^= key >> 16;

        return key % bucketCount;
    }

public:
    explicit HashIndex(uint32_t bucketSize)
        : buckets(nullptr),
          bucketCount(bucketSize),
          keyCount(0) {
        if (bucketCount == 0) {
            bucketCount = 1;
        }

        buckets = new Node*[bucketCount];

        for (uint32_t i = 0; i < bucketCount; ++i) {
            buckets[i] = nullptr;
        }
    }

    /*
     * Deletes every Node in every bucket chain.
     *
     * Each Node owns its DynamicArray of pointers. The pointers inside that
     * array are non-owning and must not be deleted here.
     */
    ~HashIndex() {
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

    HashIndex(const HashIndex& other) = delete;
    HashIndex& operator=(const HashIndex& other) = delete;

    /*
     * Inserts one LogEntry pointer under the given key.
     *
     * If the key already exists, the pointer is appended to that key's
     * DynamicArray. Otherwise, a new Node is created at the head of the
     * selected bucket chain.
     */
    void insert(uint32_t key, const LogEntry* entry) {
        uint32_t index = hashKey(key);
        Node* current = buckets[index];

        while (current != nullptr) {
            if (current->key == key) {
                current->entries.pushBack(entry);
                return;
            }

            current = current->next;
        }

        Node* created = new Node(key);
        created->entries.pushBack(entry);
        created->next = buckets[index];
        buckets[index] = created;
        ++keyCount;
    }

    /*
     * Returns the timeline array for a key, or nullptr when not found.
     */
    const DynamicArray<const LogEntry*>* get(uint32_t key) const {
        uint32_t index = hashKey(key);
        Node* current = buckets[index];

        while (current != nullptr) {
            if (current->key == key) {
                return &current->entries;
            }

            current = current->next;
        }

        return nullptr;
    }

    /*
     * Sorts every indexed timeline by timestamp.
     *
     * This keeps sorting encapsulated inside HashIndex. SearchEngine only asks
     * the index to finalize itself after all insertions are complete.
     */
    void sortAllTimelines() {
        for (uint32_t i = 0; i < bucketCount; ++i) {
            Node* current = buckets[i];

            while (current != nullptr) {
                SortUtils::sortByTimestamp(current->entries);
                current = current->next;
            }
        }
    }

    uint32_t size() const {
        return keyCount;
    }

    uint32_t getBucketCount() const {
        return bucketCount;
    }
};

#endif