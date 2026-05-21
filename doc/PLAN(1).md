# Halo - Cyber Access Engine: 10M+ Row Architecture Upgrade

## Summary

Upgrade the engine from a 1M-row in-memory design to a 10M+ row design by making ingestion single-pass, allocation-aware, dictionary-encoded, duplicate-filtered before parsing, and structurally separated.

Important scope decision: the core runtime remains purely in-memory and always supports CSV reload. No checkpointing is used for correctness. The requested binary cache is treated as a Phase 2 fast-boot optimization: if `processed_logs.bin` is valid and newer than the CSV, load it; otherwise rebuild from CSV and regenerate it.

## Key Architecture Changes

### 1. Ultra-Lightweight `LogEntry`

Avoid storing repeated strings per row. Store integer dictionary IDs instead.

```cpp
#include <string>
#include <fstream>
#include <cstdint>

enum EventType : unsigned char {
    EVENT_LOGIN = 0,
    EVENT_LOGOUT,
    EVENT_TOKEN_REFRESH,
    EVENT_ACCESS,
    EVENT_FAILED_LOGIN,
    EVENT_OPEN_APP,
    EVENT_DOWNLOAD,
    EVENT_ADMIN_ACTION,
    EVENT_INVALID = 255
};

enum Location : unsigned char {
    LOC_US = 0, LOC_VN, LOC_JP, LOC_KR, LOC_SG,
    LOC_CN, LOC_DE, LOC_FR, LOC_UK, LOC_AU,
    LOC_CA, LOC_IN, LOC_BR, LOC_RU, LOC_TH,
    LOC_INVALID = 255
};

struct LogEntry {
    uint32_t userId;
    uint32_t deviceId;
    uint32_t appId;
    uint32_t resourceId;
    int64_t timestamp;
    EventType eventType;
    Location location;

    LogEntry()
        : userId(0), deviceId(0), appId(0), resourceId(0),
          timestamp(0), eventType(EVENT_INVALID), location(LOC_INVALID) {}

    LogEntry(
        uint32_t user,
        uint32_t device,
        uint32_t app,
        uint32_t resource,
        EventType event,
        Location loc,
        int64_t ts
    )
        : userId(user), deviceId(device), appId(app), resourceId(resource),
          timestamp(ts), eventType(event), location(loc) {}

    ~LogEntry() {}
};
```

This keeps each row compact. The row stores IDs, enums, and timestamp only. Human-readable strings live in a dictionary.

### 2. `DynamicArray<T>`

Adds `reserve()` so estimated row counts can allocate once before ingestion.

```cpp
template <typename T>
class DynamicArray {
private:
    T* data;
    uint32_t length;
    uint32_t capacity;

    void resize(uint32_t newCapacity) {
        T* newData = new T[newCapacity];

        for (uint32_t i = 0; i < length; ++i) {
            newData[i] = data[i];
        }

        delete[] data;
        data = newData;
        capacity = newCapacity;
    }

public:
    DynamicArray() : data(nullptr), length(0), capacity(0) {}

    explicit DynamicArray(uint32_t initialCapacity)
        : data(nullptr), length(0), capacity(0) {
        reserve(initialCapacity);
    }

    ~DynamicArray() {
        delete[] data;
        data = nullptr;
        length = 0;
        capacity = 0;
    }

    DynamicArray(const DynamicArray& other) = delete;
    DynamicArray& operator=(const DynamicArray& other) = delete;

    void reserve(uint32_t requestedCapacity) {
        if (requestedCapacity <= capacity) {
            return;
        }

        T* newData = new T[requestedCapacity];

        for (uint32_t i = 0; i < length; ++i) {
            newData[i] = data[i];
        }

        delete[] data;
        data = newData;
        capacity = requestedCapacity;
    }

    void pushBack(const T& value) {
        if (length == capacity) {
            uint32_t nextCapacity = capacity == 0 ? 8 : capacity * 2;
            resize(nextCapacity);
        }

        data[length++] = value;
    }

    T& operator[](uint32_t index) {
        return data[index];
    }

    const T& operator[](uint32_t index) const {
        return data[index];
    }

    uint32_t size() const {
        return length;
    }

    uint32_t getCapacity() const {
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

### 3. `LogChunk`

Owns row memory in large contiguous blocks to reduce heap fragmentation.

```cpp
class LogChunk {
private:
    LogEntry* entries;
    uint32_t capacity;
    uint32_t count;

public:
    explicit LogChunk(uint32_t chunkCapacity)
        : entries(nullptr), capacity(chunkCapacity), count(0) {
        entries = new LogEntry[capacity];
    }

