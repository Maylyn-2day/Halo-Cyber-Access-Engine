// LogStore.h
#ifndef LOG_STORE_H
#define LOG_STORE_H

#include <cstdint>

#include "../core/DynamicArray.h"
#include "../core/LogChunk.h"
#include "../core/LogEntry.h"
#include "../core/StringPool.h"

/**
 * @brief Central Storage Engine managing all
 * `LogChunk`s.
 *
 * @note Architecturally, `LogStore` is responsible for scaling out
 * by allocating new `LogChunk`s when the current chunk
 * is full. This design is Block-based Allocation,
 * preventing the risk of copying all old data to a new memory region
 * (reallocation overhead) that regular dynamic arrays (`std::vector`) often
 * encounter.
 */
class LogStore {
private:
  static const uint32_t DEFAULT_CHUNK_SIZE = 8192;

  DynamicArray<LogChunk *> chunks;
  LogChunk *currentChunk;
  uint64_t totalEntries;

  /**
   * @brief Allocates a new Chunk and adds it to the internal management array.
   *
   * @return `true` if allocation is successful, `false` if system memory is exhausted.
   *
   * @note The context switch between Chunks only costs
   * O(1) Amortized Time, occurring every time the system loads `DEFAULT_CHUNK_SIZE`
   * records.
   */
  bool allocateChunk() {
    LogChunk *chunk = new LogChunk(DEFAULT_CHUNK_SIZE);

    chunks.pushBack(chunk);
    currentChunk = chunk;
    return true;
  }

public:
  StringPool stringPool;

  /**
   * @brief Initializes the Storage Engine with default parameters.
   */
  LogStore() : chunks(8), currentChunk(nullptr), totalEntries(0) {}

  /**
   * @brief Initializes the Storage Engine with pre-estimated capacity.
   *
   * @param estimatedChunks Estimated number of required chunks.
   *
   * @note The `chunks` pointer array will allocate enough memory to hold `estimatedChunks`
   * initial pointers, helping optimize the dynamic expansion cost of the inner array
   * data structure.
   */
  explicit LogStore(uint32_t estimatedChunks)
      : chunks(estimatedChunks), currentChunk(nullptr), totalEntries(0) {}

  /**
   * @brief Destroys the storage system and frees the massive amount
   * of data.
   *
   * @note `LogStore` acts as the Lifecycle Manager (Owner) for all
   * `LogChunk` structures. This Destructor has O(M) complexity where M is the number
   * of Chunks. By calling `delete` on each `LogChunk`, a Domino effect will
   * trigger the Chunk's Destructor, thereby automatically cleaning up millions of
   * `LogEntry`s cleanly.
   */
  ~LogStore() {
    for (uint32_t i = 0; i < chunks.size(); ++i) {
      delete chunks[i];
      chunks[i] = nullptr;
    }

    currentChunk = nullptr;
    totalEntries = 0;
  }

  // Disable copying to ensure uniqueness and integrity of
  // Ownership.
  LogStore(const LogStore &other) = delete;
  LogStore &operator=(const LogStore &other) = delete;

  /**
   * @brief Clears all data, frees chunks, resets StringPool.
   * Safer than destructor + placement new.
   */
  void reset() {
    for (uint32_t i = 0; i < chunks.size(); ++i) {
      delete chunks[i];
    }
    chunks.clear();
    currentChunk = nullptr;
    totalEntries = 0;

    // Reset StringPool: destroy old, create new in-place
    stringPool.~StringPool();
    new (&stringPool) StringPool();
    stringPool.reserve(262147);
  }

  /**
   * @brief Pre-reserves memory for the Chunk array index structure.
   *
   * @param estimatedChunks Expected total amount of chunks.
   *
   * @note This action does not immediately allocate memory regions for
   * `LogEntry`s, but only adjusts the capacity of the metadata
   * array `chunks`. This is a standard Pre-allocation technique to
   * avoid Resize Overhead.
   */
  void reserveChunks(uint32_t estimatedChunks) {
    chunks.reserve(estimatedChunks);
  }

  /**
   * @brief Loads a new log object into the storage system.
   *
   * @param entry `LogEntry` object containing raw data.
   * @return Stable pointer to the log's location in
   * the current Chunk.
   *
   * @note Amortized O(1) complexity. If `currentChunk` is full, the system will
   * automatically call `allocateChunk()`. Specifically, because the Engine uses Block-based
   * Allocation, pointers returned from this function have Absolute Pointer Stability.
   * Memory will never be relocated,
   * therefore Indexes (like HashIndex) can safely reference via
   * raw pointers without fearing Invalidated Pointers errors.
   */
  LogEntry *insert(const LogEntry &entry) {
    if (currentChunk == nullptr || !currentChunk->hasSpace()) {
      if (!allocateChunk()) {
        return nullptr;
      }
    }

    LogEntry *stored = currentChunk->append(entry);

    if (stored != nullptr) {
      ++totalEntries;
    }

    return stored;
  }

  /**
   * @brief Returns the total number of log records stored by the system.
   */
  uint64_t size() const { return totalEntries; }

  /**
   * @brief Returns the total number of Blocks (Chunks) currently allocated on
   * the Heap.
   */
  uint32_t chunkCount() const { return chunks.size(); }

  /**
   * @brief Directly accesses a specific data block (Chunk) via
   * index.
   *
   * @note This design serves Parallelization architecture (Multithreading/SIMD).
   * Analysis threads (Worker Threads) can get each Chunk independently via this
   * function for map-reduce processing without causing Lock Contention.
   */
  LogChunk *getChunk(uint32_t index) { return chunks[index]; }

  /**
   * @brief Read-only access to a Chunk via index.
   */
  const LogChunk *getChunk(uint32_t index) const { return chunks[index]; }

  /**
   * @brief Allows BinaryIO to inject a pre-populated chunk when loading binary.
   * @param chunk Pointer to the LogChunk that has been fread'd with data.
   * @param entryCount Actual number of entries in the chunk.
   */
  void addLoadedChunk(LogChunk *chunk, uint32_t entryCount) {
    chunks.pushBack(chunk);
    currentChunk = chunk;
    totalEntries += entryCount;
  }
};

#endif
