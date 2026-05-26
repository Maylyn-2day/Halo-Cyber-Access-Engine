// LogEntry.h
#ifndef LOG_ENTRY_H
#define LOG_ENTRY_H

#include <cstdint>

/*
 * Compact event type enum.
 *
 * Backed by unsigned char to keep each LogEntry small for high-volume in-memory analytics workloads.
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

/*
 * Compact location enum.
 *
 * Backed by unsigned char so location storage costs exactly one byte inside LogEntry.
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

/*
 * Ultra-lightweight log row.
 *
 * String fields are dictionary-encoded outside this struct. The row stores
 * only numeric dictionary IDs, compact enums, and timestamp data.
 */
struct LogEntry {
    int64_t timestamp;
    uint32_t userId;
    uint32_t deviceId;
    uint32_t appId;
    uint32_t resourceId;
    EventType eventType;
    Location location;

    LogEntry()
        : timestamp(0),
          userId(0),
          deviceId(0),
          appId(0),
          resourceId(0),
          eventType(EVENT_INVALID),
          location(LOC_INVALID) {}

    LogEntry(
        uint32_t user,
        uint32_t device,
        uint32_t app,
        uint32_t resource,
        EventType event,
        Location loc,
        int64_t ts
    )
        : timestamp(ts),
          userId(user),
          deviceId(device),
          appId(app),
          resourceId(resource),
          eventType(event),
          location(loc) {}
};

#endif