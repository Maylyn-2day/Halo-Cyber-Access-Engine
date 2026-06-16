// src/anomaly/AnomalyDetector.cpp
#include "AnomalyDetector.h"
#include "../core/StringPool.h"
#include "../indexing/SearchEngine.h" // Nơi chứa hàm lấy timeline
#include <iostream>


// ============================================================================
// Quản lý Vòng đời & Orchestrator
// ============================================================================

AnomalyDetector::AnomalyDetector(uint32_t poolSize)
    : contextCapacity(poolSize) {
  // Cấp phát mảng Direct-Address trên Heap (Khoảng 55MB)
  userContexts = new UserContext[poolSize]();
  deviceContexts = new DeviceContext[poolSize]();
}

AnomalyDetector::~AnomalyDetector() {
  // Dọn dẹp bộ nhớ tuyệt đối - Không để lại 1 byte Memory Leak
  delete[] userContexts;
  delete[] deviceContexts;
}

void AnomalyDetector::runAll(const SearchEngine &engine,
                             const StringPool &pool) {
  // Duyệt qua toàn bộ User trong từ điển (Batch Processing O(N))
  for (uint32_t userId = 0; userId < contextCapacity; ++userId) {

    // Lấy danh sách sự kiện (Timeline) của User này, đã được sort theo thời
    // gian từ Phase 1
    const DynamicArray<const LogEntry *> *timeline =
        engine.searchByUser(userId);

    // Nếu user này không có log nào, bỏ qua
    if (timeline == nullptr || timeline->size() == 0) {
      continue;
    }

    // Duyệt tuyến tính qua từng sự kiện của user
    for (uint32_t i = 0; i < timeline->size(); ++i) {
      processEvent((*timeline)[i]);
    }
  }
}

void AnomalyDetector::processEvent(const LogEntry *entry) {
  // Lấy context O(1) nhờ Direct-Address Table
  UserContext &uCtx = userContexts[entry->userId];
  DeviceContext &dCtx = deviceContexts[entry->deviceId];

  // --- XỬ LÝ VÒNG ĐỜI SESSION TRƯỚC ---
  if (entry->eventType == 1) { // Giả sử 1 là EVENT_LOGIN
    if (uCtx.hasActiveSession) {
      // Bị đè Session (Login khi chưa Logout) -> Chốt sổ session cũ
      checkLongSession(uCtx, entry);
    }
    uCtx.hasActiveSession = true;
    uCtx.sessionStartTimestamp = entry->timestamp;
    uCtx.dangerousActionCount = 0;
  }

  // --- 1. NHÓM THRESHOLD (Ngưỡng) ---
  checkBruteForce(uCtx, entry);
  checkDeviceHopping(uCtx, entry);
  checkResourceScan(dCtx, entry);
  checkOutOfHours(uCtx, entry);

  // --- 2. NHÓM BEHAVIOR (Hành vi) ---
  checkImpossibleTravel(uCtx, entry);
  checkMultiCountryHopping(uCtx, entry);

  // --- 3. NHÓM SESSION (Phiên) ---
  if (uCtx.hasActiveSession) {
    checkLongSession(uCtx, entry);
    checkDangerChain(uCtx, entry);
  }
  checkRapidSession(uCtx, entry);

  if (entry->eventType == 2) { // Giả sử 2 là EVENT_LOGOUT
    checkLongSession(uCtx, entry);
    uCtx.hasActiveSession = false;
  }

  // --- 4. NHÓM ADVANCED (Nâng cao) ---
  checkBruteForceSuccess(uCtx, entry);
  checkDormantAccount(uCtx, entry);

  // --- CẬP NHẬT BASELINE STATE SAU CÙNG ---
  uCtx.lastActivityTimestamp = entry->timestamp;
  uCtx.lastLocation = entry->location;
  uCtx.lastDeviceId = entry->deviceId;

  if (!uCtx.hasActivity) {
    uCtx.firstActivityTimestamp = entry->timestamp;
    uCtx.hasActivity = true;
  }
}

// ============================================================================
// Logic thuật toán 11 Luật Anomaly
// ============================================================================

// Luật 1: Brute Force
void AnomalyDetector::checkBruteForce(UserContext &ctx, const LogEntry *e) {
  if (e->eventType == 3) { // 3 = EVENT_FAILED_LOGIN
    ctx.failedLoginWindow.push(e->timestamp);
    if (ctx.failedLoginWindow.isThresholdBreached(
            e->timestamp, AnomalyRules::BRUTE_FORCE_WINDOW_SEC)) {
      recordAnomaly(AnomalyType::BRUTE_FORCE, e);
      ctx.failedLoginWindow.clear(); // Chống spam log
    }
  }
}

