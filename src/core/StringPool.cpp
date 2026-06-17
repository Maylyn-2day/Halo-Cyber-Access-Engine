#include "StringPool.h"

// ============================================================================
// Hash Functions
// ============================================================================

unsigned long long StringPool::hashString(const std::string &str) const {
  unsigned long long hash = 5381ULL;

  for (uint32_t i = 0; i < str.length(); ++i) {
    hash = ((hash << 5) + hash) + static_cast<unsigned char>(str[i]);
  }

  return hash;
}

unsigned long long StringPool::hashRaw(const char *data,
                                       uint32_t length) const {
  unsigned long long hash = 5381ULL;

  for (uint32_t i = 0; i < length; ++i) {
    hash = ((hash << 5) + hash) + static_cast<unsigned char>(data[i]);
  }

  return hash;
}

// ============================================================================
// Lifecycle
// ============================================================================

StringPool::StringPool(uint32_t bucketSize)
    : slots(nullptr), capacity(bucketSize), keyCount(0), strings(8) {
  if (capacity == 0) {
    capacity = 1;
  }

  slots = new Slot[capacity]();
}

StringPool::~StringPool() {
  delete[] slots;
  slots = nullptr;
  capacity = 0;
  keyCount = 0;
}

void StringPool::reserve(uint32_t cap) { strings.reserve(cap); }

// ============================================================================
// Rehash (Open-Addressing)
// ============================================================================

void StringPool::rehash(uint32_t newCapacity) {
  Slot *oldSlots = slots;
  uint32_t oldCapacity = capacity;

  slots = new Slot[newCapacity]();
  capacity = newCapacity;

  // Re-insert all old slots. keyCount remains unchanged.
  for (uint32_t i = 0; i < oldCapacity; ++i) {
    if (oldSlots[i].occupied) {
      unsigned long long h = oldSlots[i].cachedHash;
      uint32_t idx = static_cast<uint32_t>(h % capacity);

      // Linear probing to find empty slot in the new array
      while (slots[idx].occupied) {
        idx = (idx + 1) % capacity;
      }

      slots[idx].cachedHash = h;
      slots[idx].id = oldSlots[i].id;
      slots[idx].occupied = true;
    }
  }

  delete[] oldSlots;
}

// ============================================================================
// getOrCreateId (std::string overload)
// ============================================================================

uint32_t StringPool::getOrCreateId(const std::string &str) {
  // Rehash if load factor > 75%
  if (keyCount * 4 >= capacity * 3) {
    rehash(capacity * 2 + 1);
  }

  unsigned long long hash = hashString(str);
  uint32_t index = static_cast<uint32_t>(hash % capacity);

  // Linear probing: find match or empty slot
  while (slots[index].occupied) {
    if (slots[index].cachedHash == hash && strings[slots[index].id] == str) {
      return slots[index].id; // Cache hit
    }
    index = (index + 1) % capacity;
  }

  // Cache miss: assign new ID
  uint32_t newId = strings.size();
  strings.pushBack(str);

  slots[index].cachedHash = hash;
  slots[index].id = newId;
  slots[index].occupied = true;
  ++keyCount;

  return newId;
}

// ============================================================================
// getOrCreateId (raw pointer overload — zero-allocation hot path)
// ============================================================================

uint32_t StringPool::getOrCreateId(const char *data, uint32_t length) {
  // Rehash if load factor > 75%
  if (keyCount * 4 >= capacity * 3) {
    rehash(capacity * 2 + 1);
  }

  unsigned long long hash = hashRaw(data, length);
  uint32_t index = static_cast<uint32_t>(hash % capacity);

  // Linear probing: compare hash first (O(1)), then memcmp if hash matches
  while (slots[index].occupied) {
    if (slots[index].cachedHash == hash) {
      const std::string &stored = strings[slots[index].id];
      if (stored.length() == length &&
          std::memcmp(stored.c_str(), data, length) == 0) {
        return slots[index].id; // Cache hit
      }
    }
    index = (index + 1) % capacity;
  }

  // Cache miss: only create canonical std::string here
  uint32_t newId = strings.size();
  strings.pushBack(std::string(data, length));

  slots[index].cachedHash = hash;
  slots[index].id = newId;
  slots[index].occupied = true;
  ++keyCount;

  return newId;
}

// ============================================================================
// Accessors
// ============================================================================

std::string StringPool::getString(uint32_t id) const {
  if (id >= strings.size()) {
    return "";
  }

  return strings[id];
}

uint32_t StringPool::size() const { return keyCount; }

uint32_t StringPool::getBucketCount() const { return capacity; }