// LogChunk.h
#ifndef LOG_CHUNK_H
#define LOG_CHUNK_H

#include <cstdint>

#include "LogEntry.h"

/**
 * @brief Cấu trúc quản lý một khối bộ nhớ liền kề (Contiguous Block) dành riêng
 * cho các `LogEntry`.
 *
 * @note Đây là thành phần lõi của cơ chế Arena Allocation. Thay vì cấp phát
 * từng đối tượng `LogEntry` bằng toán tử `new` rời rạc, Engine sẽ cấp phát một
 * "Chunk" lớn chứa hàng ngàn phần tử. Kiến trúc này giải quyết triệt để vấn đề
 * phân mảnh bộ nhớ Heap (Heap Fragmentation) và tăng cường tối đa tính địa
 * phương của dữ liệu (Cache Locality) trong quá trình quét tuần tự (Table Scan)
 * và sắp xếp.
 */
class LogChunk {
private:
  LogEntry *entries;
  uint32_t capacity;
  uint32_t count;

public:
  /**
   * @brief Khởi tạo một Chunk mới với sức chứa (capacity) được chỉ định.
   *
   * @param chunkCapacity Số lượng đối tượng `LogEntry` tối đa mà Chunk này có
   * thể chứa.
   *
   * @note Gọi trực tiếp toán tử `new[]` để cấp phát nguyên một mảng `LogEntry`
   * liền kề trong O(1) (thời gian thực thi có thể thay đổi nhẹ tùy hệ điều
   * hành). Do `LogEntry` có Constructor rỗng, chi phí khởi tạo sẽ ở mức thấp
   * nhất.
   */
  explicit LogChunk(uint32_t chunkCapacity)
      : entries(nullptr), capacity(chunkCapacity), count(0) {
    if (capacity > 0) {
      entries = new LogEntry[capacity];
    }
  }

  /**
   * @brief Hủy Chunk và giải phóng toàn bộ khối bộ nhớ liền kề.
   *
   * @note Việc gọi `delete[] entries` đảm bảo bộ nhớ được trao trả lại cho hệ
   * điều hành. Do tập trung giải phóng hàng ngàn đối tượng cùng một lúc, chi
   * phí quản lý Heap được giảm bớt đáng kể so với việc gọi `delete` hàng ngàn
   * lần.
   */
  ~LogChunk() {
    delete[] entries;
    entries = nullptr;
    capacity = 0;
    count = 0;
  }

  /**
   * @brief Vô hiệu hóa sao chép (Copying) đối với cấu trúc bộ nhớ thô.
   *
   * @warning Do class này sở hữu độc quyền (owns) bộ nhớ Heap thông qua con trỏ
   * thô, việc cho phép sử dụng Copy Constructor hoặc toán tử gán mặc định sẽ
   * dẫn đến lỗi giải phóng bộ nhớ kép (Double-Free Bug) hoặc hỏng Heap.
   */
  LogChunk(const LogChunk &other) = delete;
  LogChunk &operator=(const LogChunk &other) = delete;

  /**
   * @brief Kiểm tra xem Chunk hiện tại có còn sức chứa hay không.
   *
   * @note Hàm inline với chi phí gọi là O(1).
   */
  bool hasSpace() const { return count < capacity; }

  /**
   * @brief Gắn thêm (append) một đối tượng `LogEntry` vào vị trí trống tiếp
   * theo trong Chunk.
   *
   * @param entry Bản ghi log đầu vào (sẽ được copy dữ liệu vào Chunk).
   * @return Con trỏ trỏ đến vị trí lưu trữ thực tế của bản ghi bên trong Chunk,
   * hoặc `nullptr` nếu Chunk đã đầy.
   *
   * @note Độ phức tạp O(1). Đây là thao tác Placement/Copy siêu tốc. Con trỏ
   * trả về là địa chỉ bộ nhớ ổn định, sẽ được dùng để xây dựng cấu trúc Index
   * mà không bị mất hiệu lực.
   */
  LogEntry *append(const LogEntry &entry) {
    if (!hasSpace()) {
      return nullptr;
    }

    entries[count] = entry;
    LogEntry *stored = &entries[count];
    ++count;

    return stored;
  }

  /**
   * @brief Truy cập mảng thô (Raw Array) phục vụ quét tuần tự (Sequential
   * Scan).
   * @return Con trỏ tới phần tử đầu tiên của mảng `LogEntry`.
   */
  LogEntry *raw() { return entries; }

  /**
   * @brief Truy cập mảng thô (Read-only).
   */
  const LogEntry *raw() const { return entries; }

  /**
   * @brief Lấy số lượng bản ghi hiện có trong Chunk.
   */
  uint32_t size() const { return count; }

  /**
   * @brief Lấy sức chứa tối đa của Chunk.
   */
  uint32_t getCapacity() const { return capacity; }
};

#endif
