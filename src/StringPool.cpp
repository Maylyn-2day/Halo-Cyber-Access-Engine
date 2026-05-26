#include "StringPool.h"

StringPool::Node::Node(const std::string& nodeKey, uint32_t nodeId)
    : key(nodeKey),
      id(nodeId),
      next(nullptr) {}

unsigned long long StringPool::hashString(const std::string& str) const {
    unsigned long long hash = 5381ULL;

    for (uint32_t i = 0; i < str.length(); ++i) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(str[i]);
    }

    return hash;
}

StringPool::StringPool(uint32_t bucketSize)
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

StringPool::~StringPool() {
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

void StringPool::reserve(uint32_t capacity) {
    strings.reserve(capacity);
}

uint32_t StringPool::getOrCreateId(const std::string& str) {
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

std::string StringPool::getString(uint32_t id) const {
    if (id >= strings.size()) {
        return "";
    }

    return strings[id];
}

uint32_t StringPool::size() const {
    return keyCount;
}

uint32_t StringPool::getBucketCount() const {
    return bucketCount;
}