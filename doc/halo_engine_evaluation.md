# Halo Engine — Phase 1 Midterm Evaluation

> **Evaluator Role:** Senior C++ Systems Architect / University IT Professor  
> **Date:** 2026-05-26  
> **Verdict:** Solid Phase 1 foundation with clear engineering intent. Several non-trivial risks require attention before the 10M-row stress test.

---

## File Inventory

| File | Type | Lines | Role |
|------|------|------:|------|
| [main.cpp](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/main.cpp) | Source | 243 | CLI entry point, timing harness |
| [LogEntry.h](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/LogEntry.h) | Header | 90 | Ultra-compact log row struct |
| [LogChunk.h](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/LogChunk.h) | Header | 90 | Arena-style contiguous block |
| [LogStore.h](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/LogStore.h) | Header | 118 | Chunk-based log ownership layer |
| [DynamicArray.h](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/DynamicArray.h) | Header | 129 | Custom `std::vector` replacement |
| [DuplicateHashSet.h](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/DuplicateHashSet.h) | Header | 129 | Fingerprint-based duplicate filter |
| [StringPool.h](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/StringPool.h) | Header | 157 | Dictionary encoder (string ↔ uint32_t) |
| [HashIndex.h](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/HashIndex.h) | Header | 167 | Inverted index (key → timeline) |
| [SearchEngine.h](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/SearchEngine.h) | Header | 71 | Index builder & query facade |
| [QueryEngine.h](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/QueryEngine.h) | Header | 209 | Business queries (journeys, Top-10) |
| [SortUtils.h](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/SortUtils.h) | Header | 117 | Stable Merge Sort on pointer arrays |
| [DataLoader.h](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/DataLoader.h) | Header | 26 | Loader declaration |
| [DataLoader.cpp](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/DataLoader.cpp) | Source | 400 | High-speed CSV ingestion |

**Total:** ~1,946 lines of handwritten C++ across 13 files. Zero STL containers used (as required).

---

## 1. Requirements Verification (Midterm Phase 1)

| # | Requirement | Status | Evidence |
|---|-------------|--------|----------|
| 1 | Parse CSV with raw `char*` / zero-copy approach | ✅ **PASS** | `DataLoader.cpp` uses `FILE* + fread` into a 256 KB `readBuffer`, splits lines with raw pointer arithmetic via `FieldView` — no `std::getline`, no `std::stringstream`. |
| 2 | Filter duplicates with custom Hash Set | ✅ **PASS** | `DuplicateHashSet` stores 64-bit `djb2` fingerprints in separate-chaining buckets. `insertIfAbsent()` returns false for duplicates, skipping them before any parsing work. |
| 3 | Dictionary-encode strings (`StringPool`) | ✅ **PASS** | `StringPool` maps `std::string → uint32_t` (forward) and `uint32_t → std::string` (reverse via `DynamicArray<std::string>`). Correct bidirectional encoding. |
| 4 | Index relationships (`HashIndex`) | ✅ **PASS** | `SearchEngine` builds two `HashIndex` instances (user, resource). Each maps a `uint32_t` dictionary ID to a `DynamicArray<const LogEntry*>` timeline. |
| 5 | Stable Merge Sort (no leaks) | ✅ **PASS** | `SortUtils::sortByTimestamp` allocates temp buffer once, runs recursive stable merge sort, then `delete[]` the buffer. Stable tie-breaking via `<=` on left half. |
| 6 | User Journey query | ✅ **PASS** | `QueryEngine::printUserJourney` — linear scan with early `break` on sorted timeline. |
| 7 | Resource Access History query | ✅ **PASS** | `QueryEngine::printResourceJourney` — same pattern. |
| 8 | Top 10 Resources in O(N) | ✅ **PASS** | Bucket-counting with `uint32_t* counts = new uint32_t[resourceCapacity]()`, then insertion into a fixed `TopResource[10]` array. |
| 9 | Interactive CLI with safe input | ✅ **PASS** | `main.cpp` uses `clearBadInput()` (`cin.clear() + cin.ignore`), try/catch on `std::stoll`, default values, and range swapping. |

