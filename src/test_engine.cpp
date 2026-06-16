// test_engine.cpp
// Unit Tests cho Halo Cyber Access Engine
// Chạy: biên dịch file này riêng và thực thi. Nếu không crash = PASS.
// Dùng assert() thuần C — không cần framework bên ngoài.

#include <cassert>
#include <cstring>
#include <iostream>

#include "core/DynamicArray.h"
#include "core/DuplicateHashSet.h"
#include "core/HashIndex.h"
#include "core/LogChunk.h"
#include "core/LogEntry.h"
#include "core/StringPool.h"
#include "anomaly/RingBuffer.h"

// ============================================================================
// Helper: In kết quả test
// ============================================================================
static int testsPassed = 0;
static int testsFailed = 0;

void reportResult(const char *testName, bool passed) {
  if (passed) {
    std::cout << "  [PASS] " << testName << '\n';
    ++testsPassed;
  } else {
    std::cout << "  [FAIL] " << testName << '\n';
    ++testsFailed;
  }
}

// ============================================================================
// TEST 1: DynamicArray — Push, Access, Reserve, Clear
// ============================================================================
void testDynamicArray() {
  std::cout << "\n--- DynamicArray Tests ---\n";

  // Test push và access
  DynamicArray<int> arr(4);
  arr.pushBack(10);
  arr.pushBack(20);
  arr.pushBack(30);
  reportResult("push 3 elements, size == 3", arr.size() == 3);
  reportResult("arr[0] == 10", arr[0] == 10);
  reportResult("arr[2] == 30", arr[2] == 30);

  // Test auto-resize (vượt capacity 4)
  arr.pushBack(40);
  arr.pushBack(50);
  reportResult("auto-resize, size == 5", arr.size() == 5);
  reportResult("after resize, arr[4] == 50", arr[4] == 50);

  // Test clear
  arr.clear();
  reportResult("after clear, size == 0", arr.size() == 0);

  // Test reserve
  DynamicArray<int> arr2;
  arr2.reserve(1000);
  reportResult("reserve(1000), capacity >= 1000",
               arr2.getCapacity() >= 1000);
  reportResult("after reserve, size == 0", arr2.size() == 0);
}

// ============================================================================
// TEST 2: DuplicateHashSet — Robin Hood Hashing
// ============================================================================
void testDuplicateHashSet() {
  std::cout << "\n--- DuplicateHashSet Tests ---\n";

  DuplicateHashSet set(16);

  // Test insert new fingerprint
  bool first = set.insertIfAbsent(12345ULL);
  reportResult("insert new fingerprint returns true", first == true);
  reportResult("size == 1 after insert", set.size() == 1);

  // Test insert duplicate
  bool second = set.insertIfAbsent(12345ULL);
  reportResult("insert duplicate returns false", second == false);
  reportResult("size still 1 after dup", set.size() == 1);

  // Test multiple distinct fingerprints
  set.insertIfAbsent(111ULL);
  set.insertIfAbsent(222ULL);
  set.insertIfAbsent(333ULL);
  reportResult("insert 3 more, size == 4", set.size() == 4);

  // Test collision: cùng bucket (modulo 16)
  set.insertIfAbsent(0ULL);
  set.insertIfAbsent(16ULL);  // Cùng bucket với 0
  set.insertIfAbsent(32ULL);  // Cùng bucket với 0 và 16
  reportResult("3 collisions handled, size == 7", set.size() == 7);

  // Verify no false positives
  bool notFound = set.insertIfAbsent(999ULL);
  reportResult("new fingerprint 999 accepted", notFound == true);

  // Test rehash (insert nhiều hơn 75% capacity)
  DuplicateHashSet small(4);
  small.insertIfAbsent(1ULL);
  small.insertIfAbsent(2ULL);
  small.insertIfAbsent(3ULL);
  small.insertIfAbsent(4ULL);  // Vượt 75% → trigger rehash
  small.insertIfAbsent(5ULL);
  reportResult("rehash: 5 items in capacity-4 set, size == 5",
               small.size() == 5);

  // Verify all original items still findable after rehash
  bool dup1 = small.insertIfAbsent(1ULL);
  bool dup3 = small.insertIfAbsent(3ULL);
  reportResult("post-rehash: item 1 still found", dup1 == false);
  reportResult("post-rehash: item 3 still found", dup3 == false);

  // Test djb2 hash function
  unsigned long long h1 = DuplicateHashSet::djb2("hello");
  unsigned long long h2 = DuplicateHashSet::djb2("hello");
  unsigned long long h3 = DuplicateHashSet::djb2("world");
  reportResult("djb2 deterministic: same input same output", h1 == h2);
  reportResult("djb2 distribution: different input different output",
               h1 != h3);
}

