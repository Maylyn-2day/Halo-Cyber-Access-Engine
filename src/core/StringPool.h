#ifndef STRING_POOL_H
#define STRING_POOL_H

#include <cstdint>
#include <cstring>
#include <string>

#include "DynamicArray.h"

/**
 * @brief Dictionary Encoding / String Pool structure system to
 * optimize RAM intensity.
 *
 * Architecture decision: Uses Open-Addressing with Linear Probing instead of
 * Separate Chaining. Each slot stores (hash, id, occupied) directly inline in a
 * flat array, completely eliminating `new Node` cost for each string. Hash is
 * cached in the slot to avoid recalculating during rehash or comparison.
 *
 * The `strings` array (DynamicArray) retains its O(1) reverse-lookup role.
 */
class StringPool {
private:
  /**
   * @brief Inline slot in the Open-Addressing flat array.
   * Stores cached hash + dictionary ID. Size: 16 bytes.
   */
  struct Slot {
    unsigned long long cachedHash; ///< Pre-calculated hash (avoid string rehash).
    uint32_t id;                   ///< Dictionary ID pointing to strings[] array.
    bool occupied;                 ///< Flag marking valid slot.
    uint8_t _pad[3];               ///< Padding alignment.

    Slot() : cachedHash(0), id(0), occupied(false) {
      _pad[0] = _pad[1] = _pad[2] = 0;
    }
  };

  Slot *slots;                       ///< Open-Addressing flat array.
  uint32_t capacity;                 ///< slots array size.
  uint32_t keyCount;                 ///< Number of saved unique strings.
  DynamicArray<std::string> strings; ///< Reverse-lookup array: ID -> original string.

  /**
   * @brief Internal djb2 string hash function.
   */
  unsigned long long hashString(const std::string &str) const;

  /**
   * @brief Zero-allocation Raw Hash function for hot-path.
   */
  unsigned long long hashRaw(const char *data, uint32_t length) const;

  /**
   * @brief Rehash when load factor exceeds 75%.
   * Iterates through the old array, inserts occupied slots into the new array.
   */
  void rehash(uint32_t newCapacity);

public:
  explicit StringPool(uint32_t bucketSize = 262147);
  ~StringPool();

  // Disable Copy/Assignment
  StringPool(const StringPool &other) = delete;
  StringPool &operator=(const StringPool &other) = delete;

  void reserve(uint32_t capacity);

  /**
   * @brief Dictionary Encoding: look up or register ID for a std::string.
   */
  uint32_t getOrCreateId(const std::string &str);

  /**
   * @brief Zero-allocation overload: look up/register ID from raw pointer.
   */
  uint32_t getOrCreateId(const char *data, uint32_t length);

  /**
   * @brief Reverse Lookup: ID -> original string. O(1).
   */
  std::string getString(uint32_t id) const;

  uint32_t size() const;
  uint32_t getBucketCount() const;
};

#endif
