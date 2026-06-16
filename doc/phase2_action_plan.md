# Phase 2: Anomaly Detection — Action Plan (v2 — Đầy đủ)
**Dự án:** Halo — Cyber Access Engine  
**Vai trò:** Senior C++ Systems Architect  
**Ràng buộc:** Zero-STL | Batch Processing | 10M-row scale

---

## 0. Danh sách đầy đủ 11 Luật phát hiện

| # | Nhóm | Luật | Trigger | Độ khó |
|---|---|---|---|---|
| 1 | Threshold | Brute-force | >= 5 FAILED_LOGIN trong 5 phút | ⭐ |
| 2 | Threshold | Device Hopping | >= 3 deviceId khác nhau trong 10 phút | ⭐ |
| 3 | Threshold | Resource Scan | 1 thiết bị >= 10 resource khác nhau trong 5 phút | ⭐⭐ |
| 4 | Threshold | Out-of-Hours | Mọi sự kiện ngoài 06:00–22:00 | ⭐ |
| 5 | Behavior | **Impossible Travel** | Location thay đổi với delta_time < 2 giờ (2 event liền kề) | ⭐⭐ |
| 6 | Behavior | **Multi-Country Hopping** | >= 3 quốc gia KHÁC NHAU trong 48 giờ | ⭐⭐ |
| 7 | Session | Long Session | Phiên > 24 giờ | ⭐ |
| 8 | Session | Danger Chain | >= 3 ADMIN_ACTION hoặc DOWNLOAD trong 1 phiên | ⭐ |
| 9 | Session | Rapid Session Creation | >= 3 phiên LOGIN trong 10 phút | ⭐⭐ |
| 10 | Advanced | Brute-force Success | FAILED_LOGIN liên tục → LOGIN thành công | ⭐⭐⭐ |
| 11 | Advanced | Dormant Account | Im lặng > 30 ngày → đột ngột hoạt động mạnh | ⭐⭐⭐ |

> **Phân biệt Luật 5 vs Luật 6:**  
> - Luật 5 (`Impossible Travel`): Dựa trên **Vận tốc**. So sánh **2 event liền kề**. Trigger khi delta_time quá ngắn giữa 2 location khác nhau (VN→US trong 30 phút = bất khả thi về vật lý).  
> - Luật 6 (`Multi-Country Hopping`): Dựa trên **Tính đa dạng**. Dùng **sliding window 48 giờ**. Trigger khi **quá nhiều quốc gia khác nhau** xuất hiện trong 2 ngày — ngay cả khi mỗi chuyến bay là hợp lý về mặt thời gian (VN→TH→SG trong 48h — vật lý OK, nhưng hành vi đáng ngờ).  
> Cần thêm `locationWindow: RingBuffer<MULTI_COUNTRY_THRESHOLD>` vào `UserContext`.

> **Luật 3** cần `DeviceContext` (mới). **Luật 6, 9, 10, 11** cần bổ sung state vào `UserContext`.

---

## 1. Cấu trúc Thư mục `src/anomaly/`

```
src/
└── anomaly/
    ├── AnomalyRules.h          ← Config: tất cả constants/thresholds
    ├── AnomalyRecord.h         ← POD: kết quả một anomaly (24 bytes)
    ├── RingBuffer.h            ← Template: Circular buffer (timestamp & uint32 variants)
    ├── UserContext.h           ← State: per-user (9 trường state)   [CẬP NHẬT]
    ├── DeviceContext.h         ← State: per-device (resource scan)  [MỚI]
    ├── AnomalyDetector.h       ← Orchestrator: interface
    └── AnomalyDetector.cpp     ← Orchestrator: 10 luật implementation
```

---

## 2. Header Interfaces

### 2.1 `AnomalyRules.h` — Tầng Config

