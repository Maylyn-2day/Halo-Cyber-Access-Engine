// LogEntry.h
#ifndef LOG_ENTRY_H
#define LOG_ENTRY_H

#include <cstdint>

/**
 * @brief Kiểu dữ liệu liệt kê (enum) lưu trữ các loại sự kiện hệ thống.
 *
 * @note Về mặt kiến trúc, `EventType` được kế thừa từ kiểu `uint8_t` (1 byte)
 * nhằm mục đích tối ưu hóa dung lượng bộ nhớ cho hệ thống phân tích log
 * In-Memory. Với khối lượng dữ liệu khổng lồ, việc giảm kích thước từ `int` (4
 * bytes) xuống 1 byte giúp tiết kiệm đáng kể không gian lưu trữ và tăng cường
 * mật độ dữ liệu trên CPU Cache (Cache line packing), từ đó cải thiện hiệu suất
 * truy xuất ở mức O(1).
 */
enum EventType : uint8_t {
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

/**
 * @brief Kiểu dữ liệu liệt kê (enum) đại diện cho vị trí địa lý của sự kiện.
 *
 * @note Giống như `EventType`, `Location` cũng được ràng buộc với kích thước
 * `uint8_t`. Quyết định này phục vụ cho kỹ thuật data packing trong `LogEntry`,
 * đảm bảo rằng chi phí lưu trữ cho trường vị trí luôn đạt mức tối thiểu (đúng 1
 * byte) thay vì bị dư thừa do memory alignment của trình biên dịch.
 */
enum Location : uint8_t {
  LOC_US = 0,
  LOC_VN,
  LOC_JP,
  LOC_KR,
  LOC_SG,
  LOC_CN,
  LOC_DE,
  LOC_FR,
  LOC_UK,
  LOC_AU,
  LOC_CA,
  LOC_IN,
  LOC_BR,
  LOC_RU,
  LOC_TH,
  LOC_INVALID = 255
};

/**
 * @brief Chuyển đổi enum EventType thành chuỗi hiển thị.
 */
inline const char *eventTypeToString(EventType type) {
  switch (type) {
  case EVENT_LOGIN:
    return "LOGIN";
  case EVENT_LOGOUT:
    return "LOGOUT";
  case EVENT_TOKEN_REFRESH:
    return "TOKEN_REFRESH";
  case EVENT_ACCESS:
    return "ACCESS";
  case EVENT_FAILED_LOGIN:
    return "FAILED_LOGIN";
  case EVENT_OPEN_APP:
    return "OPEN_APP";
  case EVENT_DOWNLOAD:
    return "DOWNLOAD";
  case EVENT_ADMIN_ACTION:
    return "ADMIN_ACTION";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief Chuyển đổi enum Location thành chuỗi hiển thị.
 */
inline const char *locationToString(Location loc) {
  switch (loc) {
  case LOC_US:
    return "US";
  case LOC_VN:
    return "VN";
  case LOC_JP:
    return "JP";
  case LOC_KR:
    return "KR";
  case LOC_SG:
    return "SG";
  case LOC_CN:
    return "CN";
  case LOC_DE:
    return "DE";
  case LOC_FR:
    return "FR";
  case LOC_UK:
    return "UK";
  case LOC_AU:
    return "AU";
  case LOC_CA:
    return "CA";
  case LOC_IN:
    return "IN";
  case LOC_BR:
    return "BR";
  case LOC_RU:
    return "RU";
  case LOC_TH:
    return "TH";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief Cấu trúc dữ liệu siêu nhẹ (ultra-lightweight) đại diện cho một bản ghi
 * log nguyên thủy.
 *
 * @note Để đạt được hiệu năng tối đa và tuân thủ nguyên tắc Zero STL (tránh chi
 * phí cấp phát bộ nhớ động của std::string), toàn bộ các trường chuỗi được mã
 * hóa thông qua Dictionary Encoding ở bên ngoài cấu trúc. `LogEntry` chỉ lưu
 * trữ các mã định danh số học (numeric IDs), các enum nén và dữ liệu thời gian.
 *
 * Về mặt căn chỉnh bộ nhớ (Memory Alignment): Kích thước tổng các trường dữ
 * liệu là 26 bytes. Do trường `timestamp` kiểu `int64_t` yêu cầu căn chỉnh
 * 8-byte, trình biên dịch có thể thêm padding bytes, đẩy tổng kích thước struct
 * lên 32 bytes (chuẩn hóa trên hệ thống 64-bit) nhằm tối ưu hóa tốc độ nạp dữ
 * liệu từ RAM lên thanh ghi CPU, đảm bảo thời gian truy cập là O(1).
 */
struct LogEntry {
  int64_t timestamp;
  uint32_t userId;
  uint32_t deviceId;
  uint32_t appId;
  uint32_t resourceId;
  EventType eventType;
  Location location;

  /**
   * @brief Khởi tạo một đối tượng `LogEntry` rỗng với các giá trị mặc định.
   *
   * @note Độ phức tạp thời gian: O(1). Việc gán các giá trị thành 0 hoặc
   * `INVALID` rất quan trọng khi cấp phát bộ nhớ thô thông qua Arena
   * Allocation, giúp hệ thống không đọc phải các dữ liệu rác (garbage data) còn
   * sót lại trong vùng nhớ tái sử dụng.
   */
  LogEntry()
      : timestamp(0), userId(0), deviceId(0), appId(0), resourceId(0),
        eventType(EVENT_INVALID), location(LOC_INVALID) {}

  /**
   * @brief Khởi tạo một đối tượng `LogEntry` với dữ liệu đầy đủ.
   *
   * @param user Mã định danh số (Dictionary ID) của người dùng.
   * @param device Mã định danh số (Dictionary ID) của thiết bị.
   * @param app Mã định danh số (Dictionary ID) của ứng dụng.
   * @param resource Mã định danh số (Dictionary ID) của tài nguyên.
   * @param event Loại sự kiện hệ thống (kích thước nén 1 byte).
   * @param loc Vị trí địa lý (kích thước nén 1 byte).
   * @param ts Dấu thời gian (timestamp) của sự kiện.
   *
   * @note Độ phức tạp thời gian: O(1). Hàm tạo này phục vụ giai đoạn phân tích
   * dữ liệu log (parsing). Việc truyền dữ liệu trực tiếp vào danh sách khởi tạo
   * (initializer list) hạn chế tối đa các bản sao trung gian, thích hợp cho
   * việc khởi tạo hàng loạt thông qua Placement New kết hợp với Arena
   * Allocator.
   *
   * @warning Nếu đối tượng này được khởi tạo trên heap thông qua các con trỏ
   * thô (raw pointers) và toán tử `new[]`, Caller phải tự chịu trách nhiệm quản
   * lý vòng đời và gọi `delete[]` để giải phóng bộ nhớ. Do thiết kế giới hạn
   * nghiêm ngặt (Zero STL), cấu trúc này hoàn toàn không triển khai RAII hay
   * Destructor ảo, mọi sơ suất sẽ trực tiếp dẫn đến rò rỉ bộ nhớ (memory leak).
   */
  LogEntry(uint32_t user, uint32_t device, uint32_t app, uint32_t resource,
           EventType event, Location loc, int64_t ts)
      : timestamp(ts), userId(user), deviceId(device), appId(app),
        resourceId(resource), eventType(event), location(loc) {}
};

#endif
