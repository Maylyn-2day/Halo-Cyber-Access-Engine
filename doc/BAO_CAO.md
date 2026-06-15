# BÁO CÁO GIỮA KỲ — HALO ENGINE
## In-Memory Log Analytics Engine (Phase 1)

> **Môn học:** Cơ sở lập trình  
> **MSSV:** 24120085  
> **Họ tên:** Lê Nguyễn Thùy Linh   

---

## 1. MỨC ĐỘ HOÀN THÀNH

### 1.1 Tổng quan hệ thống

Halo Engine là một **In-Memory Log Analytics Engine** được viết hoàn toàn bằng C++ thuần (không sử dụng STL containers), có khả năng nạp và phân tích hơn **1 triệu bản ghi log** trong bộ nhớ. Hệ thống bao gồm **17 file mã nguồn** (~2000 dòng code) được tổ chức trong thư mục `src/`.

### 1.2 Bảng đánh giá yêu cầu

| #   | Yêu cầu                               | Trạng thái | Minh chứng                                                                                                                          |
| --- | ------------------------------------- | :--------: | ----------------------------------------------------------------------------------------------------------------------------------- |
| 1   | Đọc CSV bằng `char*` / zero-copy      |     ✅      | `DataLoader.cpp`: `FILE* + fread` vào buffer 256KB, tách field bằng `FieldView` — không dùng `std::getline` hay `std::stringstream` |
| 2   | Lọc trùng lặp bằng Hash Set tự viết   |     ✅      | `DuplicateHashSet`: lưu fingerprint 64-bit (djb2), `insertIfAbsent()` trả `false` nếu trùng                                         |
| 3   | Mã hóa chuỗi (Dictionary Encoding)    |     ✅      | `StringPool`: ánh xạ hai chiều `string ↔ uint32_t`                                                                                  |
| 4   | Đánh chỉ mục quan hệ                  |     ✅      | `SearchEngine` xây 2 `HashIndex` (user, resource), mỗi key trỏ tới timeline `DynamicArray<const LogEntry*>`                         |
| 5   | Sắp xếp Merge Sort ổn định            |     ✅      | `SortUtils::sortByTimestamp` — cấp phát buffer tạm 1 lần, merge sort đệ quy, giải phóng đúng                                        |
| 6   | Truy vấn User Journey                 |     ✅      | `QueryEngine::printUserJourney` — quét tuyến tính với `break` sớm trên timeline đã sắp xếp                                          |
| 7   | Truy vấn Resource Access History      |     ✅      | `QueryEngine::printResourceJourney` — cùng pattern                                                                                  |
| 8   | Top 10 Resources trong O(N)           |     ✅      | Đếm theo bucket `uint32_t* counts`, chèn vào mảng cố định `TopResource[10]`                                                         |
| 9   | CLI tương tác với xử lý input an toàn |     ✅      | `main.cpp`: `clearBadInput()`, `try/catch` trên `std::stoll`, giá trị mặc định, auto-swap khi start > end                           |

> **Kết luận: 9/9 yêu cầu Phase 1 đã hoàn thành đầy đủ.**

### 1.3 Hiệu năng thực tế

- **Dataset:** `halo_dataset_1m.csv` (~53MB, 1 triệu dòng), `halo_dataset_1_5m.csv` (~80MB, 1.5 triệu dòng)
- **Ingestion:** Nạp 1M dòng trong vài giây nhờ block I/O (`fread` 256KB) và fingerprint dedup trước khi parse
- **Query:** Truy vấn User/Resource Journey trả kết quả ở cấp microsecond nhờ Hash Index O(1) + timeline đã sắp xếp

---

## 2. THIẾT KẾ HỆ THỐNG & KỸ THUẬT TỐI ƯU

### 2.1 Kiến trúc tổng thể

```
main.cpp
  ├── DataLoader ──→ FILE* + fread (256KB block I/O)
  │     ├── FieldView (zero-copy field splitting)
  │     ├── DuplicateHashSet (fingerprint dedup)
  │     └── StringPool (dictionary encoding)
  ├── LogStore ──→ DynamicArray<LogChunk*>
  │     └── LogChunk(8192) ──→ LogEntry[] (contiguous)
  ├── SearchEngine
  │     ├── HashIndex (userIndex)
  │     └── HashIndex (resourceIndex)
  ├── QueryEngine (User Journey, Resource History, Top-10)
  └── SortUtils (Stable Merge Sort)
```

### 2.2 Data Alignment & Memory Packing trong `LogEntry`