// Luật 2: Device Hopping
void AnomalyDetector::checkDeviceHopping(UserContext &ctx, const LogEntry *e) {
  if (e->eventType == 1) { // Chỉ tính lúc LOGIN
    ctx.deviceHopWindow.push(e->timestamp, e->deviceId);
    if (ctx.deviceHopWindow.isThresholdBreached(
            e->timestamp, AnomalyRules::DEVICE_HOP_WINDOW_SEC)) {
      if (ctx.deviceHopWindow.countUnique() >=
          AnomalyRules::DEVICE_HOP_THRESHOLD) {
        recordAnomaly(AnomalyType::DEVICE_HOPPING, e);
        ctx.deviceHopWindow.clear();
      }
    }
  }
}

// Luật 3: Resource Scan (Phát hiện cào dữ liệu)
void AnomalyDetector::checkResourceScan(DeviceContext &dctx,
                                        const LogEntry *e) {
  if (e->resourceId != 0) { // Nếu sự kiện có truy cập Resource
    dctx.resourceWindow.push(e->timestamp, e->resourceId);
    if (dctx.resourceWindow.isThresholdBreached(
            e->timestamp, AnomalyRules::RESOURCE_SCAN_WINDOW_SEC)) {
      if (dctx.resourceWindow.countUnique() >=
          AnomalyRules::RESOURCE_SCAN_THRESHOLD) {
        recordAnomaly(AnomalyType::RESOURCE_SCAN, e);
        dctx.resourceWindow.clear();
      }
    }
  }
}

// Luật 4: Out-of-Hours
void AnomalyDetector::checkOutOfHours(UserContext &ctx, const LogEntry *e) {
  if (ctx.outOfHoursReported)
    return; // Chỉ cảnh báo 1 lần / 1 user

  int32_t hour = extractHourUTC(e->timestamp);
  if (hour < AnomalyRules::WORK_HOUR_START ||
      hour >= AnomalyRules::WORK_HOUR_END) {
    recordAnomaly(AnomalyType::OUT_OF_HOURS, e);
    ctx.outOfHoursReported = true;
  }
}

// Luật 5: Impossible Travel (Vận tốc siêu nhiên)
void AnomalyDetector::checkImpossibleTravel(UserContext &ctx,
                                            const LogEntry *e) {
  // 0 = LOC_INVALID (Không xác định). Chỉ so sánh nếu cả 2 location đều hợp lệ
  if (ctx.hasActivity && ctx.lastLocation != 0 && e->location != 0) {
    if (e->location != ctx.lastLocation) {
      int64_t delta = e->timestamp - ctx.lastActivityTimestamp;
      if (delta < AnomalyRules::IMPOSSIBLE_TRAVEL_MIN_SEC) {
        recordAnomaly(AnomalyType::IMPOSSIBLE_TRAVEL, e);
      }
    }
  }
}

// Luật 6: Multi-Country Hopping (Dịch chuyển đa quốc gia)
void AnomalyDetector::checkMultiCountryHopping(UserContext &ctx,
                                               const LogEntry *e) {
  if (e->location != 0) {
    // Chỉ lưu vào buffer nếu thực sự user đổi vị trí (Tiết kiệm slot buffer)
    if (!ctx.hasActivity || e->location != ctx.lastLocation) {
      ctx.countryWindow.push(e->timestamp, static_cast<int64_t>(e->location));

      if (ctx.countryWindow.isThresholdBreached(
              e->timestamp, AnomalyRules::MULTI_COUNTRY_WINDOW_SEC)) {
        if (ctx.countryWindow.countUnique() >=
            AnomalyRules::MULTI_COUNTRY_THRESHOLD) {
          recordAnomaly(AnomalyType::MULTI_COUNTRY_HOPPING, e);
          ctx.countryWindow.clear();
        }
      }
    }
  }
}

// Luật 7: Long Session
void AnomalyDetector::checkLongSession(UserContext &ctx, const LogEntry *e) {
  int64_t duration = e->timestamp - ctx.sessionStartTimestamp;
  if (duration > AnomalyRules::MAX_SESSION_DURATION_SEC) {
    recordAnomaly(AnomalyType::LONG_SESSION, e);
    ctx.hasActiveSession = false; // Ngắt để không báo lặp lại liên tục
  }
}