    ~LogChunk() {
        delete[] entries;
        entries = nullptr;
        capacity = 0;
        count = 0;
    }

    LogChunk(const LogChunk& other) = delete;
    LogChunk& operator=(const LogChunk& other) = delete;

    bool hasSpace() const {
        return count < capacity;
    }

    LogEntry* append(const LogEntry& entry) {
        if (!hasSpace()) {
            return nullptr;
        }

        entries[count] = entry;
        return &entries[count++];
    }

    uint32_t size() const {
        return count;
    }

    LogEntry* raw() {
        return entries;
    }
};
```

### 4. `HashTable` With Complete ADT

Used for indexes and string dictionaries. It supports insert/get/remove.

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
    uint32_t bucketCount;
    uint32_t keyCount;

    unsigned long long hashString(const std::string& key) const {
        unsigned long long hash = 5381ULL;

        for (uint32_t i = 0; i < key.length(); ++i) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(key[i]);
        }

        return hash;
    }

public:
    explicit HashTable(uint32_t bucketSize)
        : buckets(nullptr), bucketCount(bucketSize), keyCount(0) {
        buckets = new Node*[bucketCount];

        for (uint32_t i = 0; i < bucketCount; ++i) {
            buckets[i] = nullptr;
        }
    }

    ~HashTable() {
        for (uint32_t i = 0; i < bucketCount; ++i) {
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

    HashTable(const HashTable& other) = delete;
    HashTable& operator=(const HashTable& other) = delete;

    void insert(const std::string& key, LogEntry* entry) {
        uint32_t index = static_cast<uint32_t>(hashString(key) % bucketCount);
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
        uint32_t index = static_cast<uint32_t>(hashString(key) % bucketCount);
        Node* current = buckets[index];

        while (current != nullptr) {
            if (current->key == key) {
                return &current->values;
            }
            current = current->next;
        }

        return nullptr;
    }

    bool remove(const std::string& key) {
        uint32_t index = static_cast<uint32_t>(hashString(key) % bucketCount);
        Node* current = buckets[index];
        Node* previous = nullptr;

        while (current != nullptr) {
            if (current->key == key) {
                if (previous == nullptr) {
                    buckets[index] = current->next;
                } else {
                    previous->next = current->next;
                }

                current->next = nullptr;
                delete current;
                --keyCount;
                return true;
            }

            previous = current;
            current = current->next;
        }

        return false;
    }

    uint32_t size() const {
        return keyCount;
    }
};
```

### 5. `DuplicateHashSet`

Rejects duplicate raw CSV lines before parsing. This avoids comma splitting, enum parsing, and dictionary lookup for duplicates.

```cpp
class DuplicateHashSet {
private:
    struct FingerprintNode {
        unsigned long long fingerprint;
        FingerprintNode* next;

        explicit FingerprintNode(unsigned long long value)
            : fingerprint(value), next(nullptr) {}
    };

    FingerprintNode** buckets;
    uint32_t bucketCount;
    uint32_t itemCount;

public:
    explicit DuplicateHashSet(uint32_t bucketSize)
        : buckets(nullptr), bucketCount(bucketSize), itemCount(0) {
        buckets = new FingerprintNode*[bucketCount];

        for (uint32_t i = 0; i < bucketCount; ++i) {
            buckets[i] = nullptr;
        }
    }

    ~DuplicateHashSet() {
        for (uint32_t i = 0; i < bucketCount; ++i) {
            FingerprintNode* current = buckets[i];

            while (current != nullptr) {
                FingerprintNode* next = current->next;
                delete current;
                current = next;
            }

            buckets[i] = nullptr;
        }

        delete[] buckets;
        buckets = nullptr;
        bucketCount = 0;
        itemCount = 0;
    }

    DuplicateHashSet(const DuplicateHashSet& other) = delete;
    DuplicateHashSet& operator=(const DuplicateHashSet& other) = delete;

    static unsigned long long djb2(const std::string& line) {
        unsigned long long hash = 5381ULL;

        for (uint32_t i = 0; i < line.length(); ++i) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(line[i]);
        }

        return hash;
    }

    bool contains(unsigned long long fingerprint) const {
        uint32_t index = static_cast<uint32_t>(fingerprint % bucketCount);
        FingerprintNode* current = buckets[index];

        while (current != nullptr) {
            if (current->fingerprint == fingerprint) {
                return true;
            }
            current = current->next;
        }

        return false;
    }

    bool insertIfAbsent(unsigned long long fingerprint) {
        uint32_t index = static_cast<uint32_t>(fingerprint % bucketCount);
        FingerprintNode* current = buckets[index];

        while (current != nullptr) {
            if (current->fingerprint == fingerprint) {
                return false;
            }
            current = current->next;
        }

        FingerprintNode* created = new FingerprintNode(fingerprint);
        created->next = buckets[index];
        buckets[index] = created;
        ++itemCount;
        return true;
    }

    uint32_t size() const {
        return itemCount;
    }
};
```

