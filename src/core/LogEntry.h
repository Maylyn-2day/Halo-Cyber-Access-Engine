// LogEntry.h
#ifndef LOG_ENTRY_H
#define LOG_ENTRY_H

#include <cstdint>

/**
 * @brief Enumeration data type (enum) storing system event types.
 *
 * @note Architecturally, `EventType` inherits from `uint8_t` (1 byte)
 * to optimize memory capacity for the In-Memory log analysis system.
 * With massive data volumes, reducing the size from `int` (4 bytes)
 * down to 1 byte significantly saves storage space and enhances
 * data density on CPU Cache (Cache line packing), thereby improving
 * access performance at O(1) level.
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
 * @brief Enumeration data type (enum) representing the geographic location of the event.
 *
 * @note Like `EventType`, `Location` is also restricted to `uint8_t` size.
 * This decision serves the data packing technique in `LogEntry`,
 * ensuring that the storage cost for the location field remains at a minimum
 * (exactly 1 byte) instead of being redundant due to compiler memory alignment.
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
 * @brief Converts EventType enum to a display string.
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
 * @brief Converts Location enum to a display string.
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
 * @brief Ultra-lightweight data structure representing a primitive
 * log record.
 *
 * @note To achieve maximum performance and comply with the Zero STL principle
 * (avoiding std::string dynamic memory allocation overhead), all string fields
 * are encoded via Dictionary Encoding outside the structure. `LogEntry` only
 * stores numeric identifiers (numeric IDs), compressed enums, and time data.
 *
 * Regarding Memory Alignment: Total data fields size is 26 bytes.
 * Since the `int64_t` `timestamp` field requires 8-byte alignment,
 * the compiler may add padding bytes, pushing the total struct size
 * to 32 bytes (standardized on 64-bit systems) to optimize the speed of
 * loading data from RAM to CPU registers, ensuring O(1) access time.
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
   * @brief Initializes an empty `LogEntry` object with default values.
   *
   * @note Time complexity: O(1). Assigning values to 0 or
   * `INVALID` is crucial when allocating raw memory via Arena Allocation,
   * helping the system avoid reading residual garbage data left
   * in the reused memory region.
   */
  LogEntry()
      : timestamp(0), userId(0), deviceId(0), appId(0), resourceId(0),
        eventType(EVENT_INVALID), location(LOC_INVALID) {}

  /**
   * @brief Initializes a `LogEntry` object with full data.
   *
   * @param user Numeric identifier (Dictionary ID) of the user.
   * @param device Numeric identifier (Dictionary ID) of the device.
   * @param app Numeric identifier (Dictionary ID) of the application.
   * @param resource Numeric identifier (Dictionary ID) of the resource.
   * @param event System event type (1 byte compressed size).
   * @param loc Geographic location (1 byte compressed size).
   * @param ts Timestamp of the event.
   *
   * @note Time complexity: O(1). This constructor serves the log parsing
   * phase. Passing data directly into the initializer list
   * minimizes intermediate copies, suitable for
   * bulk initialization via Placement New combined with Arena Allocator.
   *
   * @warning If this object is instantiated on the heap via raw pointers
   * and the `new[]` operator, the Caller takes full responsibility for managing
   * the lifecycle and calling `delete[]` to free memory. Due to strict design
   * limitations (Zero STL), this structure completely avoids implementing RAII
   * or virtual Destructor; any oversight will directly lead to a memory leak.
   */
  LogEntry(uint32_t user, uint32_t device, uint32_t app, uint32_t resource,
           EventType event, Location loc, int64_t ts)
      : timestamp(ts), userId(user), deviceId(device), appId(app),
        resourceId(resource), eventType(event), location(loc) {}
};

#endif
