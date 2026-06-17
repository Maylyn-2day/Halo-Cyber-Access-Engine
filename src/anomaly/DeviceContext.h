// src/anomaly/DeviceContext.h
#ifndef DEVICE_CONTEXT_H
#define DEVICE_CONTEXT_H

#include "AnomalyRules.h"
#include "RingBuffer.h"
#include <cstdint>


/**
 * @brief Quản lý trạng thái (State) của một thiết bị độc lập.
 * Cấu trúc này sẽ được cấp phát trong một mảng Direct-Address.
 */
struct DeviceContext {
  uint32_t deviceId;

  // --- Luật 3: Resource Scan ---
  // Lưu cặp (Timestamp, ResourceID) của 10 lần truy cập gần nhất.
  // Dùng để đếm xem có bao nhiêu Resource KHÁC NHAU được truy cập trong 5 phút.
  TimestampedRingBuffer<AnomalyRules::RESOURCE_SCAN_THRESHOLD> resourceWindow;

  // --- Luật 13: Compromised Device ---
  // Lưu cặp (Timestamp, UserId) để phát hiện nhiều user login trên cùng device
  TimestampedRingBuffer<AnomalyRules::COMPROMISED_DEVICE_THRESHOLD> userWindow;
  bool compromisedReported;

  // Constructor: Xóa rác bộ nhớ bằng cách khởi tạo mọi thứ về 0
  DeviceContext() : deviceId(0), compromisedReported(false) {}
};

#endif // DEVICE_CONTEXT_H