```cpp
// anomaly/AnomalyRules.h
#ifndef ANOMALY_RULES_H
#define ANOMALY_RULES_H

#include <cstdint>

namespace AnomalyRules {
    // --- Threshold-based ---
    constexpr uint32_t BRUTE_FORCE_THRESHOLD      = 5;
    constexpr int64_t  BRUTE_FORCE_WINDOW_SEC     = 5 * 60;       // 5 phút

    constexpr uint32_t DEVICE_HOP_THRESHOLD       = 3;
    constexpr int64_t  DEVICE_HOP_WINDOW_SEC      = 10 * 60;      // 10 phút

    constexpr uint32_t RESOURCE_SCAN_THRESHOLD    = 10;           // [MỚI] resources/device
    constexpr int64_t  RESOURCE_SCAN_WINDOW_SEC   = 5 * 60;       // [MỚI] 5 phút

    constexpr int32_t  WORK_HOUR_START            = 6;            // [MỚI] 06:00
    constexpr int32_t  WORK_HOUR_END              = 22;           // [MỚI] 22:00

    // --- Behavior-based ---
    constexpr int64_t  IMPOSSIBLE_TRAVEL_MIN_SEC  = 2 * 3600;     // 2 giờ

    constexpr uint32_t MULTI_COUNTRY_THRESHOLD    = 3;            // unique locations
    constexpr int64_t  MULTI_COUNTRY_WINDOW_SEC   = 48 * 3600;    // 48 giờ (2 ngày)

    // --- Session-based ---
    constexpr int64_t  MAX_SESSION_DURATION_SEC   = 24 * 3600;    // 24 giờ
    constexpr uint32_t DANGER_CHAIN_THRESHOLD     = 3;

    constexpr uint32_t RAPID_SESSION_THRESHOLD    = 3;            // [MỚI] phiên
    constexpr int64_t  RAPID_SESSION_WINDOW_SEC   = 10 * 60;      // [MỚI] 10 phút

    // --- Advanced ---
    constexpr int64_t  DORMANT_THRESHOLD_SEC      = 30LL*24*3600; // 30 ngày
    constexpr uint32_t DORMANT_BURST_THRESHOLD    = 20;            // [FIX] >= 20 events
    constexpr int64_t  DORMANT_BURST_WINDOW_SEC   = 10 * 60;       // [FIX] trong 10 phút sau thức dậy
}

#endif
```

---

### 2.2 `AnomalyRecord.h` — Kết quả một Anomaly (CẬP NHẬT enum)

```cpp
// anomaly/AnomalyRecord.h
#ifndef ANOMALY_RECORD_H
#define ANOMALY_RECORD_H

#include <cstdint>

enum class AnomalyType : uint8_t {
    BRUTE_FORCE           = 0,
    DEVICE_HOPPING        = 1,
    RESOURCE_SCAN         = 2,
    OUT_OF_HOURS          = 3,
    IMPOSSIBLE_TRAVEL     = 4,
    RAPID_LOC_CHANGE      = 5,   // [MỚI]
    LONG_SESSION          = 6,
    DANGER_CHAIN          = 7,
    RAPID_SESSION         = 8,
    BRUTE_FORCE_SUCCESS   = 9,
    DORMANT_ACCOUNT       = 10,
};

/**
 * @brief POD struct đại diện cho một anomaly. 24 bytes aligned. Zero-heap.
 */
struct AnomalyRecord {
    int64_t     timestamp;
    uint32_t    userId;
    uint32_t    deviceId;    // deviceId liên quan (hoặc 0)
    AnomalyType type;
    uint8_t     _pad[3];

    AnomalyRecord()
        : timestamp(0), userId(0), deviceId(0),
          type(AnomalyType::BRUTE_FORCE) {}
};

#endif
```

---

### 2.3 `RingBuffer.h` — Circular Buffer (Hai variant)

```cpp
// anomaly/RingBuffer.h
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <cstdint>

/**
 * @brief Circular Buffer lưu N giá trị int64_t gần nhất (timestamp hoặc cast ID).
 * Template CAPACITY cố định tại compile-time. Zero-STL, Zero-heap.
 */
template <uint32_t CAPACITY>
class RingBuffer {
public:
    RingBuffer() : head(0), count(0) {}

    void push(int64_t value) {
        data[head] = value;
        head = (head + 1) % CAPACITY;
        if (count < CAPACITY) ++count;
    }

    void clear() { head = 0; count = 0; }

    uint32_t size()   const { return count; }
    bool     isFull() const { return count == CAPACITY; }

    // Phần tử cũ nhất (chỉ gọi khi isFull() == true)
    int64_t oldest() const { return data[head]; }

    /**
     * @brief Kiểm tra threshold: CAPACITY sự kiện có xảy ra trong [now - window, now]?
     * Thuật toán O(1): khi đầy, `head` trỏ đúng vào phần tử CŨ NHẤT.
     */
    bool isThresholdBreached(int64_t currentTs, int64_t windowSec) const {
        if (!isFull()) return false;
        return (currentTs - data[head]) <= windowSec;
    }

    /**
     * @brief Đếm số phần tử UNIQUE trong buffer (dùng cho Resource Scan / Device Hop).
     * Thuật toán O(CAPACITY²) — chấp nhận vì CAPACITY <= 10.
     */
    uint32_t countUnique() const {
        uint32_t unique = 0;
        for (uint32_t i = 0; i < count; ++i) {
            bool isDuplicate = false;
            for (uint32_t j = 0; j < i; ++j) {
                if (data[i] == data[j]) { isDuplicate = true; break; }
            }
            if (!isDuplicate) ++unique;
        }
        return unique;
    }

private:
    int64_t  data[CAPACITY];
    uint32_t head;
    uint32_t count;
};

#endif
```