> [!IMPORTANT]
> **All 9 midterm requirements are met.** The implementation is functionally complete for Phase 1.

---

## 2. Architecture & Design Evaluation

### 2.1 Strengths (Pros)

#### ✅ Arena Allocation via `LogChunk`

```
LogStore → DynamicArray<LogChunk*> → LogChunk(8192 entries) → contiguous LogEntry[]
```

This is textbook arena-style allocation. Instead of 1M individual `new LogEntry`, you get ~122 `new LogEntry[8192]` calls. Benefits:
- **Heap fragmentation:** Reduced by ~8000×.
- **Cache locality:** Sequential scans (sorting, Top-10) hit L1/L2 cache lines naturally because entries are physically adjacent.
- **Deallocation:** One `delete[]` per chunk instead of 1M individual frees.

**Grade: A.** This is production-quality thinking for a coursework project.

---

#### ✅ Dictionary Encoding (`StringPool`)

Every string field (`userId`, `deviceId`, `appId`, `resourceId`) is encoded to a compact `uint32_t` at ingestion time. `LogEntry` stores **zero** `std::string` objects.

| Field Layout | Bytes |
|---|---:|
| `int64_t timestamp` | 8 |
| `uint32_t userId` | 4 |
| `uint32_t deviceId` | 4 |
| `uint32_t appId` | 4 |
| `uint32_t resourceId` | 4 |
| `EventType` (uint8_t) | 1 |
| `Location` (uint8_t) | 1 |
| **Padding** | 2 |
| **Total** | **28** (with typical alignment: **32**) |

At 10M rows × 32 bytes = **~305 MB** of pure event data. Without dictionary encoding, each row would carry 4 × `std::string` objects (each 32 bytes on MSVC with SSO), ballooning the footprint to **>1.5 GB**.

**Grade: A.** Excellent memory efficiency decision.

---

#### ✅ O(1) Amortized Hash Lookups

Both `StringPool` and `HashIndex` use separate-chaining hash tables with prime-sized bucket counts (`262147`). The `HashIndex` also uses a high-quality integer mixing hash (Murmur-inspired finalization). Amortized lookup is O(1) with a good load factor.

**Grade: A.**

---

#### ✅ Ownership Discipline

Every RAII class (`LogChunk`, `LogStore`, `DynamicArray`, `DuplicateHashSet`, `StringPool`, `HashIndex`) correctly:
- Deletes owned memory in its destructor.
- Marks copy constructor and assignment operator as `= delete`.
- Sets dangling pointers to `nullptr` after `delete`.

**Grade: A.** You clearly understand the Rule of Three.

---

#### ✅ CSV Parser Design

The `DataLoader.cpp` parser is impressively engineered for a coursework project:
- Block I/O via `fread` (256 KB chunks) instead of line-by-line `fgets`.
- Zero-copy field splitting via `FieldView{const char*, uint32_t}`.
- Custom `parseInt64` with overflow detection (`INT64_MAX` check).
- Duplicate fingerprinting *before* parsing — avoiding wasted CPU on duplicate rows.
- Header detection via `looksLikeHeader`.

**Grade: A.**

---

### 2.2 Weaknesses (Cons / Risks)

#### ⚠️ CON-1: Mostly Header-Only Architecture — ODR & Compile-Time Risk

11 out of 13 files are `.h` files containing full class definitions and method implementations. Only `DataLoader` has a `.cpp` file.

**Problems:**
1. **One Definition Rule (ODR) risk:** If any `.h` is included from two `.cpp` files in the future, you'll get duplicate symbol linker errors for non-template, non-inline functions.
2. **Compilation time:** Every change to `LogEntry.h` forces recompilation of everything. With 13 files this is negligible; at scale with 50+ files, it becomes painful.
3. **Not a problem *today*:** You only have one `.cpp` translation unit (`main.cpp` + `DataLoader.cpp`), so ODR is not violated. But it's a scalability liability for Phase 2.

> [!WARNING]
> **Fix priority: MEDIUM.** For Phase 2 (which adds many more feature modules), extract implementations of `HashIndex`, `StringPool`, `QueryEngine`, and `SearchEngine` into `.cpp` files. Keep templates (`DynamicArray<T>`) in headers.

