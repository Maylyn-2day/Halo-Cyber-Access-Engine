// src/anomaly/DeviceContext.h
#ifndef DEVICE_CONTEXT_H
#define DEVICE_CONTEXT_H

#include "AnomalyRules.h"
#include "RingBuffer.h"
#include <cstdint>


/**
 * @brief Manages the state of an independent device.
 * This structure will be allocated in a Direct-Address array.
 */
struct DeviceContext {
  uint32_t deviceId;

  // --- Rule 3: Resource Scan ---
  // Stores pairs of (Timestamp, ResourceID) for the last 10 accesses.
  // Used to count how many DIFFERENT Resources are accessed within 5 minutes.
  TimestampedRingBuffer<AnomalyRules::RESOURCE_SCAN_THRESHOLD> resourceWindow;

  // --- Rule 13: Compromised Device ---
  // Stores pairs of (Timestamp, UserId) to detect multiple users logging in on the same device
  TimestampedRingBuffer<AnomalyRules::COMPROMISED_DEVICE_THRESHOLD> userWindow;
  bool compromisedReported;

  // Constructor: Clears memory garbage by initializing everything to 0
  DeviceContext() : deviceId(0), compromisedReported(false) {}
};

#endif // DEVICE_CONTEXT_H