---

### 2.4 `UserContext.h` — Per-User State (CẬP NHẬT ĐẦY ĐỦ)

```cpp
// anomaly/UserContext.h
#ifndef USER_CONTEXT_H
#define USER_CONTEXT_H

#include <cstdint>
#include "../core/LogEntry.h"
#include "RingBuffer.h"
#include "AnomalyRules.h"

/**
 * @brief Toàn bộ state cần thiết cho 1 user để phát hiện anomaly.
 *
 * Direct-Address Table: contexts[userId]. Lookup O(1).
 *
 * Memory estimate (per user):
 *   failedLoginWindow : 5 * 8  = 40 bytes
 *   sessionWindow     : 3 * 8  = 24 bytes
 *   Scalars           :        ~56 bytes
 *   Total             :       ~120 bytes/user → 100K users = 12 MB ✓
 */
struct UserContext {
    uint32_t userId;

    // ── Behavior: Impossible Travel (Luật 5) ─────────────────────
    int64_t  lastActivityTimestamp; // Timestamp của event cuối (cho Dormant & Impossible Travel)
    int64_t  firstActivityTimestamp;// Timestamp của event đầu tiên (cho Dormant)
    Location lastLocation;
    bool     hasActivity;

    // ── Behavior: Rapid Location Change (Luật 6) [MỚI] ──────────
    // Lưu location (cast int64_t) của N event gần nhất để đếm unique locations
    // Push mọi event có location, dùng countUnique() + isThresholdBreached()
    RingBuffer<AnomalyRules::RAPID_LOC_THRESHOLD> locationWindow;

    // ── Threshold: Brute Force (Luật 1) ──────────────────────────
    // Lưu timestamp của các FAILED_LOGIN gần nhất
    RingBuffer<AnomalyRules::BRUTE_FORCE_THRESHOLD> failedLoginWindow;

    // ── Threshold: Device Hop (Luật 2) ───────────────────────────
    // Lưu deviceId (cast int64_t) của các lần đăng nhập gần nhất
    RingBuffer<AnomalyRules::DEVICE_HOP_THRESHOLD> deviceHopWindow;
    uint32_t lastDeviceId;

    // ── Session (Luật 6, 7) ───────────────────────────────────────
    bool     hasActiveSession;
    int64_t  sessionStartTimestamp;
    uint32_t dangerousActionCount;

    // ── Rapid Session (Luật 8) [MỚI] ─────────────────────────────
    // Lưu timestamp của các lần LOGIN gần nhất
    RingBuffer<AnomalyRules::RAPID_SESSION_THRESHOLD> sessionStartWindow;

    // ── Brute-force Success (Luật 10) ──────────────────────────────
    // Đếm số FAILED_LOGIN LIÊN TIẾP. Reset về 0 khi gặp BẤT KỲ event
    // nào KHÔNG phải FAILED_LOGIN (LOGIN, LOGOUT, DOWNLOAD, v.v.)
    uint32_t consecutiveFailedCount;

    // ── Dormant Account (Luật 11) ────────────────────────────────
    bool     wasLongDormant;          // true nếu đã phát hiện dormant gap
    uint32_t burstEventCount;         // Đếm events sau khi thức dậy
    int64_t  dormantWakeupTimestamp;   // [FIX] Thời điểm "thức dậy" → theo dõi burst window

    UserContext()
        : userId(0),
          lastActivityTimestamp(0), firstActivityTimestamp(0),
          lastLocation(LOC_INVALID), hasActivity(false),
          lastDeviceId(0),
          hasActiveSession(false), sessionStartTimestamp(0),
          dangerousActionCount(0),
          consecutiveFailedCount(0),
          wasLongDormant(false), burstEventCount(0),
          dormantWakeupTimestamp(0) {}
};

#endif
```

