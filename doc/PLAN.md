# Halo - Cyber Access Engine Architecture Plan

## Summary

Build a raw-pointer C++ log analytics engine using a Loki-inspired split between compact log chunks and lightweight metadata indexes.

The design follows Grafana Loki’s core idea: avoid indexing every field, store log records in chunks, and use metadata indexes to jump directly to relevant chunks or timelines. Loki stores indexes and chunks separately, and its index maps label sets to chunk references rather than indexing full log content. Microsoft Sentinel UEBA similarly uses entity behavior, historical baselines, frequency changes, geo/device deviations, and anomaly scoring as detection inputs.

Sources used: Grafana Loki architecture/storage docs and Microsoft Sentinel UEBA/anomaly docs.

## Section 1: Real-World Inspired Architecture & Memory Design

### Hybrid Data Model

Use three storage layers:

1. **Canonical Log Storage**
   - Store every parsed row once inside fixed-size `LogChunk` blocks.
   - Each chunk owns a raw `LogEntry* entries` array.
   - Suggested chunk size: `4096` or `8192` rows.
   - Each chunk tracks `minTimestamp`, `maxTimestamp`, and row count.
   - This mimics Loki chunk storage: logs are stored densely, while indexes hold references.

2. **Metadata Hash Indexes**
   - Custom `HashTable` maps high-level keys to timelines:
     - `user_id -> DynamicArray<LogEntry*>`
     - `resource_id -> DynamicArray<LogEntry*>`
     - `device_id -> DynamicArray<LogEntry*>`
     - optional `app_id -> DynamicArray<LogEntry*>`
   - Hash collisions use separate chaining via raw linked-list nodes.
   - No `std::map`, `std::unordered_map`, `std::vector`, or `std::set`.

3. **Sorted Timeline Arrays**
   - Each hash-table value is a tightly packed `DynamicArray<LogEntry*>`.
   - After ingestion, sort each timeline by `timestamp`.
   - Range queries use binary search:
     - lookup key: average `O(1)`
     - lower bound timestamp: `O(log N)`
     - scan matching range: `O(K)`
   - This gives fast timeline queries without scanning 1,000,000 rows.

### Why This Meets the 10-Second Goal

- A 1,000,000-row full scan is acceptable for one-time ingestion, but not for repeated queries.
- Query path should avoid global scans:
  - user timeline query reads only that user’s pointer array
  - resource journey reads only that resource’s pointer array
  - top 10 resources uses precomputed counters
  - anomaly detection runs over sorted per-user or per-device timelines
- Dense arrays reduce cache misses compared with linked lists because adjacent pointers or entries sit close in memory.
- Chunked allocation reduces heap fragmentation because the engine allocates large blocks instead of one object per row.
- Hash-table nodes are allocated only per unique key, not per log row.

### Core C++ Class Definitions

```cpp
#include <string>
#include <fstream>
#include <cstdint>

enum EventType {
    EVENT_LOGIN,
    EVENT_LOGOUT,
    EVENT_TOKEN_REFRESH,
    EVENT_ACCESS,
    EVENT_FAILED_LOGIN,
    EVENT_OPEN_APP,
    EVENT_DOWNLOAD,
    EVENT_ADMIN_ACTION,
    EVENT_INVALID
};

enum Location {
    LOC_US, LOC_VN, LOC_JP, LOC_KR, LOC_SG,
    LOC_CN, LOC_DE, LOC_FR, LOC_UK, LOC_AU,
    LOC_CA, LOC_IN, LOC_BR, LOC_RU, LOC_TH,
    LOC_INVALID
};

struct LogEntry {
    std::string user_id;
    std::string device_id;
    std::string app_id;
    std::string resource_id;
    EventType event_type;
    Location location;
    int64_t timestamp;
    bool valid;

    LogEntry()
        : event_type(EVENT_INVALID),
          location(LOC_INVALID),
          timestamp(0),
          valid(false) {}

    LogEntry(
        const std::string& user,
        const std::string& device,
        const std::string& app,
        const std::string& resource,
        EventType event,
        Location loc,
        int64_t ts,
        bool isValid
    )
        : user_id(user),
          device_id(device),
          app_id(app),
          resource_id(resource),
          event_type(event),
          location(loc),
          timestamp(ts),
          valid(isValid) {}

    ~LogEntry() {}
};
```