## Optimized `loadData()` Design

### Supporting Helpers

```cpp
struct ParsedFields {
    std::string user;
    std::string device;
    std::string app;
    std::string resource;
    EventType eventType;
    Location location;
    int64_t timestamp;
};

struct LoadStats {
    uint64_t totalLines;
    uint64_t validRows;
    uint64_t invalidRows;
    uint64_t duplicateRows;

    LoadStats()
        : totalLines(0), validRows(0), invalidRows(0), duplicateRows(0) {}
};
```

```cpp
uint64_t estimateRowsFromFile(std::ifstream& file) {
    file.seekg(0, std::ifstream::end);
    uint64_t bytes = static_cast<uint64_t>(file.tellg());
    file.seekg(0, std::ifstream::beg);

    const uint64_t estimatedAverageLineBytes = 96;
    uint64_t estimatedRows = bytes / estimatedAverageLineBytes;

    if (estimatedRows < 1024) {
        estimatedRows = 1024;
    }

    return estimatedRows + estimatedRows / 10;
}
```

```cpp
bool splitSevenColumns(const std::string& line, ParsedFields& out) {
    int starts[7];
    int ends[7];
    int column = 0;
    int start = 0;

    for (int i = 0; i <= static_cast<int>(line.length()); ++i) {
        if (i == static_cast<int>(line.length()) || line[i] == ',') {
            if (column >= 7) {
                return false;
            }

            starts[column] = start;
            ends[column] = i;
            ++column;
            start = i + 1;
        }
    }

    if (column != 7) {
        return false;
    }

    out.user = line.substr(starts[0], ends[0] - starts[0]);
    out.device = line.substr(starts[1], ends[1] - starts[1]);
    out.app = line.substr(starts[2], ends[2] - starts[2]);
    out.resource = line.substr(starts[3], ends[3] - starts[3]);

    if (out.user.empty() || out.device.empty() || out.app.empty() || out.resource.empty()) {
        return false;
    }

    std::string eventText = line.substr(starts[4], ends[4] - starts[4]);
    std::string locationText = line.substr(starts[5], ends[5] - starts[5]);
    std::string timestampText = line.substr(starts[6], ends[6] - starts[6]);

    out.eventType = parseEventType(eventText);
    out.location = parseLocation(locationText);

    if (out.eventType == EVENT_INVALID || out.location == LOC_INVALID) {
        return false;
    }

    return parseInt64(timestampText, out.timestamp);
}
```

### Main Single-Pass Loader