---

### 2.5 `DeviceContext.h` — Per-Device State [FILE MỚI]

```cpp
// anomaly/DeviceContext.h
#ifndef DEVICE_CONTEXT_H
#define DEVICE_CONTEXT_H

#include <cstdint>
#include "RingBuffer.h"
#include "AnomalyRules.h"

/**
 * @brief State của một thiết bị để phát hiện Resource Scan (Luật 3).
 *
 * Vì dataset có thể có đến 50K+ thiết bị, KHÔNG dùng Direct-Address Table.
 * Thay vào đó, dùng một HashIndex<DeviceContext> (custom, tương tự HashIndex
 * của Phase 1) để lookup O(1) theo deviceId.
 *
 * Tuy nhiên, để đơn giản trong implementation ban đầu, có thể dùng cùng
 * Direct-Address Table với kích thước = stringPool.size().
 */
struct DeviceContext {
    uint32_t deviceId;

    // Lưu (timestamp, resourceId) của các resource được truy cập gần nhất.
    // Dùng isThresholdBreached() + countUnique() để detect.
    // [FIX] Xóa windowStartTimestamp — dùng Sliding Window qua RingBuffer
    // thay cho Tumbling Window để tránh lỗ hổng boundary exploit.
    RingBuffer<AnomalyRules::RESOURCE_SCAN_THRESHOLD> resourceWindow;

    DeviceContext()
        : deviceId(0) {}
};

#endif
```

---

### 2.6 `AnomalyDetector.h` — Orchestrator (CẬP NHẬT)

```cpp
// anomaly/AnomalyDetector.h
#ifndef ANOMALY_DETECTOR_H
#define ANOMALY_DETECTOR_H

#include <cstdint>
#include "../core/DynamicArray.h"
#include "../core/LogEntry.h"
#include "AnomalyRecord.h"
#include "UserContext.h"
#include "DeviceContext.h"

class SearchEngine;
class StringPool;

class AnomalyDetector {
private:
    // ── State stores ──────────────────────────────────────────────
    UserContext   *userContexts;    // Direct-Address Table [userId]
    DeviceContext *deviceContexts;  // Direct-Address Table [deviceId]
    uint32_t       contextCapacity; // = stringPool.size()

    DynamicArray<AnomalyRecord> results;

    // ── Internal helpers ──────────────────────────────────────────
    void processEvent(const LogEntry *entry);

    // Nhóm 1: Threshold
    void checkBruteForce      (UserContext &ctx, const LogEntry *e);  // Luật 1
    void checkDeviceHopping   (UserContext &ctx, const LogEntry *e);  // Luật 2
    void checkResourceScan    (DeviceContext &dctx, const LogEntry *e); // Luật 3 [MỚI]
    void checkOutOfHours      (const LogEntry *e);                    // Luật 4 [MỚI]

    // Nhóm 2: Behavior
    void checkImpossibleTravel(UserContext &ctx, const LogEntry *e);  // Luật 5
    void checkRapidLocChange  (UserContext &ctx, const LogEntry *e);  // Luật 6 [MỚI]

    // Nhóm 3: Session
    void checkLongSession     (UserContext &ctx, const LogEntry *e);  // Luật 7
    void checkDangerChain     (UserContext &ctx, const LogEntry *e);  // Luật 8
    void checkRapidSession    (UserContext &ctx, const LogEntry *e);  // Luật 9

    // Advanced
    void checkBruteForceSuccess(UserContext &ctx, const LogEntry *e); // Luật 10
    void checkDormantAccount  (UserContext &ctx, const LogEntry *e);  // Luật 11

    // Helper: kiểm tra giờ từ Unix timestamp (UTC)
    static int32_t extractHourUTC(int64_t timestamp);

    void recordAnomaly(AnomalyType type, const LogEntry *e);

public:
    explicit AnomalyDetector(uint32_t poolSize);
    ~AnomalyDetector();

    AnomalyDetector(const AnomalyDetector &) = delete;
    AnomalyDetector &operator=(const AnomalyDetector &) = delete;

    void runAll(const SearchEngine &engine, const StringPool &pool);
    void printReport(const StringPool &pool) const;
    const DynamicArray<AnomalyRecord> &getResults() const { return results; }
};

#endif
```

---