```cpp
template <typename T>
class DynamicArray {
private:
    T* data;
    int length;
    int capacity;

    void resize(int newCapacity) {
        T* newData = new T[newCapacity];

        for (int i = 0; i < length; ++i) {
            newData[i] = data[i];
        }

        delete[] data;
        data = newData;
        capacity = newCapacity;
    }

public:
    DynamicArray()
        : data(nullptr), length(0), capacity(0) {}

    explicit DynamicArray(int initialCapacity)
        : data(nullptr), length(0), capacity(0) {
        if (initialCapacity > 0) {
            data = new T[initialCapacity];
            capacity = initialCapacity;
        }
    }

    DynamicArray(const DynamicArray& other)
        : data(nullptr), length(other.length), capacity(other.capacity) {
        if (capacity > 0) {
            data = new T[capacity];
            for (int i = 0; i < length; ++i) {
                data[i] = other.data[i];
            }
        }
    }

    DynamicArray& operator=(const DynamicArray& other) {
        if (this == &other) {
            return *this;
        }

        delete[] data;
        data = nullptr;
        length = other.length;
        capacity = other.capacity;

        if (capacity > 0) {
            data = new T[capacity];
            for (int i = 0; i < length; ++i) {
                data[i] = other.data[i];
            }
        }

        return *this;
    }

    ~DynamicArray() {
        delete[] data;
        data = nullptr;
        length = 0;
        capacity = 0;
    }

    void pushBack(const T& value) {
        if (length == capacity) {
            int nextCapacity = capacity == 0 ? 8 : capacity * 2;
            resize(nextCapacity);
        }

        data[length++] = value;
    }

    T& operator[](int index) {
        return data[index];
    }

    const T& operator[](int index) const {
        return data[index];
    }

    int size() const {
        return length;
    }

    int getCapacity() const {
        return capacity;
    }

    T* raw() {
        return data;
    }

    void clear() {
        length = 0;
    }
};
```

```cpp
class HashTable {
private:
    struct Node {
        std::string key;
        DynamicArray<LogEntry*> values;
        Node* next;

        explicit Node(const std::string& k)
            : key(k), values(8), next(nullptr) {}

        ~Node() {
            next = nullptr;
        }
    };

    Node** buckets;
    int bucketCount;
    int keyCount;

    unsigned long hashString(const std::string& key) const {
        unsigned long hash = 5381;

        for (int i = 0; i < static_cast<int>(key.length()); ++i) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(key[i]);
        }

        return hash;
    }

    Node* findNode(const std::string& key) const {
        unsigned long index = hashString(key) % bucketCount;
        Node* current = buckets[index];

        while (current != nullptr) {
            if (current->key == key) {
                return current;
            }
            current = current->next;
        }

        return nullptr;
    }

public:
    explicit HashTable(int bucketsSize)
        : buckets(nullptr), bucketCount(bucketsSize), keyCount(0) {
        buckets = new Node*[bucketCount];

        for (int i = 0; i < bucketCount; ++i) {
            buckets[i] = nullptr;
        }
    }

    ~HashTable() {
        for (int i = 0; i < bucketCount; ++i) {
            Node* current = buckets[i];

            while (current != nullptr) {
                Node* next = current->next;
                delete current;
                current = next;
            }

            buckets[i] = nullptr;
        }

        delete[] buckets;
        buckets = nullptr;
        bucketCount = 0;
        keyCount = 0;
    }

    void insert(const std::string& key, LogEntry* entry) {
        unsigned long index = hashString(key) % bucketCount;
        Node* current = buckets[index];

        while (current != nullptr) {
            if (current->key == key) {
                current->values.pushBack(entry);
                return;
            }
            current = current->next;
        }

        Node* created = new Node(key);
        created->values.pushBack(entry);
        created->next = buckets[index];
        buckets[index] = created;
        ++keyCount;
    }

    DynamicArray<LogEntry*>* get(const std::string& key) {
        Node* node = findNode(key);
        return node == nullptr ? nullptr : &node->values;
    }

    int size() const {
        return keyCount;
    }
};
```