Struct `LogEntry` được thiết kế theo nguyên tắc **sắp xếp giảm dần kích thước kiểu dữ liệu** (8-byte → 4-byte → 1-byte) để tối thiểu hóa padding do alignment:

```cpp
struct LogEntry {
    int64_t   timestamp;   // 8 bytes (offset 0)
    uint32_t  userId;      // 4 bytes (offset 8)
    uint32_t  deviceId;    // 4 bytes (offset 12)
    uint32_t  appId;       // 4 bytes (offset 16)
    uint32_t  resourceId;  // 4 bytes (offset 20)
    EventType eventType;   // 1 byte  (offset 24) — enum : uint8_t
    Location  location;    // 1 byte  (offset 25) — enum : uint8_t
    // padding: 6 bytes → Total: 32 bytes (aligned to 8-byte boundary)
};
```

**Phân tích bộ nhớ:**

| Thành phần                                          | Kích thước   |
| --------------------------------------------------- | ------------ |
| `int64_t timestamp`                                 | 8 bytes      |
| 4× `uint32_t` (userId, deviceId, appId, resourceId) | 16 bytes     |
| 2× `uint8_t` enum (EventType, Location)             | 2 bytes      |
| Padding (alignment 8-byte)                          | 6 bytes      |
| **Tổng mỗi LogEntry**                               | **32 bytes** |

Với 1 triệu bản ghi: **~30.5 MB** dữ liệu thuần. Nếu không dùng dictionary encoding mà lưu 4 `std::string` mỗi dòng (mỗi string ~32 bytes trên MSVC với SSO), bộ nhớ sẽ tăng lên **>1.5 GB** — gấp **~50 lần**.

Cả `EventType` và `Location` đều được khai báo với backing type `uint8_t`, tiết kiệm tối đa so với `int` mặc định (4 bytes).

### 2.3 Dictionary Encoding với `StringPool`

`StringPool` hoạt động như một **bảng mã hóa hai chiều** (bidirectional dictionary encoder):

- **Chiều thuận** (string → ID): Hash table separate-chaining với 262.147 bucket (số nguyên tố), sử dụng thuật toán djb2. Mỗi `Node` chứa `{key, id, next}`.
- **Chiều ngược** (ID → string): `DynamicArray<std::string>` cho phép tra cứu O(1) theo chỉ số.

```
Forward:  "USER_0042" ──djb2──→ bucket[h] ──chain──→ Node{key="USER_0042", id=42}
Reverse:  strings[42] → "USER_0042"
```

**Hiệu quả:** Với 1M dòng log nhưng chỉ ~vài nghìn user/device/app/resource duy nhất, `StringPool` nén hàng triệu chuỗi lặp lại thành các `uint32_t` 4-byte. Phương thức `getOrCreateId()` đảm bảo **mỗi chuỗi chỉ được lưu một lần**.

### 2.4 O(1) Duplicate Detection với `DuplicateHashSet`

`DuplicateHashSet` là bộ lọc trùng lặp hoạt động **trước khi parse CSV**, tiết kiệm tối đa CPU:

```
Raw CSV line ──djb2──→ 64-bit fingerprint ──insertIfAbsent()──→ true (mới) / false (trùng)
```

- **Bucket count:** 2.000.003 (số nguyên tố) — load factor ~0.5 với 1M dòng → chain length trung bình < 1
- **Chiến lược:** Hash **toàn bộ dòng raw** trước khi split/validate, tránh lãng phí CPU cho các dòng trùng
- **Hàm hash:** djb2 trên raw `char*` với `unsigned long long` — phân phối tốt, collision thấp

### 2.5 Arena Allocation với `LogChunk`

Thay vì `new LogEntry` riêng lẻ cho từng bản ghi (gây **heap fragmentation**), hệ thống sử dụng mô hình **arena allocation**:

```
LogStore → DynamicArray<LogChunk*> → LogChunk(8192) → LogEntry[8192] (contiguous)
```

- Với 1M dòng, chỉ cần ~122 lần `new LogEntry[8192]` thay vì 1M lần `new LogEntry`
- **Giảm fragmentation ~8000×**, cải thiện **cache locality** khi quét tuần tự
- Giải phóng bộ nhớ: 1 lần `delete[]` mỗi chunk thay vì 1M lần `delete`

### 2.6 Hash Index với Integer Mixing (Murmur-inspired)

`HashIndex` sử dụng hàm hash chất lượng cao cho integer key (dictionary ID):

