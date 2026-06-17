// src/anomaly/UserContext.h
#ifndef USER_CONTEXT_H
#define USER_CONTEXT_H

#include "../core/LogEntry.h"
#include "AnomalyRules.h"
#include "RingBuffer.h"
#include <cstdint>

/**
 * @brief Quản lý toàn bộ trạng thái của một người dùng.
 * Kích thước đã được căn chỉnh (Memory Padding) để tối ưu với CPU Cache.
 */
struct UserContext {
  uint32_t userId;

  // --- 1. Nhóm Behavior (Luật 5, Luật 6) ---
  int64_t lastActivityTimestamp;
  int64_t firstActivityTimestamp;
  Location lastLocation; // Lưu vị trí của event liền trước (Dành cho Luật 5)
  bool hasActivity;

  // Luật 6 (Multi-Country): Lưu cặp (Timestamp, Location)
  // Sức chứa = 10 để tránh mất dữ liệu nếu user liên tục tạo event ở cùng 1
  // nước
  TimestampedRingBuffer<10> countryWindow;

  // --- 2. Nhóm Threshold (Luật 1, Luật 2, Luật 4) ---
  // Luật 1 (Brute-force): Chỉ quan tâm thời gian, dùng bản RingBuffer nhẹ
  RingBuffer<AnomalyRules::BRUTE_FORCE_THRESHOLD> failedLoginWindow;

  // Luật 2 (Device Hopping): Quan tâm số lượng thiết bị khác nhau, dùng bản
  // Timestamped
  TimestampedRingBuffer<AnomalyRules::DEVICE_HOP_THRESHOLD> deviceHopWindow;
  uint32_t lastDeviceId;

  // Luật 4 (Out-of-hours): Cờ chặn spam hàng triệu log cho người làm ca đêm
  bool outOfHoursReported;

  // --- 3. Nhóm Session (Luật 7, Luật 8, Luật 9) ---
  bool hasActiveSession;
  int64_t sessionStartTimestamp;
  uint32_t
      dangerousActionCount; // Đếm số lần ADMIN_ACTION hoặc DOWNLOAD trong phiên

  // Luật 9 (Rapid Session): Theo dõi tần suất mở Session mới
  RingBuffer<AnomalyRules::RAPID_SESSION_THRESHOLD> sessionStartWindow;

  // --- 4. Nhóm Advanced (Luật 10, Luật 11) ---
  // Luật 10 (Brute-force Success)
  uint32_t consecutiveFailedCount;

  // Luật 11 (Dormant Account)
  bool wasLongDormant;
  uint32_t burstEventCount;
  int64_t dormantWakeupTimestamp; // Ghi nhận khoảnh khắc tài khoản "sống lại"

  // --- 5. Nhóm Custom (Đề xuất mới) ---
  // Luật 12 (Data Exfiltration): Download nhiều resource khác nhau ngoài giờ
  TimestampedRingBuffer<AnomalyRules::EXFILTRATION_THRESHOLD>
      exfiltrationWindow;
  bool exfiltrationReported; // Cờ chặn báo trùng

  // Luật 14 (Lateral Movement): Nhảy giữa nhiều App khác nhau
  TimestampedRingBuffer<AnomalyRules::LATERAL_MOVEMENT_THRESHOLD> appWindow;
  bool lateralMovementReported;

  // --- Constructor Khởi tạo (Zero-initialization) ---
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