### Ownership Rules

- `LogChunk` owns actual `LogEntry` objects.
- `HashTable` owns only timeline arrays and linked-list nodes.
- Timeline arrays contain non-owning `LogEntry*` pointers.
- `HashTable` destructor must not delete `LogEntry*`.
- `LogStore` destructor deletes all chunks.
- Temporary arrays inside functions must be deleted before return, or wrapped in local RAII classes.

## Section 2: Step-by-Step Implementation Roadmap

### Phase 1: Midterm, Weeks 1-2

Focus: correctness, parser resilience, 10,000 rows, baseline analytics.

Week 1:

- Define core types:
  - `LogEntry`
  - `DynamicArray<T>`
  - `LogChunk`
  - `LogStore`
  - enum parsers for `EventType` and `Location`
- Build a manual CSV parser:
  - read line by line using `std::ifstream`
  - split exactly 7 columns
  - trim simple whitespace
  - validate enum values
  - validate timestamp as signed 64-bit integer
  - reject malformed rows without crashing
- Store valid rows in chunked storage.
- Track bad-row count and duplicate-row count.
- Implement duplicate detection for 10,000 rows using a simple custom hash set keyed by:
  - `user_id|device_id|app_id|resource_id|event_type|location|timestamp`

Week 2:

- Implement custom sorting:
  - MergeSort preferred for stable timeline sorting
  - QuickSort acceptable if pivot handling avoids worst-case sorted-input behavior
- Implement baseline queries:
  - list events by `user_id`, sorted by timestamp
  - list events by `resource_id`, sorted by timestamp
  - show user/resource journey with event type, location, timestamp
  - top 10 resources by access count
- For top 10:
  - custom resource counter hash table
  - fixed-size `TopResource top[10]`
  - update top 10 after counting
- Midterm acceptance:
  - load 10,000 rows
  - invalid rows skipped with counters
  - duplicate rows skipped or marked once
  - no crash on empty fields, unknown enum values, bad timestamps
  - all allocated memory released on program exit and after temporary functions

### Phase 2: Final, Weeks 3-5

Focus: 1,000,000 rows under 10 seconds, indexed queries, SIEM-style anomaly detection.

Week 3:

- Replace any Phase 1 linear lookup with custom hash indexes:
  - `userIndex`
  - `resourceIndex`
  - `deviceIndex`
  - `appIndex`
- Insert each valid `LogEntry*` into indexes during ingestion.
- Pre-size hash tables:
  - start with `262147` buckets for 1,000,000 rows
  - use a prime bucket count
  - keep load factor reasonable
- Sort each timeline once after ingest.
- Add binary search helpers:
  - `lowerBoundByTimestamp`
  - `upperBoundByTimestamp`
- Query target:
  - user/resource range query should avoid global scan.

Week 4:

- Build anomaly engine with rule matchers.

Threshold-based rules:

- `FAILED_LOGIN_BURST`
  - same `user_id`
  - 5 or more `FAILED_LOGIN` events within 5 minutes
- `RESOURCE_RATE_LIMIT`
  - same `user_id + resource_id`
  - more than 100 `ACCESS` events within 60 seconds
- `ADMIN_ACTION_SPIKE`
  - more than 3 `ADMIN_ACTION` events by same user within 10 minutes

Behavior-based rules:

- `NEW_LOCATION_FOR_USER`
  - user logs in from a location not previously seen in earlier timeline
- `NEW_DEVICE_FOR_USER`
  - user uses an unseen device
- `IMPOSSIBLE_TRAVEL`
  - same user appears in two different countries within an unrealistically short interval
  - for this project, use a simplified rule: different location within 10 minutes

Session-based rules:

