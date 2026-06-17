// src/anomaly/UserContext.h
#ifndef USER_CONTEXT_H
#define USER_CONTEXT_H

#include "../core/LogEntry.h"
#include "AnomalyRules.h"
#include "RingBuffer.h"
#include <cstdint>

/**
 * @brief Manages the entire state of a user.
 * Size is aligned (Memory Padding) to optimize for CPU Cache.
 */
struct UserContext {
  uint32_t userId;

  // --- 1. Behavior Group (Rule 5, Rule 6) ---
  int64_t lastActivityTimestamp;
  int64_t firstActivityTimestamp;
  Location lastLocation; // Stores location of the immediately preceding event (For Rule 5)
  bool hasActivity;

  // Rule 6 (Multi-Country): Stores pairs of (Timestamp, Location)
  // Capacity = 10 to avoid data loss if user continuously creates events in the same
  // country
  TimestampedRingBuffer<10> countryWindow;

  // --- 2. Threshold Group (Rule 1, Rule 2, Rule 4) ---
  // Rule 1 (Brute-force): Only cares about time, uses lightweight RingBuffer
  RingBuffer<AnomalyRules::BRUTE_FORCE_THRESHOLD> failedLoginWindow;

  // Rule 2 (Device Hopping): Cares about the number of different devices, uses
  // Timestamped version
  TimestampedRingBuffer<AnomalyRules::DEVICE_HOP_THRESHOLD> deviceHopWindow;
  uint32_t lastDeviceId;

  // Rule 4 (Out-of-hours): Flag to prevent spamming millions of logs for night-shift workers
  bool outOfHoursReported;

  // --- 3. Session Group (Rule 7, Rule 8, Rule 9) ---
  bool hasActiveSession;
  int64_t sessionStartTimestamp;
  uint32_t
      dangerousActionCount; // Counts ADMIN_ACTION or DOWNLOAD occurrences within the session

  // Rule 9 (Rapid Session): Tracks frequency of opening new Sessions
  RingBuffer<AnomalyRules::RAPID_SESSION_THRESHOLD> sessionStartWindow;

  // --- 4. Advanced Group (Rule 10, Rule 11) ---
  // Rule 10 (Brute-force Success)
  uint32_t consecutiveFailedCount;

  // Rule 11 (Dormant Account)
  bool wasLongDormant;
  uint32_t burstEventCount;
  int64_t dormantWakeupTimestamp; // Records the moment the account "wakes up"

  // --- 5. Custom Group (New proposals) ---
  // Rule 12 (Data Exfiltration): Download multiple different resources out of hours
  TimestampedRingBuffer<AnomalyRules::EXFILTRATION_THRESHOLD>
      exfiltrationWindow;
  bool exfiltrationReported; // Flag to prevent duplicate reports

  // Rule 14 (Lateral Movement): Jumping between multiple different Apps
  TimestampedRingBuffer<AnomalyRules::LATERAL_MOVEMENT_THRESHOLD> appWindow;
  bool lateralMovementReported;

  // --- Constructor Initialization (Zero-initialization) ---
  UserContext()
      : userId(0), lastActivityTimestamp(0), firstActivityTimestamp(0),
        lastLocation(LOC_INVALID), hasActivity(false),
        outOfHoursReported(false), hasActiveSession(false),
        sessionStartTimestamp(0), dangerousActionCount(0),
        consecutiveFailedCount(0), wasLongDormant(false), burstEventCount(0),
        dormantWakeupTimestamp(0), exfiltrationReported(false),
        lateralMovementReported(false) {}
};

#endif