// Luật 8: Danger Chain (Hành động quản trị đáng ngờ)
void AnomalyDetector::checkDangerChain(UserContext &ctx, const LogEntry *e) {
  if (e->eventType == 4 || e->eventType == 5) { // Giả sử 4=ADMIN, 5=DOWNLOAD
    ctx.dangerousActionCount++;
    if (ctx.dangerousActionCount >= AnomalyRules::DANGER_CHAIN_THRESHOLD) {
      recordAnomaly(AnomalyType::DANGER_CHAIN, e);
      ctx.dangerousActionCount = 0;
    }
  }
}

// Luật 9: Rapid Session (Spam mở ứng dụng)
void AnomalyDetector::checkRapidSession(UserContext &ctx, const LogEntry *e) {
  if (e->eventType == 1) { // LOGIN
    ctx.sessionStartWindow.push(e->timestamp);
    if (ctx.sessionStartWindow.isThresholdBreached(
            e->timestamp, AnomalyRules::RAPID_SESSION_WINDOW_SEC)) {
      recordAnomaly(AnomalyType::RAPID_SESSION, e);
      ctx.sessionStartWindow.clear();
    }
  }
}

// Luật 10: Brute-Force Success (Nâng cao 1)
void AnomalyDetector::checkBruteForceSuccess(UserContext &ctx,
                                             const LogEntry *e) {
  if (e->eventType == 3) { // FAILED_LOGIN
    ctx.consecutiveFailedCount++;
  } else {
    if (e->eventType == 1 &&
        ctx.consecutiveFailedCount >= AnomalyRules::BRUTE_FORCE_THRESHOLD) {
      recordAnomaly(AnomalyType::BRUTE_FORCE_SUCCESS, e);
    }
    // RESET khi gặp mọi event khác
    ctx.consecutiveFailedCount = 0;
  }
}

// Luật 11: Dormant Account (Nâng cao 2)
void AnomalyDetector::checkDormantAccount(UserContext &ctx, const LogEntry *e) {
  if (ctx.hasActivity) {
    int64_t delta = e->timestamp - ctx.lastActivityTimestamp;
    if (delta > AnomalyRules::DORMANT_THRESHOLD_SEC) {
      ctx.wasLongDormant = true;
      ctx.burstEventCount = 0;
      ctx.dormantWakeupTimestamp = e->timestamp;
    }
  }

  if (ctx.wasLongDormant) {
    if (e->timestamp - ctx.dormantWakeupTimestamp <=
        AnomalyRules::DORMANT_BURST_WINDOW_SEC) {
      ctx.burstEventCount++;
      if (ctx.burstEventCount == AnomalyRules::DORMANT_BURST_THRESHOLD) {
        recordAnomaly(AnomalyType::DORMANT_ACCOUNT, e);
      }
    } else {
      ctx.wasLongDormant =
          false; // Đã qua 10 phút, tài khoản về trạng thái an toàn
    }
  }
}

// ============================================================================
// Các hàm Tiện ích (Utilities)
// ============================================================================

int32_t AnomalyDetector::extractHourUTC(int64_t timestamp) {
  // Chuyển đổi Epoch time sang giờ hệ thống (Giả định Dataset ở múi giờ GMT+7)
  // 7 giờ * 3600 giây = 25200 giây. Modulo 86400 để lấy số giây trong 1 ngày.
  int64_t localSeconds = (timestamp + 25200) % 86400;
  return (int32_t)(localSeconds / 3600);
}

void AnomalyDetector::recordAnomaly(AnomalyType type, const LogEntry *e) {
  AnomalyRecord rec;
  rec.timestamp = e->timestamp;
  rec.userId = e->userId;
  rec.deviceId = e->deviceId;
  rec.type = type;

  results.pushBack(rec); // Giả định DynamicArray của bạn đang dùng hàm pushBack
}

void AnomalyDetector::printReport(const StringPool &pool) const {
  std::cout << "\n============================================\n";
  std::cout << "         HALO CYBER ACCESS ENGINE           \n";
  std::cout << "         ANOMALY DETECTION REPORT           \n";
  std::cout << "============================================\n";
  std::cout << "[!] Total Anomalies Detected: " << results.size() << "\n\n";

  // In ra 10 cảnh báo đầu tiên để minh họa
  uint32_t limit = (results.size() > 10) ? 10 : results.size();
  for (uint32_t i = 0; i < limit; ++i) {
    const AnomalyRecord &rec = results[i];
    std::cout << "- At TS: " << rec.timestamp << " | Type: " << (int)rec.type
              << " | UserID: " << rec.userId << "\n";
  }
  std::cout << "============================================\n";
}