# NHẬT KÝ SỬ DỤNG TRỢ LÝ AI (AI USAGE LOG)

> **Mục đích:** Tài liệu này liệt kê các câu lệnh (prompts) đã sử dụng với trợ lý AI (GitHub Copilot / Gemini) trong quá trình hoàn thiện Phase 1 nhằm đảm bảo tính minh bạch học thuật theo yêu cầu của Giảng viên.

---

## 1. Công cụ sử dụng

* **AI Models:** GitHub Copilot (tích hợp trong IDE Visual Studio 2026), Gemini.
* **Mục đích chính:** Hỗ trợ lập kế hoạch từ bản yêu cầu của giảng viên, tối ưu hóa cấu trúc thiết kế hệ thống của sinh viên, phân tích các cảnh báo biên dịch, tối ưu cú pháp C++ chuyên sâu và hỗ trợ định dạng tài liệu báo cáo kỹ thuật.
* **Quyền tác giả:** Hầu hết logic cốt lõi, thuật toán tự xây dựng và kiến trúc cấu trúc dữ liệu nền tảng (Zero STL containers) hoàn toàn do sinh viên tự nghiên cứu, thiết kế và lập trình độc lập.

---

## 2. Chi tiết các Prompts đã sử dụng

### 2.1. Thiết kế Kiến trúc và Lập kế hoạch Hệ thống
* **Ngữ cảnh:** Lập kế hoạch kiến trúc tổng thể cho Halo Engine dựa trên các ràng buộc nghiêm ngặt của đề bài (Không dùng thư viện STL, tối ưu hóa bộ nhớ RAM và CPU Cache Line, xử lý dữ liệu lớn).
* **Prompt đã dùng:**

```text
Act as a Principal Software Engineer specializing in High-Performance Systems. I need to design a custom log analytics engine in C++ called "Halo Engine" to process 1,000,000+ log rows. 

Here are the strict technical constraints:
- MUST USE: Custom structures, raw pointers, custom DynamicArray templates.
- STRICTLY FORBIDDEN: Any STL containers (std::vector, std::map, std::unordered_map, std::set).
- SCHEMA: user_id, device_id, app_id, resource_id, event_type (enum), location (enum), timestamp (int64_t).
- MEMORY MANAGEMENT: Zero memory leaks allowed, using RAII. Must handle invalid values and duplicates gracefully.

Please propose an architectural design document focusing on Phase 1 (Midterm):
1. A compact memory design for `LogEntry` to minimize padding.
2. A custom Dictionary Encoding structure (StringPool) using a separate-chaining Hash Table with djb2 algorithm to avoid string duplication.
3. An arena allocation strategy using contiguous memory blocks (LogChunk) to prevent heap fragmentation.
4. A custom stable sorting algorithm (Merge Sort) to sort timelines.
```


### 2.2. Khắc phục Cảnh báo Bộ nhớ và Tối ưu hóa CLI
* **Ngữ cảnh:** Khắc phục cảnh báo nghiêm trọng về tràn bộ nhớ Stack (C6262) trên trình biên dịch MSVC và tối ưu hóa tính linh hoạt, tự động hóa của hệ thống giao diện dòng lệnh.
* **Prompt đã dùng:**

```text
Act as a Senior C++ Developer assisting with code refinement and robustness improvements for my In-Memory Log Analytics Engine.

Please assist me with the following two refactoring tasks based on my codebase:
1. In my `DataLoader.cpp`, the compiler triggers Warning C6262 because `char readBuffer[256 * 1024]` and `char lineBuffer[4096]` occupy too much space on the Stack. Help me refactor this `load` function to allocate these buffers on the Heap using `new[]` and safely release them with `delete[]` before the function returns to guarantee zero memory leaks.
2. Update the logic of `parseInt64` to strictly reject negative numbers (return false if the first char is '-').
3. In `main.cpp`, instead of hardcoding default test values, implement a "Dynamic Quick-Test" logic: after loading the dataset, automatically fetch the first valid `LogEntry` from the `LogStore`, reverse-lookup its original string values from the `StringPool`, and use them as the default prompt inputs for the CLI.

Continue to strictly follow the "Zero STL containers" rule for the data structures themselves.
```

### 2.3. Hỗ trợ tổng hợp Báo cáo Kỹ thuật (Documentation)
* **Ngữ cảnh:** Tổng hợp toàn bộ quá trình phát triển, các giải pháp kỹ thuật và các lỗi hệ thống đã vượt qua trong Phase 1 thành một file báo cáo chuyên nghiệp chuẩn Markdown.
* **Prompt đã dùng:**

```text
Act as a Senior C++ Software Architect and an IT University Lecturer.

I have just completed Phase 1 (Midterm) of my C++ project named "Halo Engine" (an In-Memory Log Analytics Engine). 
Please analyze my current workspace (specifically all files in the `src/` directory).

Based on my actual code and development journey, write a comprehensive and highly technical Midterm Report in VIETNAMESE. The output should be formatted as a Markdown file, ready to be saved as `BAO_CAO.md`.

Please strictly follow this structure:
1. MỨC ĐỘ HOÀN THÀNH: Evaluate what the code currently achieves (reading 1M+ rows, zero-copy parsing, strict data validation, preventing duplicates).
2. THIẾT KẾ HỆ THỐNG & KỸ THUẬT TỐI ƯU: Deep dive into:
   - Data Alignment & Memory Packing in `LogEntry` (ordering from 8-byte to 1-byte types to minimize padding).
   - Dictionary Encoding in `StringPool`.
   - O(1) hashing with `DuplicateHashSet`.
   - Refactoring from Header-Only to `.h`/`.cpp` separation to prevent ODR violations and improve compile time.
3. CÁC KHÓ KHĂN ĐÃ VƯỢT QUA: Detail the extensive technical challenges faced during development:
   - Quản lý phiên bản & Git: Overcoming accidental code breakages by mastering Git time-travel commands (git restore, git reset --hard) to safely rollback and merge feature branches (fix/phase1-cleanup).
   - Cú pháp C++ & Memory Stack: Resolving the -Wreorder initializer warning in LogEntry, fixing constructor argument mismatches, and cleaning up redundant nullptr checks for modern new operators.
   - Tràn bộ nhớ & Static Analysis: Fixing the C6262 Stack Overflow warning in DataLoader::load by moving the 256KB buffer to the Heap (new[]), silencing the C4996 unsafe fopen macro, and learning to navigate false positive warnings (C6386 in DynamicArray).
   - Tính linh hoạt của hệ thống: Implementing a "Dynamic Quick-Test" in main.cpp and patching parseInt64 to strictly reject negative timestamps.
4. HƯỚNG PHÁT TRIỂN (PHASE 2): Briefly suggest future improvements (Binary Search, Memory-Mapped I/O, Anomaly Detection).

Use professional IT terminology in Vietnamese, tone should be academic, detailed, and directly referencing my actual codebase and the struggles of system programming.
```