// BinaryIO.h
#ifndef BINARY_IO_H
#define BINARY_IO_H

#include <cstdint>
#include <string>

class LogStore;

/**
 * @brief Module đọc/ghi nhị phân (Binary Serialization) cho trạng thái
 * pre-processed của Halo Engine.
 *
 * Quyết định kiến trúc: Thay vì parse CSV mỗi lần khởi động (~2-5 giây
 * cho 10M rows), BinaryIO cho phép "chụp ảnh" (snapshot) toàn bộ LogStore
 * + StringPool xuống file nhị phân. Ở lần chạy tiếp theo, hệ thống đọc
 * thẳng khối byte vào RAM bằng fread — tốc độ chỉ bị giới hạn bởi băng
 * thông ổ cứng (~50ms trên SSD). LogEntry là POD struct 32 bytes nên có
 * thể fwrite/fread nguyên khối mà không cần serialize từng field.
 */
class BinaryIO {
public:
  /**
   * @brief Dump toàn bộ LogStore + StringPool ra file nhị phân.
   *
   * Format: Header (32 bytes) → StringPool strings → LogChunk entries
   *
   * @param filepath Đường dẫn file output (VD: "halo_db.bin").
   * @param store LogStore chứa dữ liệu đã xử lý.
   * @param csvPath Đường dẫn file CSV gốc (lưu vào header để detect stale).
   * @return true nếu ghi thành công.
   */
  static bool dump(const char *filepath, const LogStore &store,
                   const std::string &csvPath);

  /**
   * @brief Load file nhị phân ngược lại vào LogStore + StringPool.
   *
   * Luồng: Validate header → Restore StringPool → Restore LogChunks
   *
   * @param filepath Đường dẫn file binary.
   * @param store LogStore rỗng để inject data vào.
   * @return true nếu load thành công (magic + version + checksum hợp lệ).
   */
  static bool load(const char *filepath, LogStore &store);

  /**
   * @brief Kiểm tra xem file binary có còn hợp lệ so với file CSV gốc.
   *
   * So sánh file size và modification time của CSV lúc dump vs hiện tại.
   *
   * @param binaryPath Đường dẫn file binary.
   * @param csvPath Đường dẫn file CSV hiện tại.
   * @return true nếu binary vẫn tươi (fresh), false nếu stale.
   */
  static bool isValid(const char *binaryPath, const std::string &csvPath);
};

#endif