- `ACCESS_WITHOUT_LOGIN`
  - user performs `ACCESS`, `DOWNLOAD`, or `ADMIN_ACTION` before a `LOGIN`
- `SESSION_NOT_CLOSED`
  - `LOGIN` without `LOGOUT` after a configured duration
- `TOKEN_REFRESH_WITHOUT_SESSION`
  - `TOKEN_REFRESH` before login or after logout

Sliding-window implementation:

- Use sorted per-user timelines.
- Maintain two integer indexes: `left`, `right`.
- Move `right` forward event by event.
- Move `left` while timestamp window is exceeded.
- Count matching events inside the window.
- No STL queue required.

Week 5:

- Performance tuning and final polish:
  - reduce string copies in parser
  - chunk allocation instead of per-row allocation
  - pre-size arrays
  - avoid repeated sorting
  - avoid full scans during queries
- Add benchmark mode:
  - load time
  - index build time
  - sort time
  - query time
  - anomaly detection time
- Final acceptance:
  - load and index 1,000,000 rows
  - common queries complete well under 10 seconds
  - full anomaly pass targets under 10 seconds on normal student hardware, or reports timing by phase if hardware-bound
  - no memory leaks under Visual Studio Diagnostics, Dr. Memory, or AddressSanitizer where available

## Section 3: Production-Grade Testing & Memory Safety

### Zero-Leak Checklist

- Every `new[]` has exactly one matching `delete[]`.
- Every `new Node` has exactly one matching `delete Node`.
- `HashTable::~HashTable()` walks every bucket chain and deletes all nodes.
- `DynamicArray::~DynamicArray()` deletes its internal array.
- `LogStore::~LogStore()` deletes every allocated `LogChunk`.
- `LogChunk::~LogChunk()` deletes its `LogEntry[]`.
- Indexes never delete `LogEntry*`; they only reference entries owned by chunks.
- Copy constructors and assignment operators are either correctly implemented or explicitly disabled.
- Temporary buffers in parser functions are stack-based where possible.
- Any temporary heap allocation inside a function is wrapped in a local RAII object.
- After destruction, pointers are set to `nullptr` to reduce accidental reuse.
- Run leak checks after:
  - loading empty file
  - loading malformed file
  - loading 10,000 rows
  - loading 1,000,000 rows
  - running all anomaly rules

### CSV Parser Optimization Strategy

- Read file line by line with `std::ifstream`.
- Avoid building many temporary strings.
- Parse each line with integer start/end positions.
- Validate exactly 7 columns.
- Convert enum strings with direct comparisons.
- Convert timestamp manually:
  - reject empty timestamp
  - reject non-digit characters except optional leading `-`
  - detect overflow before multiplying by 10
- Reserve chunk capacity up front.
- Insert indexes during ingestion to avoid second pass where possible.
- Defer timeline sorting until after all rows are loaded.
- For duplicates at final scale, use a custom hash set with row fingerprint keys.

### 5-Row Mock CSV Dataset

```csv
user_id,device_id,app_id,resource_id,event_type,location,timestamp
u001,d001,appA,resX,LOGIN,VN,1716000000
u001,d001,appA,resX,LOGIN,VN,1716000000
u001,d001,appA,resX,FAILED_LOGIN,VN,1716000010
u002,d009,appB,resY,ACCESS,MARS,1716000020
u003,d003,appC,resZ,DOWNLOAD,US,not_a_timestamp
```

Expected parser behavior:

- Row 1 accepted.
- Row 2 detected as duplicate.
- Row 3 accepted.
- Row 4 rejected because `MARS` is invalid.
- Row 5 rejected because timestamp is invalid.

### Assumptions

- The project is greenfield; current repo folders are empty.
- `std::string`, `std::ifstream`, and file I/O are permitted.
- STL containers remain forbidden, but simple headers for primitive utilities may be avoided entirely.
- The engine is single-process and in-memory for the course scope.
- Duplicate rows should be skipped, counted, and reported.
- Invalid rows should be skipped, counted, and never crash the loader.