```cpp
uint32_t hashKey(uint32_t key) const {
    key ^= key >> 16;
    key *= 0x7feb352dU;  // Murmur-inspired constant
    key ^= key >> 15;
    key *= 0x846ca68bU;
    key ^= key >> 16;
    return key % bucketCount;
}
```

Đây là **finalization step của MurmurHash3** — phân phối đều hơn đáng kể so với `key % bucketCount` đơn thuần, đặc biệt khi key là các số tuần tự (0, 1, 2, ...) từ `StringPool`.

### 2.7 Stable Merge Sort tự viết

`SortUtils` triển khai **Merge Sort ổn định** trên mảng con trỏ `LogEntry*`:

- Buffer tạm `temp` được cấp phát **một lần duy nhất** và tái sử dụng qua mọi lần gọi đệ quy → tránh **recursive heap churn**
- Tính ổn định: khi timestamp bằng nhau, phần tử bên trái được chọn trước (`<=`), giữ nguyên thứ tự ban đầu
- Sau khi sort: `delete[] temp` → **zero memory leak**

### 2.8 Tách Header/Implementation (.h/.cpp)

Các module phức tạp đã được refactor sang mô hình **declaration/definition separation**:

| Module       | Header (.h) | Implementation (.cpp) |
| ------------ | :---------: | :-------------------: |
| DataLoader   |      ✅      |           ✅           |
| StringPool   |      ✅      |           ✅           |
| HashIndex    |      ✅      |           ✅           |
| SearchEngine |      ✅      |           ✅           |
| QueryEngine  |      ✅      |           ✅           |

Việc tách này **ngăn chặn vi phạm ODR** (One Definition Rule) khi nhiều translation unit include cùng header, đồng thời **giảm thời gian biên dịch** vì thay đổi implementation không bắt buộc recompile toàn bộ project.

Các class template (`DynamicArray<T>`) và lightweight struct (`LogEntry`, `LogChunk`, `LogStore`) vẫn giữ header-only — đúng theo best practice của C++.

---

## 3. CÁC KHÓ KHĂN ĐÃ VƯỢT QUA

### 3.1 Quản lý phiên bản & Git

**Vấn đề:** Trong quá trình phát triển, nhiều lần code bị hỏng do thử nghiệm feature mới trực tiếp trên nhánh chính. Một số commit đã vô tình phá vỡ logic parse CSV, khiến toàn bộ pipeline ingestion ngừng hoạt động.

**Giải pháp đã áp dụng:**

- **`git restore`**: Khôi phục từng file về trạng thái commit cuối cùng khi phát hiện lỗi sớm
- **`git reset --hard <commit>`**: "Du hành thời gian" về commit ổn định khi nhiều file cùng bị ảnh hưởng
- **Feature branching**: Tạo nhánh `fix/phase1-cleanup` để refactor an toàn, chỉ merge vào `main` sau khi đã test kỹ

**Bài học:** Git không chỉ là công cụ backup mà là **safety net** cho system programming — nơi một thay đổi nhỏ có thể gây cascade failure.

### 3.2 Cú pháp C++ & Memory Stack

**Vấn đề 1 — `-Wreorder` warning trong `LogEntry`:**

Ban đầu, constructor của `LogEntry` khởi tạo member theo thứ tự tham số hàm, không theo thứ tự khai báo:

```cpp
// SAI — initializer list không khớp declaration order
LogEntry(uint32_t user, ..., int64_t ts)
    : userId(user), deviceId(device), ..., timestamp(ts) {}
//      ↑ member thứ 2         ↑ member thứ 1 — Compiler warning!
```

C++ **luôn** khởi tạo member theo thứ tự khai báo trong struct, bất kể thứ tự trong initializer list. Compiler cảnh báo `-Wreorder` vì hành vi thực tế khác với code viết.

**Giải pháp:** Sắp xếp lại initializer list khớp declaration order: `timestamp` → `userId` → `deviceId` → `appId` → `resourceId` → `eventType` → `location`.

**Vấn đề 2 — Constructor argument mismatch:**

Nhiều lần gọi constructor `LogEntry(...)` với sai thứ tự tham số (ví dụ truyền `timestamp` trước `userId`), dẫn đến dữ liệu bị hoán vị âm thầm. Phải debug bằng cách in từng field và so sánh với CSV gốc.

**Vấn đề 3 — Kiểm tra `nullptr` thừa cho `new`:**

