// src/anomaly/AnomalyDetector.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "AnomalyDetector.h"
#include "../core/StringPool.h"
#include "../indexing/SearchEngine.h"
#include "../ConsoleColor.h"
#include <cstdio>
#include <ctime>
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
  if (entry->eventType == EVENT_LOGIN) {
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

  if (entry->eventType == EVENT_LOGOUT) {
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
  if (e->eventType == EVENT_FAILED_LOGIN) {
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
  if (e->eventType == EVENT_LOGIN) {
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
  if (e->eventType == EVENT_ADMIN_ACTION || e->eventType == EVENT_DOWNLOAD) {
    ctx.dangerousActionCount++;
    if (ctx.dangerousActionCount >= AnomalyRules::DANGER_CHAIN_THRESHOLD) {
      recordAnomaly(AnomalyType::DANGER_CHAIN, e);
      ctx.dangerousActionCount = 0;
    }
  }
}

// Luật 9: Rapid Session (Spam mở ứng dụng)
void AnomalyDetector::checkRapidSession(UserContext &ctx, const LogEntry *e) {
  if (e->eventType == EVENT_LOGIN) {
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
  if (e->eventType == EVENT_FAILED_LOGIN) {
    ctx.consecutiveFailedCount++;
  } else {
    if (e->eventType == EVENT_LOGIN &&
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

const char *AnomalyDetector::anomalyTypeToString(AnomalyType type) {
  switch (type) {
  case AnomalyType::BRUTE_FORCE:
    return "BRUTE_FORCE";
  case AnomalyType::DEVICE_HOPPING:
    return "DEVICE_HOPPING";
  case AnomalyType::RESOURCE_SCAN:
    return "RESOURCE_SCAN";
  case AnomalyType::OUT_OF_HOURS:
    return "OUT_OF_HOURS";
  case AnomalyType::IMPOSSIBLE_TRAVEL:
    return "IMPOSSIBLE_TRAVEL";
  case AnomalyType::MULTI_COUNTRY_HOPPING:
    return "MULTI_COUNTRY_HOPPING";
  case AnomalyType::LONG_SESSION:
    return "LONG_SESSION";
  case AnomalyType::DANGER_CHAIN:
    return "DANGER_CHAIN";
  case AnomalyType::RAPID_SESSION:
    return "RAPID_SESSION";
  case AnomalyType::BRUTE_FORCE_SUCCESS:
    return "BRUTE_FORCE_SUCCESS";
  case AnomalyType::DORMANT_ACCOUNT:
    return "DORMANT_ACCOUNT";
  default:
    return "UNKNOWN";
  }
}

// ============================================================================
// Executive Summary (Console Dashboard)
// ============================================================================

void AnomalyDetector::printReport(const StringPool &pool) const {
  std::cout << '\n';
  std::cout << ConsoleColor::CYAN
            << "============================================================"
            << ConsoleColor::RESET << '\n';
  std::cout << ConsoleColor::BCYAN
            << "       HALO CYBER ACCESS ENGINE - ANOMALY REPORT            "
            << ConsoleColor::RESET << '\n';
  std::cout << ConsoleColor::CYAN
            << "============================================================"
            << ConsoleColor::RESET << '\n';

  if (results.size() == 0) {
    std::cout << ConsoleColor::BGREEN << "  [OK] No anomalies detected."
              << ConsoleColor::RESET << '\n';
    std::cout << ConsoleColor::CYAN
              << "============================================================"
              << ConsoleColor::RESET << '\n';
    return;
  }

  // --- 1. Dem theo loai anomaly ---
  const uint32_t TYPE_COUNT = 11;
  uint32_t countByType[TYPE_COUNT] = {};
  for (uint32_t i = 0; i < results.size(); ++i) {
    uint8_t idx = static_cast<uint8_t>(results[i].type);
    if (idx < TYPE_COUNT)
      countByType[idx]++;
  }

  std::cout << "\n  SEVERITY BREAKDOWN\n";
  std::cout << "  ----------------------------------------------------------\n";

  // Critical (Do sang)
  uint32_t critical = countByType[9] + countByType[4] + countByType[10];
  std::cout << ConsoleColor::BRED << "  [CRITICAL] " << critical << " alerts"
            << ConsoleColor::RESET << '\n';
  if (countByType[9] > 0)
    std::cout << ConsoleColor::BRED << "    - Brute-Force Success : "
              << countByType[9] << ConsoleColor::RESET << '\n';
  if (countByType[4] > 0)
    std::cout << ConsoleColor::BRED << "    - Impossible Travel   : "
              << countByType[4] << ConsoleColor::RESET << '\n';
  if (countByType[10] > 0)
    std::cout << ConsoleColor::BRED << "    - Dormant Account     : "
              << countByType[10] << ConsoleColor::RESET << '\n';

  // High (Vang sang)
  uint32_t high = countByType[7] + countByType[5] + countByType[2];
  std::cout << ConsoleColor::BYELLOW << "  [HIGH]     " << high << " alerts"
            << ConsoleColor::RESET << '\n';
  if (countByType[7] > 0)
    std::cout << ConsoleColor::BYELLOW << "    - Danger Chain        : "
              << countByType[7] << ConsoleColor::RESET << '\n';
  if (countByType[5] > 0)
    std::cout << ConsoleColor::BYELLOW << "    - Multi-Country Hop   : "
              << countByType[5] << ConsoleColor::RESET << '\n';
  if (countByType[2] > 0)
    std::cout << ConsoleColor::BYELLOW << "    - Resource Scan       : "
              << countByType[2] << ConsoleColor::RESET << '\n';

  // Medium (Vang)
  uint32_t medium =
      countByType[0] + countByType[1] + countByType[8] + countByType[6];
  std::cout << ConsoleColor::YELLOW << "  [MEDIUM]   " << medium << " alerts"
            << ConsoleColor::RESET << '\n';
  if (countByType[0] > 0)
    std::cout << ConsoleColor::YELLOW << "    - Brute Force         : "
              << countByType[0] << ConsoleColor::RESET << '\n';
  if (countByType[1] > 0)
    std::cout << ConsoleColor::YELLOW << "    - Device Hopping      : "
              << countByType[1] << ConsoleColor::RESET << '\n';
  if (countByType[8] > 0)
    std::cout << ConsoleColor::YELLOW << "    - Rapid Session       : "
              << countByType[8] << ConsoleColor::RESET << '\n';
  if (countByType[6] > 0)
    std::cout << ConsoleColor::YELLOW << "    - Long Session        : "
              << countByType[6] << ConsoleColor::RESET << '\n';

  // Low (Xam)
  uint32_t low = countByType[3];
  std::cout << ConsoleColor::GRAY << "  [LOW]      " << low << " alerts"
            << ConsoleColor::RESET << '\n';
  if (countByType[3] > 0)
    std::cout << ConsoleColor::GRAY << "    - Out-of-Hours        : "
              << countByType[3] << ConsoleColor::RESET << '\n';

  // --- 2. Top 5 User vi pham nhieu nhat ---
  uint32_t *userCounts = new uint32_t[contextCapacity]();
  for (uint32_t i = 0; i < results.size(); ++i) {
    if (results[i].userId < contextCapacity)
      userCounts[results[i].userId]++;
  }

  std::cout << "\n  TOP 5 RISKIEST USERS\n";
  std::cout << "  ----------------------------------------------------------\n";

  for (uint32_t rank = 0; rank < 5; ++rank) {
    uint32_t maxCount = 0;
    uint32_t maxId = 0;
    for (uint32_t u = 0; u < contextCapacity; ++u) {
      if (userCounts[u] > maxCount) {
        maxCount = userCounts[u];
        maxId = u;
      }
    }
    if (maxCount == 0)
      break;
    std::cout << ConsoleColor::BRED << "    " << (rank + 1) << ". "
              << pool.getString(maxId) << ConsoleColor::RESET
              << " (" << maxCount << " alerts)\n";
    userCounts[maxId] = 0;
  }

  delete[] userCounts;

  // --- 3. Tong ket ---
  std::cout << "\n  ----------------------------------------------------------\n";
  std::cout << "  TOTAL: " << ConsoleColor::BRED << results.size()
            << ConsoleColor::RESET << " anomalies detected.\n";
  std::cout << ConsoleColor::CYAN
            << "============================================================"
            << ConsoleColor::RESET << '\n';
}

// ============================================================================
// Detailed CSV Export
// ============================================================================

bool AnomalyDetector::exportToCSV(const char *filepath,
                                  const StringPool &pool) const {
  FILE *file = std::fopen(filepath, "w");
  if (file == nullptr) {
    std::cerr << "[AnomalyDetector] Cannot create report file '" << filepath
              << "'\n";
    return false;
  }

  // Ghi header CSV
  std::fprintf(file, "timestamp,user_id,device_id,anomaly_type,severity\n");

  for (uint32_t i = 0; i < results.size(); ++i) {
    const AnomalyRecord &rec = results[i];

    // Dịch ngược Dictionary ID → chuỗi gốc nhờ StringPool
    std::string userName = pool.getString(rec.userId);
    std::string deviceName = pool.getString(rec.deviceId);
    const char *typeName = anomalyTypeToString(rec.type);

    // Phân loại severity
    const char *severity = "LOW";
    switch (rec.type) {
    case AnomalyType::BRUTE_FORCE_SUCCESS:
    case AnomalyType::IMPOSSIBLE_TRAVEL:
    case AnomalyType::DORMANT_ACCOUNT:
      severity = "CRITICAL";
      break;
    case AnomalyType::DANGER_CHAIN:
    case AnomalyType::MULTI_COUNTRY_HOPPING:
    case AnomalyType::RESOURCE_SCAN:
      severity = "HIGH";
      break;
    case AnomalyType::BRUTE_FORCE:
    case AnomalyType::DEVICE_HOPPING:
    case AnomalyType::RAPID_SESSION:
    case AnomalyType::LONG_SESSION:
      severity = "MEDIUM";
      break;
    case AnomalyType::OUT_OF_HOURS:
      severity = "LOW";
      break;
    default:
      break;
    }

    // Chuyển epoch → human-readable
    char timeStr[32];
    time_t rawTime = static_cast<time_t>(rec.timestamp);
    struct tm *ti = gmtime(&rawTime);
    if (ti != nullptr) {
      std::snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
                    ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
                    ti->tm_hour, ti->tm_min, ti->tm_sec);
    } else {
      std::snprintf(timeStr, sizeof(timeStr), "%lld",
                    static_cast<long long>(rec.timestamp));
    }

    std::fprintf(file, "%s,%s,%s,%s,%s\n", timeStr, userName.c_str(),
                 deviceName.c_str(), typeName, severity);
  }

  std::fclose(file);
  return true;
}