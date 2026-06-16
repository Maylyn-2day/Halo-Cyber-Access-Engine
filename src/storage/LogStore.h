// LogStore.h
#ifndef LOG_STORE_H
#define LOG_STORE_H

#include <cstdint>

#include "../core/DynamicArray.h"
#include "../core/LogChunk.h"
#include "../core/LogEntry.h"
#include "../core/StringPool.h"

/**
 * @brief Hệ thống lưu trữ trung tâm (Storage Engine) quản lý toàn bộ các
 * `LogChunk`.
 *
 * @note Về mặt kiến trúc, `LogStore` chịu trách nhiệm mở rộng dung lượng theo
 * chiều ngang (scaling) bằng cách cấp phát các `LogChunk` mới khi chunk hiện
 * tại bị đầy. Thiết kế này là dạng Block-based Allocation (cấp phát theo khối),
 * ngăn ngừa rủi ro phải sao chép toàn bộ dữ liệu cũ sang vùng nhớ mới
 * (reallocation overhead) mà các mảng động thông thường (`std::vector`) hay gặp
 * phải.
 */
class LogStore {
private:
  static const uint32_t DEFAULT_CHUNK_SIZE = 8192;

  DynamicArray<LogChunk *> chunks;
  LogChunk *currentChunk;
  uint64_t totalEntries;

  /**
   * @brief Cấp phát một Chunk mới và đưa vào mảng quản lý nội bộ.
   *
   * @return `true` nếu cấp phát thành công, `false` nếu tràn bộ nhớ hệ thống.
   *
   * @note Quá trình chuyển đổi (context switch) giữa các Chunk chỉ tốn chi phí
   * O(1) Amortized Time, diễn ra mỗi khi hệ thống nạp đủ `DEFAULT_CHUNK_SIZE`
   * bản ghi.
   */
  bool allocateChunk() {
    LogChunk *chunk = new LogChunk(DEFAULT_CHUNK_SIZE);

    chunks.pushBack(chunk);
    currentChunk = chunk;
    return true;
  }

public:
  StringPool stringPool;

  /**
   * @brief Khởi tạo Storage Engine với thông số mặc định.
   */
  LogStore() : chunks(8), currentChunk(nullptr), totalEntries(0) {}

  /**
   * @brief Khởi tạo Storage Engine với dự toán dung lượng trước.
   *
   * @param estimatedChunks Số lượng chunk ước tính cần thiết.
   *
   * @note Mảng con trỏ `chunks` sẽ cấp phát đủ bộ nhớ để chứa `estimatedChunks`
   * con trỏ ban đầu, giúp tối ưu chi phí mở rộng động của cấu trúc dữ liệu mảng
   * bên trong.
   */
  explicit LogStore(uint32_t estimatedChunks)
      : chunks(estimatedChunks), currentChunk(nullptr), totalEntries(0) {}

  /**
   * @brief Hủy hệ thống lưu trữ và giải phóng toàn bộ khối lượng dữ liệu khổng
   * lồ.
   *
   * @note `LogStore` đóng vai trò Quản lý Vòng đời (Owner) cho mọi cấu trúc
   * `LogChunk`. Hàm Destructor này có độ phức tạp O(M) với M là số lượng
   * Chunks. Bằng cách gọi `delete` trên từng `LogChunk`, hiệu ứng Domino sẽ
   * kích hoạt Destructor của Chunk, qua đó tự động dọn dẹp hàng triệu
   * `LogEntry` một cách sạch sẽ.
   */
  ~LogStore() {
    for (uint32_t i = 0; i < chunks.size(); ++i) {
      delete chunks[i];
      chunks[i] = nullptr;
    }

    currentChunk = nullptr;
    totalEntries = 0;
  }

  // Vô hiệu hóa việc sao chép để đảm bảo tính duy nhất và toàn vẹn của
  // Ownership.
  LogStore(const LogStore &other) = delete;
  LogStore &operator=(const LogStore &other) = delete;

  /**
   * @brief Đặt trước bộ nhớ cho cấu trúc chỉ mục mảng Chunk.
   *
   * @param estimatedChunks Tổng lượng chunk dự kiến.
   *
   * @note Hành động này không cấp phát ngay lập tức các vùng nhớ cho
   * `LogEntry`, mà chỉ điều chỉnh sức chứa (capacity) của mảng siêu dữ liệu
   * (metadata array) `chunks`. Đây là kỹ thuật Pre-allocation tiêu chuẩn để
   * tránh Resize Overhead.
   */
  void reserveChunks(uint32_t estimatedChunks) {
    chunks.reserve(estimatedChunks);
  }

  /**
   * @brief Nạp một đối tượng log mới vào hệ thống lưu trữ.
   *
   * @param entry Đối tượng `LogEntry` chứa dữ liệu thô.
   * @return Con trỏ vĩnh viễn (stable pointer) trỏ đến vị trí của log trong
   * Chunk hiện tại.
   *
   * @note Độ phức tạp Amortized O(1). Nếu `currentChunk` bị đầy, hệ thống sẽ tự
   * động gọi `allocateChunk()`. Đặc biệt, vì Engine dùng Block-based
   * Allocation, các con trỏ được trả về từ hàm này có Độ ổn định tuyệt đối
   * (Absolute Pointer Stability). Bộ nhớ sẽ không bao giờ bị dịch chuyển
   * (relocated), do đó các Index (như HashIndex) có thể tham chiếu an toàn bằng
   * con trỏ thô mà không sợ lỗi Invalidated Pointers.
   */
  LogEntry *insert(const LogEntry &entry) {
    if (currentChunk == nullptr || !currentChunk->hasSpace()) {
      if (!allocateChunk()) {
        return nullptr;
      }
    }

    LogEntry *stored = currentChunk->append(entry);

    if (stored != nullptr) {
      ++totalEntries;
    }

    return stored;
  }

  /**
   * @brief Trả về tổng số lượng bản ghi log đã được hệ thống lưu trữ.
   */
  uint64_t size() const { return totalEntries; }

  /**
   * @brief Trả về tổng số lượng Block (Chunk) hiện tại đang được cấp phát trên
   * Heap.
   */
  uint32_t chunkCount() const { return chunks.size(); }

  /**
   * @brief Truy cập trực tiếp một khối dữ liệu (Chunk) cụ thể thông qua chỉ
   * mục.
   *
   * @note Thiết kế này phục vụ kiến trúc Song song hóa (Multithreading/SIMD).
   * Các luồng phân tích (Worker Threads) có thể lấy từng Chunk độc lập qua hàm
   * này để xử lý map-reduce mà không gây tắc nghẽn khóa luồng (Lock
   * Contention).
   */
  LogChunk *getChunk(uint32_t index) { return chunks[index]; }

  /**
   * @brief Truy cập Read-only một Chunk thông qua chỉ mục.
   */
  const LogChunk *getChunk(uint32_t index) const { return chunks[index]; }

  /**
   * @brief Cho phép BinaryIO inject chunk đã populate sẵn khi load binary.
   * @param chunk Con trỏ tới LogChunk đã được fread data.
   * @param entryCount Số entries thực tế trong chunk.
   */
  void addLoadedChunk(LogChunk *chunk, uint32_t entryCount) {
    chunks.pushBack(chunk);
    currentChunk = chunk;
    totalEntries += entryCount;
  }
};

#endif