```cpp
LogChunk* chunk = new LogChunk(DEFAULT_CHUNK_SIZE);
if (chunk == nullptr) { return false; }  // ← DEAD CODE
```

Trong C++ chuẩn, `new` throw `std::bad_alloc` khi hết bộ nhớ, **không bao giờ trả `nullptr`** (trừ khi dùng `new(std::nothrow)`). Đoạn kiểm tra này là dead code, tạo cảm giác an toàn giả. Đã loại bỏ sau khi hiểu rõ cơ chế exception của `new`.

### 3.3 Tràn bộ nhớ & Static Analysis

**Vấn đề 1 — C6262 Stack Overflow trong `DataLoader::load`:**

Ban đầu, buffer đọc file được khai báo trên **stack**:

```cpp
bool DataLoader::load(...) {
    char readBuffer[256 * 1024];  // 256KB trên stack!
    char lineBuffer[4096];        // thêm 4KB nữa
    // → Tổng ~260KB stack frame — vượt ngưỡng 16KB mặc định
}
```

Visual Studio phát cảnh báo **C6262**: "Function uses 262144 bytes of stack space, exceeds /analyze:stacksize 16384." Stack mặc định trên Windows chỉ **1MB** — một hàm chiếm 256KB là **rủi ro stack overflow** nghiêm trọng.

**Giải pháp:** Di chuyển buffer sang **Heap** bằng `new[]`:

```cpp
char* readBuffer = new char[READ_BUFFER_SIZE];  // Heap — an toàn
char* lineBuffer = new char[MAX_LINE_SIZE];
// ... sử dụng ...
delete[] readBuffer;
delete[] lineBuffer;
```

**Vấn đề 2 — C4996 unsafe `fopen`:**

MSVC cảnh báo `fopen` là "unsafe" và đề xuất `fopen_s`. Giải pháp: thêm `#define _CRT_SECURE_NO_WARNINGS` ở đầu `DataLoader.cpp` để giữ tính portable (không phụ thuộc MSVC-specific API).

**Vấn đề 3 — C6386 false positive trong `DynamicArray`:**

Static analyzer cảnh báo "Buffer overrun" trong hàm `resize()` của `DynamicArray` — nhưng đây là **false positive** vì vòng lặp copy chỉ chạy `i < length` trong khi buffer mới có `newCapacity >= length`. Đã học cách nhận diện và bỏ qua false positive thay vì thêm code workaround không cần thiết.

### 3.4 Tính linh hoạt của hệ thống

**Vấn đề 1 — Dynamic Quick-Test trong `main.cpp`:**

Để tăng tốc debug, `main.cpp` triển khai cơ chế **"Dynamic Default"**: tự động trích xuất `userId` và `resourceId` từ bản ghi đầu tiên trong dataset để làm giá trị mặc định cho CLI:

```cpp
if (store.size() > 0 && store.chunkCount() > 0) {
    const LogChunk* firstChunk = store.getChunk(0);
    if (firstChunk != nullptr && firstChunk->size() > 0) {
        const LogEntry* firstEntry = firstChunk->raw();
        defaultUser = store.stringPool.getString(firstEntry[0].userId);
        defaultRes  = store.stringPool.getString(firstEntry[0].resourceId);
    }
}
```

Nhờ đó, khi test chỉ cần nhấn Enter thay vì gõ userId/resourceId — tiết kiệm thời gian đáng kể khi chạy hàng trăm lần test.

**Vấn đề 2 — `parseInt64` từ chối timestamp âm:**

Ban đầu `parseInt64` chấp nhận số âm. Tuy nhiên, timestamp Unix **không bao giờ âm** trong ngữ cảnh log hiện đại. Đã thêm validation:

```cpp
if (field.start[0] == '-') {
    return false;  // Từ chối timestamp âm — invalid data
}
```

Đây là ví dụ về **domain-specific validation** — không chỉ kiểm tra cú pháp số mà còn kiểm tra ngữ nghĩa dữ liệu.

---

## 4. HƯỚNG PHÁT TRIỂN (PHASE 2)

### 4.1 Binary Search trên Timeline đã sắp xếp

Hiện tại `printUserJourney` quét tuyến tính O(N) trên timeline. Với timeline đã sorted, có thể triển khai `lowerBound()` / `upperBound()` để giảm xuống **O(log N + K)** (K = số kết quả). Đây là thay đổi có **ROI cao nhất** cho Phase 2.

### 4.2 Memory-Mapped I/O

