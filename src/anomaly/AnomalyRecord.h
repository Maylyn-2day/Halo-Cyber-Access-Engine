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
  DATA_EXFILTRATION = 11,
  COMPROMISED_DEVICE = 12,
  LATERAL_MOVEMENT = 13,
};

/**
 * @brief POD struct representing a detected anomaly.
 * Total size: 24 bytes (Data alignment 8-byte boundaries).
 * Strictly contains no pointers or heap memory.
 */
struct AnomalyRecord {
  int64_t timestamp; // 8 bytes: Time of detection
  uint32_t userId;   // 4 bytes: User's Dictionary ID
  uint32_t deviceId; // 4 bytes: Device's Dictionary ID (if any, otherwise 0)
  AnomalyType type;  // 1 byte : Alert type
  uint8_t _pad[3];   // 3 bytes: Padding to fill gaps (prevents memory garbage)

  // Default constructor
  AnomalyRecord()
      : timestamp(0), userId(0), deviceId(0), type(AnomalyType::BRUTE_FORCE) {
    _pad[0] = _pad[1] = _pad[2] = 0; // Clear garbage
  }
};

#endif