```cpp
bool loadData(const char* csvPath, LogStore& store, LoadStats& stats) {
    std::ifstream file(csvPath);

    if (!file.is_open()) {
        return false;
    }

    uint64_t estimatedRows = estimateRowsFromFile(file);

    store.reserveRows(estimatedRows);
    store.userIndex.reserveBuckets(262147);
    store.resourceIndex.reserveBuckets(262147);
    store.deviceIndex.reserveBuckets(262147);
    store.appIndex.reserveBuckets(262147);
    store.stringPool.reserve(estimatedRows / 4);

    DuplicateHashSet duplicates(static_cast<uint32_t>(estimatedRows * 2 + 1));

    std::string line;
    ParsedFields parsed;

    if (!std::getline(file, line)) {
        return true;
    }

    while (std::getline(file, line)) {
        ++stats.totalLines;

        if (line.empty()) {
            ++stats.invalidRows;
            continue;
        }

        unsigned long long fingerprint = DuplicateHashSet::djb2(line);

        if (!duplicates.insertIfAbsent(fingerprint)) {
            ++stats.duplicateRows;
            continue;
        }

        if (!splitSevenColumns(line, parsed)) {
            ++stats.invalidRows;
            continue;
        }

        uint32_t userId = store.stringPool.getOrCreateId(parsed.user);
        uint32_t deviceId = store.stringPool.getOrCreateId(parsed.device);
        uint32_t appId = store.stringPool.getOrCreateId(parsed.app);
        uint32_t resourceId = store.stringPool.getOrCreateId(parsed.resource);

        LogEntry entry(
            userId,
            deviceId,
            appId,
            resourceId,
            parsed.eventType,
            parsed.location,
            parsed.timestamp
        );

        LogEntry* stored = store.append(entry);

        if (stored == nullptr) {
            ++stats.invalidRows;
            continue;
        }

        store.userIndex.insert(parsed.user, stored);
        store.resourceIndex.insert(parsed.resource, stored);
        store.deviceIndex.insert(parsed.device, stored);
        store.appIndex.insert(parsed.app, stored);

        ++stats.validRows;
    }

    store.sortAllTimelinesByTimestamp();
    return true;
}
```

Implementation note: `reserveBuckets()` can be implemented by constructing indexes with the right bucket count up front. If assignment requirements forbid hash-table resizing, instantiate the indexes after estimating file size.

## Binary Serialization Plan

### Why Raw Dumps Are Forbidden

Do not write `sizeof(LogEntry)` blindly if `LogEntry` contains `std::string`, raw pointers, or process-specific addresses. Reloading those bytes would restore invalid pointers and cause undefined behavior.

With the upgraded compact `LogEntry`, binary serialization is safer because rows contain only integers and enums. Still, the string dictionary must be serialized explicitly.

### Binary Format

Use this layout:

```text
HALO_BIN_V1
uint32_t stringCount
repeat stringCount:
    uint32_t length
    char[length] bytes

uint64_t logCount
repeat logCount:
    uint32_t userId
    uint32_t deviceId
    uint32_t appId
    uint32_t resourceId
    int64_t timestamp
    unsigned char eventType
    unsigned char location
```

### Boot Logic

```text
if processed_logs.bin exists and is newer than source CSV:
    load dictionary from binary
    load compact LogEntry rows from binary into LogStore chunks
    rebuild hash indexes from loaded rows
    sort timelines
else:
    load CSV using single-pass parser
    serialize valid dictionary and rows to processed_logs.bin
```

Indexes should not be serialized in v1. Rebuilding indexes from compact binary rows is simpler, safer, and still fast.

## Test Plan

- Load empty CSV: no crash, zero valid rows.
- Load malformed CSV: invalid row counter increments.
- Load duplicate lines: duplicate rejected before parsing.
- Load unknown event/location: row skipped.
- Load timestamp overflow or non-numeric timestamp: row skipped.
- Load 10M synthetic rows:
  - no array resizing after reservation for main row storage
  - duplicate hash set remains O(1) average case
  - timelines are sorted once after ingestion
- Memory validation:
  - `DynamicArray` releases arrays
  - `LogChunk` releases row blocks
  - `HashTable::remove()` deletes removed nodes
  - `HashTable` destructor deletes all remaining nodes
  - `DuplicateHashSet` destructor deletes all fingerprint nodes
- Binary cache validation:
  - first boot parses CSV and writes binary
  - second boot loads binary
  - loaded row count and top-10 resources match CSV path
  - corrupt binary falls back to CSV reload

## Assumptions

- Phase 1 follows the pure in-memory CSV-reload requirement.
- Phase 2 may add binary fast-boot caching as an optimization, not as correctness checkpointing.
- `event_type` and `location` are stored as 1-byte enums.
- IDs are dictionary encoded as `uint32_t`, supporting more than enough unique users/devices/apps/resources for this project.
- Exact CSV quoting support is out of scope unless the dataset contains quoted commas.
