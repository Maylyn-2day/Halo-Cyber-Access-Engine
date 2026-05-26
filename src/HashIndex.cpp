#include "HashIndex.h"

#include "SortUtils.h"

HashIndex::Node::Node(uint32_t nodeKey)
    : key(nodeKey),
      entries(8),
      next(nullptr) {}

uint32_t HashIndex::hashKey(uint32_t key) const {
    key ^= key >> 16;
    key *= 0x7feb352dU;
    key ^= key >> 15;
    key *= 0x846ca68bU;
    key ^= key >> 16;

    return key % bucketCount;
}

HashIndex::HashIndex(uint32_t bucketSize)
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

HashIndex::~HashIndex() {
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

void HashIndex::insert(uint32_t key, const LogEntry* entry) {
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

const DynamicArray<const LogEntry*>* HashIndex::get(uint32_t key) const {
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

void HashIndex::sortAllTimelines() {
    for (uint32_t i = 0; i < bucketCount; ++i) {
        Node* current = buckets[i];

        while (current != nullptr) {
            SortUtils::sortByTimestamp(current->entries);
            current = current->next;
        }
    }
}

uint32_t HashIndex::size() const {
    return keyCount;
}

uint32_t HashIndex::getBucketCount() const {
    return bucketCount;
}