// ============================================================================
// TEST 3: StringPool — Dictionary Encoding
// ============================================================================
void testStringPool() {
  std::cout << "\n--- StringPool Tests ---\n";

  StringPool pool(64);

  // Test getOrCreateId (std::string overload)
  uint32_t id1 = pool.getOrCreateId("U001");
  uint32_t id2 = pool.getOrCreateId("U002");
  uint32_t id3 = pool.getOrCreateId("U001");  // Duplicate

  reportResult("first string gets id 0", id1 == 0);
  reportResult("second string gets id 1", id2 == 1);
  reportResult("duplicate returns same id", id3 == id1);
  reportResult("pool size == 2", pool.size() == 2);

  // Test getString (reverse lookup)
  reportResult("getString(0) == 'U001'", pool.getString(0) == "U001");
  reportResult("getString(1) == 'U002'", pool.getString(1) == "U002");
  reportResult("getString(999) == '' (out of bounds)",
               pool.getString(999) == "");

  // Test raw pointer overload
  const char *raw = "DEVICE_XYZ";
  uint32_t rawId =
      pool.getOrCreateId(raw, static_cast<uint32_t>(std::strlen(raw)));
  reportResult("raw overload creates new id", rawId == 2);
  reportResult("raw getString matches",
               pool.getString(rawId) == "DEVICE_XYZ");

  // Test raw pointer finds existing
  uint32_t rawId2 =
      pool.getOrCreateId(raw, static_cast<uint32_t>(std::strlen(raw)));
  reportResult("raw overload finds existing", rawId2 == rawId);
}

// ============================================================================
// TEST 4: LogChunk — Arena Allocation & Timestamp Tracking
// ============================================================================
void testLogChunk() {
  std::cout << "\n--- LogChunk Tests ---\n";

  LogChunk chunk(4);

  reportResult("new chunk has space", chunk.hasSpace() == true);
  reportResult("new chunk size == 0", chunk.size() == 0);

  // Append entries
  LogEntry e1(0, 0, 0, 0, EVENT_LOGIN, LOC_US, 1000);
  LogEntry e2(1, 1, 1, 1, EVENT_LOGOUT, LOC_VN, 2000);
  LogEntry e3(2, 2, 2, 2, EVENT_ACCESS, LOC_JP, 500);

  LogEntry *p1 = chunk.append(e1);
  LogEntry *p2 = chunk.append(e2);
  LogEntry *p3 = chunk.append(e3);

  reportResult("append returns non-null", p1 != nullptr);
  reportResult("size == 3 after 3 appends", chunk.size() == 3);

  // Timestamp tracking
  reportResult("minTimestamp == 500", chunk.getMinTimestamp() == 500);
  reportResult("maxTimestamp == 2000", chunk.getMaxTimestamp() == 2000);

  // Fill to capacity
  LogEntry e4(3, 3, 3, 3, EVENT_DOWNLOAD, LOC_SG, 3000);
  chunk.append(e4);
  reportResult("chunk full, hasSpace == false", chunk.hasSpace() == false);

  // Append on full chunk returns nullptr
  LogEntry e5(4, 4, 4, 4, EVENT_ACCESS, LOC_DE, 4000);
  LogEntry *pNull = chunk.append(e5);
  reportResult("append on full chunk returns nullptr", pNull == nullptr);

  // Pointer stability (addresses within chunk's raw array)
  reportResult("pointer stable: p1 == chunk.raw()",
               p1 == chunk.raw());
  reportResult("pointer stable: p2 == chunk.raw() + 1",
               p2 == chunk.raw() + 1);

  // Test setters (used by BinaryIO)
  chunk.setTimestampRange(100, 9999);
  reportResult("setTimestampRange works", chunk.getMinTimestamp() == 100);
  chunk.setCount(2);
  reportResult("setCount works", chunk.size() == 2);
}

// ============================================================================
// TEST 5: RingBuffer — Sliding Window (Brute Force Detection)
// ============================================================================
void testRingBuffer() {
  std::cout << "\n--- RingBuffer Tests ---\n";

  // Capacity = 5 (simulates BRUTE_FORCE_THRESHOLD)
  RingBuffer<5> buf;

  reportResult("new buffer not full", buf.isFull() == false);

  // Push 4 timestamps — not yet full
  buf.push(100);
  buf.push(200);
  buf.push(300);
  buf.push(400);
  reportResult("4 items, not full", buf.isFull() == false);

  // Push 5th — now full
  buf.push(500);
  reportResult("5 items, full", buf.isFull() == true);

  // All within 500 second window
  reportResult("5 events in 500s window: breached",
               buf.isThresholdBreached(500, 500) == true);

  // Not within 100 second window (oldest=100, current=500 → diff=400)
  reportResult("5 events NOT in 100s window: not breached",
               buf.isThresholdBreached(500, 100) == false);

  // Push more — oldest gets overwritten
  buf.push(900);  // Overwrites 100
  // Now buffer: [200, 300, 400, 500, 900]
  reportResult("after overwrite, still full", buf.isFull() == true);
  reportResult("new oldest=200, current=900, window=700: breached",
               buf.isThresholdBreached(900, 700) == true);
  reportResult("new oldest=200, current=900, window=600: not breached",
               buf.isThresholdBreached(900, 600) == false);

  // Test clear
  buf.clear();
  reportResult("after clear, not full", buf.isFull() == false);
}

