// LogChunk.h
#ifndef LOG_CHUNK_H
#define LOG_CHUNK_H

#include <climits>

#include "LogEntry.h"

/**
 * @brief Manages a Contiguous Memory Block exclusively
 * for `LogEntry` objects.
 *
 * @note This is the core component of the Arena Allocation mechanism. Instead of allocating
 * each `LogEntry` object with discrete `new` operators, the Engine allocates a
 * large "Chunk" containing thousands of elements. This architecture completely resolves
 * Heap Fragmentation and maximizes
 * data locality (Cache Locality) during sequential scanning (Table Scan)
 * and sorting.
 */
class LogChunk {
private:
  LogEntry *entries;
  uint32_t capacity;
  uint32_t count;

  /**
   * @brief Minimum and maximum timestamps within the Chunk. Updated O(1)
   * after each append().
   *
   * Architecture decision: These two metadata fields allow QueryEngine to skip
   * the entire Chunk using only 2 integer comparisons when querying
   * a time-range that does not overlap with the Chunk. On 10M rows / 8192 entries-per-chunk,
   * this can cut down 90%+ of Chunks needing to be scanned, improving speed from
   * O(N) down to O(overlapping_entries).
   */
  int64_t minTimestamp;
  int64_t maxTimestamp;

public:
  /**
   * @brief Initializes a new Chunk with the specified capacity.
   *
   * @param chunkCapacity Maximum number of `LogEntry` objects this Chunk can
   * hold.
   *
   * @note Directly calls `new[]` operator to allocate an entire contiguous
   * array of `LogEntry` in O(1) (execution time may vary slightly depending on
   * OS). Since `LogEntry` has an empty Constructor, initialization cost will be
   * at the minimum.
   */
  explicit LogChunk(uint32_t chunkCapacity)
      : entries(nullptr), capacity(chunkCapacity), count(0),
        minTimestamp(INT64_MAX), maxTimestamp(INT64_MIN) {
    if (capacity > 0) {
      entries = new LogEntry[capacity];
    }
  }

  /**
   * @brief Destroys the Chunk and frees the entire contiguous memory block.
   *
   * @note Calling `delete[] entries` ensures memory is returned to the OS.
   * Concentrating the deallocation of thousands of objects at once significantly
   * reduces Heap management overhead compared to calling `delete` thousands of
   * times.
   */
  ~LogChunk() {
    delete[] entries;
    entries = nullptr;
    capacity = 0;
    count = 0;
  }

  /**
   * @brief Disables Copying for raw memory structures.
   *
   * @warning Since this class exclusively owns Heap memory through a raw pointer,
   * allowing the use of Copy Constructor or default assignment operator will
   * lead to Double-Free Bugs or Heap corruption.
   */
  LogChunk(const LogChunk &other) = delete;
  LogChunk &operator=(const LogChunk &other) = delete;

  /**
   * @brief Checks if the current Chunk still has capacity.
   *
   * @note Inline function with O(1) call cost.
   */
  bool hasSpace() const { return count < capacity; }

  /**
   * @brief Appends a `LogEntry` object to the next empty position
   * in the Chunk.
   *
   * @param entry Input log record (data will be copied into the Chunk).
   * @return Pointer to the actual storage location of the record inside the Chunk,
   * or `nullptr` if the Chunk is full.
   *
   * @note O(1) complexity. This is an ultra-fast Placement/Copy operation. The returned
   * pointer is a stable memory address, which will be used to build the Index structure
   * without being invalidated.
   */
  LogEntry *append(const LogEntry &entry) {
    if (!hasSpace()) {
      return nullptr;
    }

    entries[count] = entry;
    LogEntry *stored = &entries[count];
    ++count;

    if (entry.timestamp < minTimestamp) minTimestamp = entry.timestamp;
    if (entry.timestamp > maxTimestamp) maxTimestamp = entry.timestamp;

    return stored;
  }

  /**
   * @brief Accesses the Raw Array for Sequential Scan.
   * @return Pointer to the first element of the `LogEntry` array.
   */
  LogEntry *raw() { return entries; }

  /**
   * @brief Accesses the Raw Array (Read-only).
   */
  const LogEntry *raw() const { return entries; }

  /**
   * @brief Gets the current number of records in the Chunk.
   */
  uint32_t size() const { return count; }

  /**
   * @brief Gets the maximum capacity of the Chunk.
   */
  uint32_t getCapacity() const { return capacity; }

  /**
   * @brief Returns the minimum timestamp in the Chunk (O(1)).
   * Used by QueryEngine to skip Chunks not in the time-range.
   */
  int64_t getMinTimestamp() const { return minTimestamp; }

  /**
   * @brief Returns the maximum timestamp in the Chunk (O(1)).
   * Used by QueryEngine to skip Chunks not in the time-range.
   */
  int64_t getMaxTimestamp() const { return maxTimestamp; }

  /**
   * @brief Allows BinaryIO to restore timestamp metadata when loading binary.
   */
  void setTimestampRange(int64_t min, int64_t max) {
    minTimestamp = min;
    maxTimestamp = max;
  }

  /**
   * @brief Allows BinaryIO to set count after fread-ing the entire entries block.
   */
  void setCount(uint32_t c) { count = c; }
};
#endif
