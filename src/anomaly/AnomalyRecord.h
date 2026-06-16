#ifndef ANOMALY_RECORD_H
#define ANOMALY_RECORD_H

#include <cstdint>

enum class AnomalyType : uint8_t {
  BRUTE_FORCE = 0,
  DEVICE_HOPPING = 1,
  RESOURCE_SCAN = 2,
  OUT_OF_HOURS = 3,
  IMPOSSIBLE_TRAVEL = 4,
  MULTI_COUNTRY_HOPPING = 5,
  LONG_SESSION = 6,
  DANGER_CHAIN = 7,
  RAPID_SESSION = 8,
  BRUTE_FORCE_SUCCESS = 9,
  DORMANT_ACCOUNT = 10,
};

/**
 * @brief POD struct đại diện cho một anomaly phát hiện được.
 * Tổng kích thước: 24 bytes (Data alignment 8-byte boundaries).
 * Tuyệt đối không chứa con trỏ hoặc bộ nhớ heap.
 */
struct AnomalyRecord {
  int64_t timestamp; // 8 bytes: Thời điểm phát hiện
  uint32_t userId;   // 4 bytes: Dictionary ID của User
  uint32_t deviceId; // 4 bytes: Dictionary ID của Device (nếu có, không thì 0)
  AnomalyType type;  // 1 byte : Loại cảnh báo
  uint8_t _pad[3];   // 3 bytes: Padding lấp đầy khoảng trống (tránh rác bộ nhớ)

  // Khởi tạo mặc định
  AnomalyRecord()
      : timestamp(0), userId(0), deviceId(0), type(AnomalyType::BRUTE_FORCE) {
    _pad[0] = _pad[1] = _pad[2] = 0; // Xóa sạch rác
  }
};

#endif // ANOMALY_RECORD_H