// ============================================================================
// TEST 6: TimestampedRingBuffer — countUnique
// ============================================================================
void testTimestampedRingBuffer() {
  std::cout << "\n--- TimestampedRingBuffer Tests ---\n";

  TimestampedRingBuffer<5> buf;

  // Push 5 entries with some duplicate values
  buf.push(100, 1);  // deviceId=1
  buf.push(200, 2);  // deviceId=2
  buf.push(300, 1);  // deviceId=1 (duplicate)
  buf.push(400, 3);  // deviceId=3
  buf.push(500, 2);  // deviceId=2 (duplicate)

  reportResult("buffer full after 5 pushes", buf.isFull() == true);
  reportResult("countUnique == 3 (devices 1,2,3)",
               buf.countUnique() == 3);

  // All within window
  reportResult("all in 400s window: breached",
               buf.isThresholdBreached(500, 400) == true);

  // Push more — oldest overwritten
  buf.push(600, 4);  // Overwrites entry at index 0
  // Now: [200/2, 300/1, 400/3, 500/2, 600/4]
  reportResult("after overwrite, countUnique == 4",
               buf.countUnique() == 4);
}

// ============================================================================
// TEST 7: HashIndex — Insert, Get, SortTimelines
// ============================================================================
void testHashIndex() {
  std::cout << "\n--- HashIndex Tests ---\n";

  HashIndex index(16);

  // Create some LogEntries on stack
  LogEntry entries[5];
  entries[0] = LogEntry(10, 0, 0, 0, EVENT_LOGIN, LOC_US, 300);
  entries[1] = LogEntry(10, 0, 0, 0, EVENT_ACCESS, LOC_VN, 100);
  entries[2] = LogEntry(10, 0, 0, 0, EVENT_LOGOUT, LOC_JP, 200);
  entries[3] = LogEntry(20, 0, 0, 0, EVENT_LOGIN, LOC_US, 400);
  entries[4] = LogEntry(20, 0, 0, 0, EVENT_ACCESS, LOC_SG, 500);

  // Insert by userId
  index.insert(10, &entries[0]);
  index.insert(10, &entries[1]);
  index.insert(10, &entries[2]);
  index.insert(20, &entries[3]);
  index.insert(20, &entries[4]);

  reportResult("2 unique keys", index.size() == 2);

  // Get user 10
  const DynamicArray<const LogEntry *> *user10 = index.get(10);
  reportResult("user 10 found", user10 != nullptr);
  reportResult("user 10 has 3 entries", user10->size() == 3);

  // Get user 20
  const DynamicArray<const LogEntry *> *user20 = index.get(20);
  reportResult("user 20 has 2 entries", user20->size() == 2);

  // Get nonexistent
  reportResult("user 99 not found", index.get(99) == nullptr);

  // Sort timelines
  index.sortAllTimelines();
  reportResult("user 10 sorted: [0].ts=100",
               (*user10)[0]->timestamp == 100);
  reportResult("user 10 sorted: [1].ts=200",
               (*user10)[1]->timestamp == 200);
  reportResult("user 10 sorted: [2].ts=300",
               (*user10)[2]->timestamp == 300);

  // Test reset
  index.reset();
  reportResult("after reset, size == 0", index.size() == 0);
  reportResult("after reset, get(10) == nullptr",
               index.get(10) == nullptr);
}

// ============================================================================
// TEST 8: LogEntry — POD struct size & field access
// ============================================================================
void testLogEntry() {
  std::cout << "\n--- LogEntry Tests ---\n";

  LogEntry e(42, 7, 3, 99, EVENT_DOWNLOAD, LOC_BR, 1714236478LL);

  reportResult("userId == 42", e.userId == 42);
  reportResult("deviceId == 7", e.deviceId == 7);
  reportResult("appId == 3", e.appId == 3);
  reportResult("resourceId == 99", e.resourceId == 99);
  reportResult("eventType == EVENT_DOWNLOAD",
               e.eventType == EVENT_DOWNLOAD);
  reportResult("location == LOC_BR", e.location == LOC_BR);
  reportResult("timestamp == 1714236478",
               e.timestamp == 1714236478LL);

  // Default constructor
  LogEntry def;
  reportResult("default eventType == EVENT_INVALID",
               def.eventType == EVENT_INVALID);
  reportResult("default location == LOC_INVALID",
               def.location == LOC_INVALID);
  reportResult("default timestamp == 0", def.timestamp == 0);
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
  std::cout << "========================================\n";
  std::cout << "  Halo Engine — Unit Test Suite\n";
  std::cout << "========================================\n";

  testDynamicArray();
  testDuplicateHashSet();
  testStringPool();
  testLogChunk();
  testRingBuffer();
  testTimestampedRingBuffer();
  testHashIndex();
  testLogEntry();

  std::cout << "\n========================================\n";
  std::cout << "  Results: " << testsPassed << " passed, " << testsFailed
            << " failed\n";
  std::cout << "========================================\n";

  return testsFailed > 0 ? 1 : 0;
}