## 3. Data Flow: O(N) End-to-End

```
Phase 1 Output: LogStore + SearchEngine (userIndex sorted by timestamp)
                          │
                          ▼
        AnomalyDetector::runAll(engine, pool)
                          │
     ┌────────────────────▼────────────────────────┐
     │  for userId = 0 → pool.size() - 1           │
     │    timeline = engine.searchByUser(userId)   │  O(1)
     │    if (nullptr) continue                     │
     │                                             │
     │    for each entry in timeline               │  O(K)
     │      processEvent(entry)                    │
     │        ├─ checkBruteForce()        O(1)     │
     │        ├─ checkDeviceHopping()     O(1)     │
     │        ├─ checkResourceScan()      O(C²)≈O(1)│  C≤10
     │        ├─ checkOutOfHours()        O(1)     │
     │        ├─ checkImpossibleTravel()  O(1)     │
     │        ├─ checkLongSession()       O(1)     │
     │        ├─ checkDangerChain()       O(1)     │
     │        ├─ checkRapidSession()      O(1)     │
     │        ├─ checkBruteForceSuccess() O(1)     │
     │        └─ checkDormantAccount()   O(1)      │
     └─────────────────────────────────────────────┘
                          │
                          ▼
        DynamicArray<AnomalyRecord> results → printReport()
```

**Tổng: O(N)** — N = tổng entries, mỗi entry xử lý đúng 1 lần.

---

## 4. Thuật toán chi tiết cho các luật MỚI

### Luật 3 — Resource Scan (DeviceContext) [ĐÃ SỬA]
```
Khi gặp bất kỳ event nào của device D:
  1. deviceCtx.resourceWindow.push(entry.resourceId)  // cast sang int64_t
  2. Nếu deviceCtx.resourceWindow.isThresholdBreached(entry.timestamp, RESOURCE_SCAN_WINDOW_SEC):
     a. Nếu deviceCtx.resourceWindow.countUnique() >= RESOURCE_SCAN_THRESHOLD:
        → ANOMALY (RESOURCE_SCAN)
        → deviceCtx.resourceWindow.clear()  // Reset để tránh spam báo liên tục
```
> **Tại sao dùng Sliding Window (RingBuffer) thay vì Tumbling Window:**  
> Tumbling Window bị lỗ hổng boundary exploit: hacker quét 9 resource ở phút 4:59  
> rồi quét 9 resource ở phút 5:01 (tổng 18 resource trong 2 giây) mà không bị phát hiện  
> vì bộ đếm reset ở ranh giới phút 5:00. RingBuffer loại bỏ hoàn toàn lỗ hổng này.

### Luật 4 — Out-of-Hours
```
hour = extractHourUTC(entry.timestamp)   // = (timestamp / 3600) % 24
if (hour < WORK_HOUR_START || hour >= WORK_HOUR_END): → ANOMALY
```
> **Lưu ý:** `extractHourUTC` = `(timestamp / 3600) % 24`. Zero-dependency, O(1).

### Luật 8 — Rapid Session Creation
```
Khi gặp EVENT_LOGIN:
  sessionStartWindow.push(entry.timestamp)
  if sessionStartWindow.isThresholdBreached(ts, RAPID_SESSION_WINDOW_SEC): → ANOMALY
```

### Luật 10 — Brute-force Success ⭐⭐⭐ [ĐÃ SỬA]
```
Khi gặp EVENT_FAILED_LOGIN:
  consecutiveFailedCount++

Khi gặp BẤT KỲ event nào KHÁC FAILED_LOGIN:    // [FIX] không chỉ LOGIN
  if (eventType == EVENT_LOGIN && consecutiveFailedCount >= BRUTE_FORCE_THRESHOLD):
    → ANOMALY (BRUTE_FORCE_SUCCESS)
  consecutiveFailedCount = 0   // LUÔN reset khi gặp non-FAILED_LOGIN
```
> **Tại sao reset trên mọi non-FAILED_LOGIN thay vì chỉ LOGIN:**  
> Nếu chỉ reset khi gặp LOGIN, user gõ sai pass 5 lần → đi lướt web (sinh EVENT_DOWNLOAD,  
> EVENT_ACCESS...) → hôm sau login thành công → hệ thống vẫn báo anomaly vì  
> consecutiveFailedCount vẫn = 5. Điều này là false positive.