---

#### ⚠️ CON-2: `LogEntry` Initializer List Order Mismatch

In [LogEntry.h:63-70](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/LogEntry.h#L63-L70), the default constructor's initializer list is:

```cpp
LogEntry()
    : userId(0),       // member declared 2nd
      deviceId(0),     // member declared 3rd
      appId(0),        // member declared 4th
      resourceId(0),   // member declared 5th
      timestamp(0),    // member declared 1st ← OUT OF ORDER
      eventType(EVENT_INVALID),
      location(LOC_INVALID) {}
```

Members are initialized in **declaration order** (`timestamp` first), not initializer list order. The compiler will warn with `-Wreorder`. While harmless here (all are independent POD values), it signals sloppy habits to a professor.

> [!NOTE]
> **Fix priority: LOW.** Reorder the initializer list to match declaration order: `timestamp, userId, deviceId, appId, resourceId, eventType, location`.

---

#### ⚠️ CON-3: `new` Never Fails with `nullptr` — Dead Code

In [LogStore.h:27-31](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/LogStore.h#L27-L31):

```cpp
LogChunk* chunk = new LogChunk(DEFAULT_CHUNK_SIZE);
if (chunk == nullptr) {   // ← DEAD CODE
    return false;
}
```

In standard C++, `new` throws `std::bad_alloc` on failure; it never returns `nullptr` (unless you use `new(std::nothrow)`). This null check is dead code that provides a false sense of safety. Under real OOM, the program will crash with an unhandled exception *before* reaching this check.

> [!NOTE]
> **Fix priority: LOW.** Either use `new(std::nothrow)` to make the check meaningful, or remove the check and let the exception propagate. The latter is standard practice.

---

#### ⚠️ CON-4: `StringPool` Stores Strings Twice

Each unique string is stored in **two** locations:
1. Inside the `Node::key` field (for forward lookup equality comparison).
2. Inside `DynamicArray<std::string> strings` (for reverse lookup by ID).

For 10M rows with ~100K unique strings, this doubles the string memory overhead. Not catastrophic, but wasteful.

> [!TIP]
> **Optimization:** Store the string once in the `DynamicArray` (the canonical copy). Have `Node` store a `uint32_t id` (which it already does) and look up the key from `strings[id]` during equality comparison. This halves string memory.

---

#### ⚠️ CON-5: `DuplicateHashSet` Has No Dynamic Rehashing

The hash set is initialized with a fixed `bucketCount = 2000003`. For 1M rows, the average chain length is ~0.5 (excellent). For 10M rows, the average chain length jumps to ~5.0, degrading insert/lookup from O(1) to O(5) — still acceptable, but fragile.

More critically, the set **never rehashes**. If the dataset grows beyond expectations, chains will grow unbounded.

> [!NOTE]
> **Fix priority: LOW for Phase 1.** For Phase 2, consider dynamic rehashing when `loadFactor > 2.0`, or simply increase the initial bucket count proportionally to expected row count.

---

#### ⚠️ CON-6: `DynamicArray<T>` Lacks Move Semantics

The class deletes copy operations (correctly), but does not define move constructor or move assignment. This means you can never `return` a `DynamicArray` from a function, store it in another container, or transfer ownership. For Phase 1 this is fine since all arrays are members, but Phase 2's anomaly detectors may need to produce result arrays.

---

#### ⚠️ CON-7: `printTop10Resources` Uses `StringPool::size()` as Array Bound

In [QueryEngine.h:153](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/QueryEngine.h#L153):

```cpp
uint32_t resourceCapacity = store.stringPool.size();
uint32_t* counts = new uint32_t[resourceCapacity]();
```

`StringPool::size()` returns `keyCount` (the number of *all* interned strings — users, devices, apps, AND resources). This means `counts[]` is indexed by `entry.resourceId`, but `resourceId` is a global dictionary ID, not a resource-specific ID. This works **only because** all IDs share the same `StringPool` namespace and `resourceId < keyCount` is always true.

**This is correct but fragile.** The `counts` array is larger than necessary (it allocates slots for user/device/app IDs that are never written to), wasting memory. At 10M rows with ~250K unique strings, this allocates a ~1 MB `uint32_t` array — acceptable, but conceptually impure.

---

#### ⚠️ CON-8: No `EventType`/`Location` String Conversion Utilities

The CLI prints User Journey and Resource Journey, but never prints `eventType` or `location` as human-readable strings. The `QueryEngine` output shows:

```
- T: 1713225863 | D018 -> APP003 -> R025
```

Missing the event type and location. For Phase 2 anomaly detection ("FAILED_LOGIN from unusual location"), you'll need `eventTypeToString()` and `locationToString()` helpers.

---

## 3. Code Quality & Safety Audit

### 3.1 Memory Leaks

| Component | Leak? | Analysis |
|-----------|-------|----------|
| `LogChunk` | ✅ No | `delete[] entries` in destructor. |
| `LogStore` | ✅ No | Loops over `chunks`, deletes each `LogChunk*`. |
| `DynamicArray<T>` | ✅ No | `delete[] data` in destructor. |
| `DuplicateHashSet` | ✅ No | Walks every chain, deletes every `FingerprintNode`. |
| `StringPool` | ✅ No | Walks every chain, deletes every `Node`. `DynamicArray<std::string>` auto-destructs. |
| `HashIndex` | ✅ No | Walks every chain, deletes every `Node`. Inner `DynamicArray` auto-destructs. |
| `SortUtils::sortByTimestamp` | ✅ No | `new[]` then `delete[]` for temp buffer. |
| `QueryEngine::printTop10Resources` | ✅ No | `new uint32_t[]` then `delete[]` at the end. |

> [!TIP]
> **Verdict: Zero memory leaks detected.** All `new`/`new[]` have matching `delete`/`delete[]` with correct ownership semantics. Well done.

---

### 3.2 Dangling Pointers

| Scenario | Risk? | Analysis |
|----------|-------|----------|
| `SearchEngine` outlives `LogStore` | ⚠️ **Potential** | `HashIndex` stores `const LogEntry*` pointing into `LogStore`'s chunks. If `LogStore` is destroyed first, all index pointers dangle. In `main.cpp`, both live on the stack with `store` declared before `engine`, so `engine` destructs first. **Safe in current code.** |
| `DynamicArray::resize()` | ✅ No | Old `data` is deleted after copying to `newData`. Pointers to old array elements would dangle — but `DynamicArray` is never used by external code to cache element pointers during resize. |

> [!WARNING]
> **The `LogStore` before `SearchEngine` declaration order in `main.cpp` is a latent correctness dependency.** If someone later moves `engine` to a wider scope or into a struct alongside `store`, destruction order could silently invert. Add a comment documenting this invariant.

---

### 3.3 Double-Free Risks

**None.** All owning classes have `= delete` on copy/assignment. No shared ownership exists.

---

### 3.4 Unsafe Implicit Conversions

| Location | Issue | Severity |
|----------|-------|----------|
| `DataLoader.cpp:41` | `strlen()` returns `size_t`, cast to `uint32_t` | **Negligible** — field names are < 20 chars. |
| `DataLoader.cpp:231` | Pointer subtraction `&line[i] - fieldStart` cast to `uint32_t` | **Safe** — line buffer is 4096 bytes max. |
| `DuplicateHashSet:101` | `fingerprint % bucketCount` — `unsigned long long % uint32_t` | **Safe** — modulo result fits in `uint32_t`. |

**Verdict:** No dangerous implicit conversions found.

---

### 3.5 Input Validation in `main.cpp`

| Input | Validated? | How |
|-------|-----------|-----|
| CSV file path | ⚠️ Partial | Defaults to constant. `fopen` failure returns false. No path traversal check (acceptable for coursework). |
| Menu choice | ✅ Yes | `cin >> choice` with `clearBadInput()` on failure. Invalid values print error message. |
| User/Resource ID | ✅ Yes | Defaults provided. Any string accepted (empty → default). |
| Timestamps | ✅ Yes | `readInt64WithDefault` uses `std::stoll` with try/catch. Start > End auto-swaps. |
| Overly long CSV lines | ✅ Yes | Lines exceeding `MAX_LINE_SIZE (4096)` are silently skipped. |
| Malformed CSV fields | ✅ Yes | `splitAndValidateLine` returns false, row is silently skipped. |

**Grade: A-** — Robust for a coursework CLI. The only minor gap is no feedback for skipped malformed rows (silent data loss).

---

### 3.6 Thread Safety

Not applicable for Phase 1 (single-threaded), but worth noting: **none** of these data structures are thread-safe. If Phase 2 introduces parallel ingestion or query execution, every shared structure will need synchronization.

---

## 4. Phase 2 Scalability — 5 Actionable Improvements

### Improvement 1: Binary Search on Sorted Timelines

**Problem:** `printUserJourney` and `printResourceJourney` perform linear scans O(N) over sorted timelines to find entries within `[startTime, endTime]`. For a user with 50K events but a query window of 100ms, you scan all 50K entries to find 3 matches.

**Solution:** Implement `lowerBound(timestamp)` and `upperBound(timestamp)` binary search on the already-sorted `DynamicArray<const LogEntry*>`. This reduces time-range queries from O(N) to O(log N + K) where K is the result count.

```cpp
// Add to SortUtils or a new BinarySearch utility:
static uint32_t lowerBound(
    const DynamicArray<const LogEntry*>& arr,
    int64_t targetTimestamp
) {
    uint32_t lo = 0, hi = arr.size();
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (arr[mid]->timestamp < targetTimestamp) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}
```

**Impact:** Query time drops from milliseconds to **microseconds** on 10M-row datasets. This is the single highest-ROI change for Phase 2.

---

### Improvement 2: Memory-Mapped I/O (`mmap` / `CreateFileMapping`)

**Problem:** `DataLoader` reads 256 KB blocks via `fread`, then copies bytes into `lineBuffer`. For a 500 MB file, this is ~2000 `fread` syscalls plus byte-by-byte line assembly.

**Solution:** Memory-map the entire CSV file. The OS maps the file directly into virtual address space. `FieldView` pointers point directly into the mapped region — true zero-copy parsing with no `lineBuffer` intermediary.

```
Before: fread → readBuffer → lineBuffer → FieldView → fieldToString → StringPool
After:  mmap → FieldView points directly into mapped memory → StringPool
```

On Windows, use `CreateFileMapping` + `MapViewOfFile`. On Linux, `mmap`.

**Impact:** Eliminates all buffer management code. Ingestion speed improves 2–3× for large files. Enables true zero-copy since `FieldView` pointers are valid for the file's lifetime.

---

### Improvement 3: Eliminate `fieldToString` Allocation in Hot Path

**Problem:** In [DataLoader.cpp:310-313](file:///d:/tlinh/nam_2_(2025-2026)/hk2/CSLT/TH/project/24120085/src/DataLoader.cpp#L310-L313), every parsed row calls `fieldToString()` 4 times, each constructing a temporary `std::string`:

```cpp
store.stringPool.getOrCreateId(fieldToString(parsed.userId)),   // heap alloc
store.stringPool.getOrCreateId(fieldToString(parsed.deviceId)), // heap alloc
store.stringPool.getOrCreateId(fieldToString(parsed.appId)),    // heap alloc
store.stringPool.getOrCreateId(fieldToString(parsed.resourceId)),// heap alloc
```

At 10M rows × 4 fields = **40 million temporary `std::string` allocations**. Even with Small String Optimization (SSO), this is a massive hot-path cost.

**Solution:** Add a `getOrCreateId(const char* data, uint32_t length)` overload to `StringPool` that hashes and compares directly from the `FieldView` pointer, only constructing a `std::string` when a genuinely new key is inserted:

```cpp
uint32_t getOrCreateId(const char* data, uint32_t length) {
    unsigned long long hash = hashRaw(data, length);
    uint32_t index = static_cast<uint32_t>(hash % bucketCount);
    Node* current = buckets[index];
    
    while (current != nullptr) {
        if (current->key.length() == length &&
            std::memcmp(current->key.c_str(), data, length) == 0) {
            return current->id;
        }
        current = current->next;
    }
    
    // Only allocate std::string for genuinely new entries
    std::string key(data, length);
    // ... insert as before
}
```

**Impact:** Eliminates ~39.9M of the 40M temporary allocations (since most strings are repeats). Ingestion speed improves 30–50%.

---

### Improvement 4: Open-Addressing Hash Tables with Robin Hood Hashing

**Problem:** All three hash tables (`DuplicateHashSet`, `StringPool`, `HashIndex`) use separate chaining with linked-list nodes. Each node is individually `new`-allocated, causing:
- 1 heap allocation per unique key.
- Poor cache locality (nodes scattered across the heap).
- 24+ bytes overhead per node (pointer + allocation header).

**Solution:** Replace separate chaining with open-addressing (linear probing or Robin Hood hashing). Store entries inline in the bucket array:

```
Before: buckets[i] → Node* → Node* → Node* → nullptr  (3 heap allocs, 3 cache misses)
After:  buckets[i] = {key, value, occupied}               (0 heap allocs, 1 cache line)
```

For `DuplicateHashSet` at 10M rows, this eliminates **10 million `new FingerprintNode`** calls. For `StringPool`, it eliminates ~100K `new Node` calls.

**Impact:** Index construction time drops 40–60%. Memory overhead drops by 20–30 bytes per entry.

---

### Improvement 5: Per-User/Per-Resource Timestamp Range Caching (Min/Max Metadata)

**Problem:** `printTop10Resources` scans **all** entries across all chunks, even when the query window is narrow. At 10M rows, this is an unavoidable O(N) scan every time.

**Solution:** Store `minTimestamp` and `maxTimestamp` per `LogChunk` (computed during ingestion). During Top-10 counting, skip entire chunks whose time range doesn't overlap the query window:

```cpp
// In LogChunk:
int64_t minTimestamp = INT64_MAX;
int64_t maxTimestamp = INT64_MIN;

// Update on append:
LogEntry* append(const LogEntry& entry) {
    // ... existing code ...
    if (entry.timestamp < minTimestamp) minTimestamp = entry.timestamp;
    if (entry.timestamp > maxTimestamp) maxTimestamp = entry.timestamp;
    return stored;
}

// In QueryEngine, skip entire chunks:
if (chunk->maxTimestamp < startTime || chunk->minTimestamp > endTime) {
    continue;  // Skip ~8192 entries in one comparison
}
```

**Impact:** For narrow time-range queries on 10M rows, this can skip 90%+ of chunks, reducing scan time from ~30ms to ~3ms.

---

## Summary Scorecard

| Category | Score | Notes |
|----------|-------|-------|
| **Requirements Completeness** | 9/9 | All Phase 1 requirements fully implemented. |
| **Memory Safety** | 9/10 | Zero leaks, zero double-frees. Deducted 1 for the latent destruction-order dependency. |
| **Algorithmic Correctness** | 10/10 | Stable merge sort, correct hash indexing, correct bucket counting. |
| **Code Quality** | 8/10 | Excellent comments and naming. Deducted for initializer order mismatch and dead null-check. |
| **Architecture** | 7/10 | Strong foundation, but header-only model and dual string storage are liabilities at scale. |
| **Performance Readiness (10M)** | 6/10 | Linear scans and 40M temp allocations will hurt. Binary search and `FieldView`-based `StringPool` are essential. |

> [!IMPORTANT]
> **Overall Assessment:** This is a **strong Phase 1 submission** demonstrating genuine systems-level thinking — arena allocation, dictionary encoding, fingerprint-based deduplication, and zero-copy parsing are all advanced techniques rarely seen in undergraduate coursework. The memory safety discipline (delete in destructors, = delete on copy) is exemplary. 
> 
> **To survive the 10M-row stress test**, prioritize Improvements #1 (binary search) and #3 (eliminate `fieldToString`) — these two changes alone will likely cut your total query + ingestion time by 50%+.