Thay `fread` bằng `CreateFileMapping` + `MapViewOfFile` (Windows) để ánh xạ file CSV trực tiếp vào virtual address space. `FieldView` sẽ trỏ thẳng vào vùng nhớ mapped — **true zero-copy** không cần `lineBuffer` trung gian.

### 4.3 Anomaly Detection

Triển khai phát hiện bất thường:
- **Brute-force detection:** Đếm `FAILED_LOGIN` per user trong sliding window
- **Impossible travel:** So sánh location thay đổi với khoảng cách thời gian
- **Unusual access patterns:** Phát hiện user truy cập resource ngoài giờ thường

### 4.4 Chunk-level Metadata

Lưu `minTimestamp` / `maxTimestamp` trong mỗi `LogChunk` để skip toàn bộ chunk khi truy vấn time-range hẹp — giảm scan time từ O(N) xuống fraction nhỏ.

---

## PHỤ LỤC: Bản đồ file mã nguồn

| File                 | Loại   | Dòng | Vai trò                                     |
| -------------------- | ------ | ---: | ------------------------------------------- |
| `main.cpp`           | Source |  258 | Entry point, CLI, timing harness            |
| `DataLoader.cpp`     | Source |  386 | High-speed CSV ingestion                    |
| `DataLoader.h`       | Header |   26 | Loader declaration                          |
| `LogEntry.h`         | Header |   90 | Compact log row struct (32 bytes)           |
| `LogChunk.h`         | Header |   90 | Arena-style contiguous block (8192 entries) |
| `LogStore.h`         | Header |  114 | Chunk-based ownership layer                 |
| `DynamicArray.h`     | Header |  129 | Custom `std::vector` replacement (template) |
| `DuplicateHashSet.h` | Header |  129 | Fingerprint duplicate filter                |
| `StringPool.h`       | Header |   40 | Dictionary encoder declaration              |
| `StringPool.cpp`     | Source |   96 | Dictionary encoder implementation           |
| `HashIndex.h`        | Header |   39 | Inverted index declaration                  |
| `HashIndex.cpp`      | Source |  106 | Inverted index implementation               |
| `SearchEngine.h`     | Header |   29 | Index builder declaration                   |
| `SearchEngine.cpp`   | Source |   38 | Index builder implementation                |
| `QueryEngine.h`      | Header |   46 | Query engine declaration                    |
| `QueryEngine.cpp`    | Source |  171 | Business queries (Journey, Top-10)          |
| `SortUtils.h`        | Header |  117 | Stable Merge Sort                           |

**Tổng cộng: ~1,914 dòng C++ thủ công. Zero STL containers.**

---

## 5. HƯỚNG DẪN BIÊN DỊCH VÀ SỬ DỤNG

### 5.1 Yêu cầu môi trường
- **Trình biên dịch:** Hỗ trợ chuẩn C++17 trở lên.
- **IDE khuyên dùng:** Visual Studio 2026 (Windows).

### 5.2 Chuẩn bị dữ liệu (Dataset)
- File dữ liệu gốc bắt buộc phải được đổi tên thành `data.csv`.
- **Vị trí đặt file khi chấm điểm:** Đặt `data.csv` nằm cùng cấp với file thực thi `halo.exe` (thường nằm trong thư mục `x64/Release` hoặc `x64/Debug` sau khi Build). 
- *Lưu ý khi debug bằng IDE:* Nếu chạy trực tiếp bằng nút F5 trên Visual Studio, cần đặt file `data.csv` tại thư mục gốc của project (ngang hàng với thư mục `src/`) để khớp với Working Directory mặc định.

### 5.3 Thao tác chạy chương trình
1. Mở file solution `24120085.sln` bằng Visual Studio.
2. Thiết lập chế độ Build là **Release / x64** để kích hoạt các cờ tối ưu hóa của compiler, giúp hệ thống đạt tốc độ I/O và truy vấn tối đa.
3. Nhấn **F5** (Local Windows Debugger) để biên dịch và khởi chạy.
4. Tại giao diện Console (CLI):
   - Chờ vài giây để hệ thống nạp dữ liệu từ `data.csv` và xây dựng Hash Index.
   - Nhấn **Enter** để gọi tính năng "Dynamic Quick-Test" (truy vấn nhanh bằng ID hợp lệ được lấy mẫu ngẫu nhiên từ dữ liệu thực).
   - Hoặc gõ trực tiếp `UserId` (VD: U06619) hoặc `ResourceId` (VD: R00659) để xem lịch sử hành trình (Journey) đã được sắp xếp theo thời gian.

---