### Luật 11 — Dormant Account ⭐⭐⭐ [ĐÃ SỬA]
```
Với mỗi event của user U (timeline đã sort tăng dần):
  if (!hasActivity):
    firstActivityTimestamp = entry.timestamp
    hasActivity = true
  else:
    delta = entry.timestamp - lastActivityTimestamp
    if (delta > DORMANT_THRESHOLD_SEC):
      wasLongDormant = true
      burstEventCount = 0
      dormantWakeupTimestamp = entry.timestamp   // [FIX] ghi nhận thời điểm thức dậy

  if (wasLongDormant):
    if (entry.timestamp - dormantWakeupTimestamp <= DORMANT_BURST_WINDOW_SEC):
      burstEventCount++
      if (burstEventCount == DORMANT_BURST_THRESHOLD):  // [FIX] >= 20 events trong 10 phút
        → ANOMALY (DORMANT_ACCOUNT)
    else:
      wasLongDormant = false   // Hết thời gian theo dõi burst, user bình thường

  lastActivityTimestamp = entry.timestamp
```
> **Tại sao cần burst threshold thay vì trigger ngay event đầu tiên:**  
> Nhân viên đi du lịch 1 tháng về, bật app lên tạo 1 event → hệ thống báo anomaly  
> và khóa tài khoản = false positive. "Đột ngột hoạt động mạnh" = >= 20 events trong  
> 10 phút sau khi thức dậy, đúng hành vi của kẻ tấn công kích hoạt tài khoản cũ.

---

## 5. Lộ trình Code (Dependency-First)

### Bước 1 — Config & POD (15 phút)
- [ ] `AnomalyRules.h` — tất cả constants
- [ ] `AnomalyRecord.h` — enum 10 types + POD struct

### Bước 2 — Data Structures (30 phút)
- [ ] `RingBuffer.h` — thêm `countUnique()` và `oldest()`
- [ ] `UserContext.h` — cập nhật đầy đủ 10 trường state
- [ ] `DeviceContext.h` — file mới

### Bước 3 — Detection Rules (90 phút theo thứ tự tăng dần độ phức tạp)
- [ ] `AnomalyDetector.h` — khai báo đầy đủ 10 rules
- [ ] `AnomalyDetector.cpp`:
  1. `checkOutOfHours()` — 3 dòng
  2. `checkBruteForce()` — RingBuffer push + isThresholdBreached
  3. `checkRapidSession()` — tương tự Brute Force
  4. `checkDeviceHopping()` — RingBuffer + countUnique
  5. `checkResourceScan()` — DeviceContext + countUnique
  6. `checkImpossibleTravel()` — compare lastLocation
  7. `checkLongSession()` — compare sessionStartTimestamp
  8. `checkDangerChain()` — đếm dangerousActionCount
  9. `checkBruteForceSuccess()` — consecutiveFailedCount logic
  10. `checkDormantAccount()` — delta + burst logic

### Bước 4 — Integration (10 phút)
```cpp
// main.cpp, sau engine.buildIndices(store):
AnomalyDetector detector(store.stringPool.size());
detector.runAll(engine, store.stringPool);
detector.printReport(store.stringPool);
```

---

## 6. Lưu ý kiến trúc quan trọng

| Vấn đề | Giải pháp |
|---|---|
| `DeviceContext` Direct-Address Table | Dùng cùng `contextCapacity = stringPool.size()`. deviceId là Dictionary ID → index hợp lệ. |
| `extractHourUTC` timezone | Timestamp trong CSV là UTC. Công thức `(ts / 3600) % 24` là đủ. Nếu cần timezone VN (+7): `((ts + 25200) / 3600) % 24` |
| `checkResourceScan` | [FIX] Dùng Sliding Window (RingBuffer) thay Tumbling Window. Xóa `windowStartTimestamp`. Tránh boundary exploit. |
| Session không có LOGOUT | Khi gặp `EVENT_LOGIN` mới: nếu `hasActiveSession == true` → tự đóng session cũ → kiểm tra Long Session → mở session mới |
| Brute-force Success vs Brute Force | [FIX] Hai luật độc lập. Luật 1 dùng `failedLoginWindow` (sliding). Luật 10 dùng `consecutiveFailedCount` (reset khi gặp **bất kỳ** non-FAILED_LOGIN) |
| Dormant Account "burst" | [FIX] Trigger khi `burstEventCount >= 20` trong 10 phút sau thức dậy. Không trigger ngay event đầu tiên. |
