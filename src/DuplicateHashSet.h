// DuplicateHashSet.h
#ifndef DUPLICATE_HASH_SET_H
#define DUPLICATE_HASH_SET_H

#include <cstdint>
#include <string>

/*
 * DuplicateHashSet stores 64-bit fingerprints of raw CSV lines.
 *
 * The loader hashes each raw line before parsing it. If the fingerprint is
 * already present, the line is skipped immediately, avoiding expensive CSV
 * splitting, validation, and dictionary lookup work.
 */
class DuplicateHashSet {
private:
    struct FingerprintNode {
        unsigned long long fingerprint;
        FingerprintNode* next;

        explicit FingerprintNode(unsigned long long value)
            : fingerprint(value),
              next(nullptr) {}
    };

    FingerprintNode** buckets;
    uint32_t bucketCount;
    uint32_t itemCount;

public:
    explicit DuplicateHashSet(uint32_t bucketSize)
        : buckets(nullptr),
          bucketCount(bucketSize),
          itemCount(0) {
        if (bucketCount == 0) {
            bucketCount = 1;
        }

        buckets = new FingerprintNode*[bucketCount];

        for (uint32_t i = 0; i < bucketCount; ++i) {
            buckets[i] = nullptr;
        }
    }

    /*
     * Walks every bucket chain and deletes every node.
     * This guarantees that all dynamically allocated FingerprintNode objects
     * are released before the hash set is destroyed.
     */
    ~DuplicateHashSet() {
        for (uint32_t i = 0; i < bucketCount; ++i) {
            FingerprintNode* current = buckets[i];

            while (current != nullptr) {
                FingerprintNode* next = current->next;
                delete current;
                current = next;
            }

            buckets[i] = nullptr;
        }

        delete[] buckets;
        buckets = nullptr;
        bucketCount = 0;
        itemCount = 0;
    }

    /*
     * Copying is disabled because this class owns linked-list nodes and a raw
     * bucket array. Default copying would duplicate pointers, not ownership.
     */
    DuplicateHashSet(const DuplicateHashSet& other) = delete;
    DuplicateHashSet& operator=(const DuplicateHashSet& other) = delete;

    /*
     * djb2 string hash.
     *
     * Used on the entire raw CSV line before parsing. The output is stored as
     * an unsigned long long fingerprint.
     */
    static unsigned long long djb2(const std::string& line) {
        unsigned long long hash = 5381ULL;

        for (uint32_t i = 0; i < line.length(); ++i) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(line[i]);
        }

        return hash;
    }

    /*
     * Inserts the fingerprint only if it does not already exist.
     *
     * Returns:
     * - true when the fingerprint was newly inserted
     * - false when the fingerprint already existed
     */
    bool insertIfAbsent(unsigned long long fingerprint) {
        uint32_t index = static_cast<uint32_t>(fingerprint % bucketCount);
        FingerprintNode* current = buckets[index];

        while (current != nullptr) {
            if (current->fingerprint == fingerprint) {
                return false;
            }

            current = current->next;
        }

        FingerprintNode* created = new FingerprintNode(fingerprint);
        created->next = buckets[index];
        buckets[index] = created;
        ++itemCount;

        return true;
    }

    uint32_t size() const {
        return itemCount;
    }

    uint32_t getBucketCount() const {
        return bucketCount;
    }
};

#endif