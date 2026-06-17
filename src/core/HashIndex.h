// HashIndex.h
#ifndef HASH_INDEX_H
#define HASH_INDEX_H

#include <cstdint>

#include "DynamicArray.h"
#include "LogEntry.h"

/**
 * @brief Specialized Hash Table data structure acting as the Index Server for
 * the system.
 *
 * @note Instead of using `std::unordered_map` which has high latency due to
 * small node allocations (overhead), `HashIndex` is optimized for a Zero STL
 * environment. The hash table uses Separate Chaining with an array of raw
 * pointers and manual allocation. The key is `uint32_t` (Dictionary ID) and the
 * value is a `DynamicArray` containing pointers to the original `LogEntry`
 * objects (Zero-Copy), ensuring an average lookup time of O(1) and minimizing
 * memory fragmentation.
 */
class HashIndex {
private:
  /**
   * @brief Basic Node constituting the Singly Linked List in each Bucket.
   *
   * @note Grouping all LogEntries with the same Key into a `DynamicArray`
   * enhances Spatial Locality for the CPU Cache when iterating over an object's
   * timeline. This is significantly more efficient than creating a separate Node
   * for each log line.
   */
  struct Node {
    uint32_t key;
    DynamicArray<const LogEntry *> entries;
    Node *next;

    /**
     * @brief Initializes a new Node in the Collision Chain.
     *
     * @param nodeKey 32-bit identifier key (user's or device's Dictionary ID,
     * etc.).
     *
     * @note The `explicit` keyword prevents logic errors caused by automatic
     * compiler type casting (implicit conversion).
     */
    explicit Node(uint32_t nodeKey);
  };

  Node **buckets;
  uint32_t bucketCount;
  uint32_t keyCount;

  /**
   * @brief Internal Hash Function to distribute keys.
   *
   * @param key Input 32-bit identifier key.
   * @return Corresponding bucket index (from 0 to bucketCount - 1).
   *
   * @note Uses a primitive integer hashing algorithm (like MurmurHash3 or
   * simple modulo) with extremely low O(1) computational cost.
   */
  uint32_t hashKey(uint32_t key) const;

public:
  /**
   * @brief Initializes the hash table structure with a predefined capacity.
   *
   * @param bucketSize Initial number of buckets (Should be a prime number to
   * minimize collisions).
   *
   * @note Time complexity O(N) where N is `bucketSize`. Allocates an array of
   * raw pointers `new Node*[bucketCount]` and initializes it to empty to avoid
   * memory garbage. Predefining the size completely disables the highly
   * expensive runtime Rehashing process.
   */
  explicit HashIndex(uint32_t bucketSize);

  /**
   * @brief Destructor to clean up all resources.
   *
   * @note Time complexity O(B + N) where B is the number of buckets and N is
   * the number of keys.
   * @warning This function only destroys the Index structure (frees the
   * `buckets` array and the `Node`s). It DOES NOT free the original `LogEntry`
   * objects in order to comply with the Ownership principle.
   */
  ~HashIndex();

  // Disable Copy Constructor and Assignment Operator to prevent
  // Double-Free errors due to raw pointer management.
  HashIndex(const HashIndex &other) = delete;
  HashIndex &operator=(const HashIndex &other) = delete;

  /**
   * @brief Inserts a `LogEntry` reference (pointer) into the index.
   *
   * @param key 32-bit identifier key used as the clustering criteria.
   * @param entry Constant pointer (const pointer) pointing to the original log
   * allocated in the Arena/Chunk.
   *
   * @note Average time complexity O(1), worst case O(K) where K is the number
   * of colliding elements in the bucket. If the key already exists, the new
   * pointer will be appended to the `DynamicArray` in Amortized O(1).
   */
  void insert(uint32_t key, const LogEntry *entry);

  /**
   * @brief Retrieves the list of all events corresponding to a Key.
   *
   * @param key 32-bit identifier key.
   * @return A constant pointer pointing to the dynamic array containing the
   * list of `LogEntry*`, or `nullptr` if not found.
   *
   * @note Average time complexity O(1). The function signature strictly uses
   * `const` to ensure the immutability of the Index against query threads.
   */
  const DynamicArray<const LogEntry *> *get(uint32_t key) const;

  /**
   * @brief Sorts all log data of all keys chronologically.
   *
   * @note System complexity O(N * K log K) where N is the number of keys and K
   * is the average array size. Called after completing the Bulk Loading phase
   * to prepare for behavioral sequence analysis or Binary Search.
   */
  void sortAllTimelines();

  /**
   * @brief Returns the total number of unique keys that have been indexed.
   */
  uint32_t size() const;

  /**
   * @brief Clears all Index data without destroying the structure.
   * Frees all Nodes and resets buckets to an empty state.
   * Safer than using destructor + placement new.
   */
  void reset();

  uint32_t getBucketCount() const;
};

#endif
