// DuplicateHashSet.h
#ifndef DUPLICATE_HASH_SET_H
#define DUPLICATE_HASH_SET_H

#include <cstdint>
#include <cstring>
#include <string>

/**
 * @brief Specialized Hash Set structure for storing 64-bit fingerprints of raw
 * CSV data.
 *
 * Architecture decision: Use Open-Addressing with Robin Hood Hashing instead of
 * Separate Chaining. The entire entry is stored inline in a flat array,
 * completely eliminating heap allocation overhead for each node (10M+ `new`
 * calls → 0 calls). This significantly increases Cache Locality because the CPU
 * prefetcher only needs to scan sequentially on a contiguous memory region
 * instead of jumping around the heap.
 *
 * Robin Hood Hashing: When a collision occurs, the new entry "steals" the
 * position of the entry with a shorter probe distance. This keeps the average
 * probe distance extremely low (usually < 2), ensuring practical O(1)
 * performance even when the load factor reaches 70-80%.
 */
class DuplicateHashSet {
private:
  /**
   * @brief Inline slot in the flat array. No need for a `next` pointer.
   * Size: 16 bytes (8 fingerprint + 4 probeDistance + 4 padding/flags).
   */
  struct Slot {
    unsigned long long fingerprint; ///< 64-bit fingerprint.
    uint32_t
        probeDistance; ///< Distance from original bucket (Robin Hood metadata).
    bool occupied;     ///< Flag marking the slot contains valid data.
    uint8_t _pad[3];   ///< Padding alignment.

    Slot() : fingerprint(0), probeDistance(0), occupied(false) {
      _pad[0] = _pad[1] = _pad[2] = 0;
    }
  };

  Slot *slots;        ///< Flat array containing all entries inline.
  uint32_t capacity;  ///< Array size (number of slots).
  uint32_t itemCount; ///< Number of unique fingerprints currently stored.

  /**
   * @brief Automatically expands and rehashes when load factor exceeds 75%
   * threshold.
   *
   * Robin Hood rehash: Iterate through the old array, only insert occupied
   * slots into the new array. No need to traverse a linked-list → faster than
   * Separate Chaining rehash.
   */
  void rehash(uint32_t newCapacity) {
    Slot *oldSlots = slots;
    uint32_t oldCapacity = capacity;

    slots = new Slot[newCapacity]();
    capacity = newCapacity;
    itemCount = 0;

    for (uint32_t i = 0; i < oldCapacity; ++i) {
      if (oldSlots[i].occupied) {
        insertInternal(oldSlots[i].fingerprint);
      }
    }

    delete[] oldSlots;
  }

  /**
   * @brief Internal insert using Robin Hood probing.
   * Called by both insertIfAbsent (public) and rehash (private).
   */
  void insertInternal(unsigned long long fingerprint) {
    uint32_t index = static_cast<uint32_t>(fingerprint % capacity);
    uint32_t dist = 0;

    while (true) {
      if (!slots[index].occupied) {
        // Empty slot → insert
        slots[index].fingerprint = fingerprint;
        slots[index].probeDistance = dist;
        slots[index].occupied = true;
        ++itemCount;
        return;
      }

      // Robin Hood: if the current entry is "richer" (shorter probe distance),
      // we steal its position and push it away to find a new spot.
      if (slots[index].probeDistance < dist) {
        // Swap: current entry is pushed out
        unsigned long long tmpFp = slots[index].fingerprint;
        uint32_t tmpDist = slots[index].probeDistance;

        slots[index].fingerprint = fingerprint;
        slots[index].probeDistance = dist;

        fingerprint = tmpFp;
        dist = tmpDist;
      }

      // Linear probing: advance to the next slot
      index = (index + 1) % capacity;
      ++dist;
    }
  }

public:
  /**
   * @brief Initializes Open-Addressing hash table with a given capacity.
   *
   * @param bucketSize Size of the slots array (Prime number recommended).
   */
  explicit DuplicateHashSet(uint32_t bucketSize)
      : slots(nullptr), capacity(bucketSize), itemCount(0) {
    if (capacity == 0) {
      capacity = 1;
    }

    slots = new Slot[capacity]();
  }

  /**
   * @brief Frees the flat array. Only 1 single delete[] command.
   * Compared to Separate Chaining: saves O(N) node deletion calls.
   */
  ~DuplicateHashSet() {
    delete[] slots;
    slots = nullptr;
    capacity = 0;
    itemCount = 0;
  }

  // Disable Copy to protect memory ownership.
  DuplicateHashSet(const DuplicateHashSet &other) = delete;
  DuplicateHashSet &operator=(const DuplicateHashSet &other) = delete;

  /**
   * @brief Standard djb2 string hashing algorithm.
   */
  static unsigned long long djb2(const std::string &line) {
    unsigned long long hash = 5381ULL;

    for (uint32_t i = 0; i < line.length(); ++i) {
      hash = ((hash << 5) + hash) + static_cast<unsigned char>(line[i]);
    }

    return hash;
  }

  /**
   * @brief Validates and inserts fingerprint using Robin Hood probing.
   *
   * Algorithm: O(1) average. Average probe distance with Robin Hood is usually
   * < 2 even at 70% load factor. If load factor exceeds 75%, it automatically
   * rehashes to maintain performance.
   *
   * @param fingerprint 64-bit fingerprint to validate.
   * @return true if new fingerprint (successfully inserted).
   * @return false if fingerprint already exists (rejects duplicate line).
   */
  bool insertIfAbsent(unsigned long long fingerprint) {
    // Rehash if load factor > 75%
    if (itemCount * 4 >= capacity * 3) {
      rehash(capacity * 2 + 1);
    }

    uint32_t index = static_cast<uint32_t>(fingerprint % capacity);
    uint32_t dist = 0;

    while (true) {
      if (!slots[index].occupied) {
        // Empty slot → fingerprint doesn't exist → insert
        slots[index].fingerprint = fingerprint;
        slots[index].probeDistance = dist;
        slots[index].occupied = true;
        ++itemCount;
        return true;
      }

      // Fingerprint already exists → reject
      if (slots[index].fingerprint == fingerprint) {
        return false;
      }

      // Robin Hood: if the current probe distance is shorter than ours,
      // we are sure the fingerprint is no longer behind this position
      // (Robin Hood invariant property).
      // However, to be safe with rehash, we still continue probing
      // until we hit an empty slot or match.

      // If current entry is "richer" → steal position (Robin Hood swap)
      if (slots[index].probeDistance < dist) {
        // Swap and continue finding a spot for the pushed out entry
        unsigned long long tmpFp = slots[index].fingerprint;
        uint32_t tmpDist = slots[index].probeDistance;

        slots[index].fingerprint = fingerprint;
        slots[index].probeDistance = dist;

        fingerprint = tmpFp;
        dist = tmpDist;
      }

      index = (index + 1) % capacity;
      ++dist;
    }
  }

  uint32_t size() const { return itemCount; }
  uint32_t getBucketCount() const { return capacity; }
};

#endif
