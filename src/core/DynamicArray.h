// DynamicArray.h
#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <cstdint>

/**
 * @brief Custom Dynamic Array structure optimized for high performance
 * in In-Memory Analytics.
 *
 * This dynamic array is designed around the "Zero STL" philosophy, completely
 * eliminating redundant metadata and semantic overhead of std::vector. By directly
 * managing a contiguous memory block via raw pointers, this structure maximizes
 * spatial locality, increasing L1/L2/L3 Cache Hit ratios when the CPU performs
 * sequential scans across millions of log lines.
 *
 * @tparam T Data type of the elements.
 *
 * @warning Memory Ownership: This structure manages its own heap memory.
 * However, if `T` is a pointer (e.g., `char*`), the caller is responsible for
 * iterating through the array and calling delete on the pointed objects before
 * this structure is destroyed to completely prevent Memory Leaks.
 */
template <typename T> class DynamicArray {
private:
  T *data;
  uint32_t length;
  uint32_t capacity;

  /**
   * @brief Automatically reallocates memory when the array reaches its capacity.
   *
   * Algorithm decision: This allocation strategy has an O(N) time cost
   * as it requires moving a large volume of data to a new contiguous memory region.
   * Directly copying using the assignment operator instead of std::move is a
   * deliberate trade-off for POD/Trivial data structures, aiming to keep the
   * memory control logic as primitive and fast as possible.
   *
   * @param newCapacity New memory capacity to be requested from the OS.
   */
  void resize(uint32_t newCapacity) {
    T *newData = new T[newCapacity];

    for (uint32_t i = 0; i < length; ++i) {
      newData[i] = data[i];
    }

    delete[] data;
    data = newData;
    capacity = newCapacity;
  }

public:
  /**
   * @brief Lazy Initialization of memory allocation until the first
   * element is inserted.
   *
   * Architecture decision: This technique saves a significant amount of wasted RAM
   * if the system has to pre-initialize thousands of empty dynamic arrays as
   * buckets in the Hash Table partition. Initialization time is strictly O(1).
   */
  DynamicArray() : data(nullptr), length(0), capacity(0) {}

  /**
   * @brief Pre-allocates a contiguous memory block to avoid
   * fragmentation and bottlenecks.
   *
   * Architecture decision: Ingesting millions of CSV rows into RAM requires
   * completely eliminating expensive, intermittent dynamic allocation syscalls.
   * By setting a large initialCapacity from the beginning, the data ingestion speed
   * achieves pure linear O(1), completely preventing O(N) resize costs.
   *
   * @param initialCapacity Initial memory size (in terms of `T` elements)
   * to be pre-allocated.
   */
  explicit DynamicArray(uint32_t initialCapacity)
      : data(nullptr), length(0), capacity(0) {
    reserve(initialCapacity);
  }

  /**
   * @brief Ensures clean heap cleanup following the RAII (Resource
   * Acquisition Is Initialization) principle.
   *
   * Architecture decision: Memory cleanup time is O(1) for POD data sequences.
   * The `delete[]` command implicitly invokes the destructor (if any) for each
   * element. Manual memory management mentality using raw pointers eliminates
   * the access and Reference Counting overhead from thread-safety imposed by
   * smart pointers (e.g., std::shared_ptr).
   */
  ~DynamicArray() {
    delete[] data;
    data = nullptr;
    length = 0;
    capacity = 0;
  }

  /**
   * @brief Disables automatic Deep Copy mechanism to enforce a Unique Ownership policy.
   *
   * Architecture decision: Copying a massive array block accidentally leads to
   * severe performance degradation (O(N) memory & CPU overhead). More importantly,
   * disabling copy logic helps isolate raw pointers, completely protecting the system
   * against Double-Free Crash risks during cross-memory manipulations.
   *
   * @param other Original referenced DynamicArray object.
   */
  DynamicArray(const DynamicArray &other) = delete;

  /**
   * @brief Disables Assignment Operator to ensure structural memory ownership integrity.
   *
   * @param other Original referenced DynamicArray object.
   * @return Deleted.
   */
  DynamicArray &operator=(const DynamicArray &other) = delete;

  /**
   * @brief Move Constructor. Safely transfers memory ownership.
   *
   * Architecture decision: Allows Anomaly analyzers to return a result array
   * (Return by value) without performance drops. The structure steals the pointer
   * from the R-value and resets the old object to an empty state.
   *
   * @param other Referenced R-value object.
   */
  DynamicArray(DynamicArray &&other) noexcept
      : data(other.data), length(other.length), capacity(other.capacity) {
    other.data = nullptr;
    other.length = 0;
    other.capacity = 0;
  }

  /**
   * @brief Move Assignment operator. Frees old memory and takes over new memory.
   *
   * @param other Referenced R-value object.
   * @return Reference to the current object.
   */
  DynamicArray &operator=(DynamicArray &&other) noexcept {
    if (this != &other) {
      delete[] data;
      data = other.data;
      length = other.length;
      capacity = other.capacity;

      other.data = nullptr;
      other.length = 0;
      other.capacity = 0;
    }
    return *this;
  }

  /**
   * @brief Manually intervenes in the memory allocation mechanism (Manual Capacity Allocation).
   *
   * Algorithm: If requested capacity is not greater than the current capacity,
   * the function exits immediately in O(1). Otherwise, the system triggers the
   * Reallocation mechanism which takes O(N) copy time. This operation is crucial
   * for hot-path processing threads to eliminate Memory Fragmentation risks.
   *
   * @param requestedCapacity Maximum memory capacity requested to be pre-allocated.
   */
  void reserve(uint32_t requestedCapacity) {
    if (requestedCapacity <= capacity) {
      return;
    }

    T *newData = new T[requestedCapacity];

    for (uint32_t i = 0; i < length; ++i) {
      newData[i] = data[i];
    }

    delete[] data;
    data = newData;
    capacity = requestedCapacity;
  }

  /**
   * @brief Sequentially inserts data into the buffer using an Exponential Growth strategy.
   *
   * Algorithm: Amortized Time Complexity is O(1). In case of peak data load
   * (length == capacity), the O(N) internal data relocation with Growth Factor = 2
   * automatically triggers. The initial Capacity level is hardcoded to 8 elements
   * to minimize wasted space for extremely small sparse datasets.
   *
   * @param value Data/object to be appended to the memory block.
   */
  void pushBack(const T &value) {
    if (length == capacity) {
      uint32_t nextCapacity = capacity == 0 ? 8 : capacity * 2;
      resize(nextCapacity);
    }

    data[length] = value;
    ++length;
  }

  /**
   * @brief Random Access interface directly interacting with raw memory space.
   *
   * Architecture decision: Completely eliminates safety bounds-checking if-else
   * statements to avoid breaking the CPU instruction execution pipeline
   * (branch prediction miss). Index access maps directly to the standard C-language
   * O(1) memory offset equation: `base_address + index * sizeof(T)`.
   *
   * @warning Thread Warning: The Caller is fully responsible for ensuring
   * `index < length`. The behavior is Undefined (Undefined Behavior) if
   * accessing outside the allocated memory boundary (causes Segfault).
   *
   * @param index Offset position to be accessed.
   * @return T& Direct pointer reference to the actual data slot on RAM.
   */
  T &operator[](uint32_t index) { return data[index]; }

  /**
   * @brief Read-Only Random Access interface ensuring absolute minimum O(1) latency.
   *
   * @param index Offset position to be read.
   * @return const T& Constant reference (const) preventing modification of original RAM data.
   */
  const T &operator[](uint32_t index) const { return data[index]; }

  /**
   * @brief Instantaneously queries the actual data resource volume occupying the buffer.
   *
   * @return uint32_t Current Length of the indexed dataset (O(1) complexity).
   */
  uint32_t size() const { return length; }

  /**
   * @brief Evaluates the maximum scale of successfully borrowed physical RAM block
   * (in storage slots).
   *
   * @return uint32_t Maximum capacity without overflow before reallocation (O(1) complexity).
   */
  uint32_t getCapacity() const { return capacity; }

  /**
   * @brief Exposes the original heap stream Base Pointer for low-level C-Library operations.
   *
   * Architecture decision: Vital gateway for Low-level & Zero STL projects.
   * This feature allows passing the entire raw memory buffer straight into system
   * APIs like `write()`, `send()` (Socket I/O) or `memcpy()` without going through
   * any intermediary encoding layer.
   *
   * @return T* Raw pointer holding the start of the contiguous memory at index 0.
   */
  T *raw() { return data; }

  /**
   * @brief Extracts the original pointer in anti-overwrite form for purely
   * read-only data streaming purposes.
   *
   * @return const T* Raw pointer ensuring read-buffer integrity.
   */
  const T *raw() const { return data; }

  /**
   * @brief Rapid memory reuse mechanism completely eliminating Kernel interaction
   * (Memory Reuse).
   *
   * Architecture decision: By simply overwriting the internal `length = 0`
   * variable (Absolute O(1) runtime), the entire existing array block is ready
   * to record for a new Batch Processing session. This is the technical core behind
   * Object Pooling and Arena Allocation design patterns, helping to prevent
   * continuous `delete[]` and `new[]` commands from eroding OS allocation performance.
   */
  void clear() { length = 0; }
};

#endif
