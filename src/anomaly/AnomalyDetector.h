// src/anomaly/AnomalyDetector.h
#ifndef ANOMALY_DETECTOR_H
#define ANOMALY_DETECTOR_H

#include "../core/DynamicArray.h"
#include "../core/LogEntry.h"
#include "AnomalyRecord.h"
#include "DeviceContext.h"
#include "UserContext.h"
#include <cstdint>

// Khai báo sớm (Forward Declaration) để giảm thời gian biên dịch
// và tránh circular dependency
class SearchEngine;
class StringPool;

/**
 * @brief Lớp điều phối quá trình phát hiện bất thường (Orchestrator).
 * Áp dụng mô hình Batch Processing: duyệt qua tất cả timeline của user/device.
 */
class AnomalyDetector {
private:
  // --- State Stores (Direct-Address Tables) ---
  // Mảng lưu trữ Context. Index của mảng chính là Dictionary ID.
  UserContext *userContexts;
  DeviceContext *deviceContexts;
  uint32_t contextCapacity;

  // Nơi lưu trữ tất cả các bất thường phát hiện được
  DynamicArray<AnomalyRecord> results;

  // --- Helper Methods nội bộ ---

  // Xử lý 1 dòng log duy nhất, kích hoạt toàn bộ 11 luật
  void processEvent(const LogEntry *entry);

  // [Nhóm 1]: Bất thường dựa trên Threshold (Ngưỡng)
  void checkBruteForce(UserContext &ctx, const LogEntry *e);
  void checkDeviceHopping(UserContext &ctx, const LogEntry *e);
  void checkResourceScan(DeviceContext &dctx, const LogEntry *e);
  void checkOutOfHours(
      UserContext &ctx,
      const LogEntry *e); // Đã thêm ctx để check cờ outOfHoursReported

  // [Nhóm 2]: Bất thường dựa trên Behavior (Hành vi)
  void checkImpossibleTravel(UserContext &ctx, const LogEntry *e);
  void checkMultiCountryHopping(UserContext &ctx, const LogEntry *e);

  // [Nhóm 3]: Bất thường dựa trên Session (Phiên làm việc)
  void checkLongSession(UserContext &ctx, const LogEntry *e);
  void checkDangerChain(UserContext &ctx, const LogEntry *e);
  void checkRapidSession(UserContext &ctx, const LogEntry *e);

  // [Nhóm 4]: Advanced (Nâng cao)
  void checkBruteForceSuccess(UserContext &ctx, const LogEntry *e);
  void checkDormantAccount(UserContext &ctx, const LogEntry *e);

  // Trích xuất giờ (0-23) từ Timestamp dạng UTC
  static int32_t extractHourUTC(int64_t timestamp);

  // Chuyển đổi AnomalyType enum → chuỗi C (dùng cho cả console và CSV)
  static const char *anomalyTypeToString(AnomalyType type);

  // Ghi nhận một bất thường mới vào mảng results
  void recordAnomaly(AnomalyType type, const LogEntry *e);

public:
  /**
   * @brief Constructor cấp phát bộ nhớ mảng.
   * @param poolSize Số lượng chuỗi tối đa trong StringPool (để làm capacity cho
   * mảng)
   */
  explicit AnomalyDetector(uint32_t poolSize);

  /**
   * @brief Destructor giải phóng toàn bộ bộ nhớ Context. Rất quan trọng để
   * tránh Memory Leak!
   */
  ~AnomalyDetector();

  // Vô hiệu hóa Copy Constructor và Copy Assignment (Luật Rule of Three)
  // Đảm bảo không ai có thể copy nhầm bộ nhớ động của class này
  AnomalyDetector(const AnomalyDetector &) = delete;
  AnomalyDetector &operator=(const AnomalyDetector &) = delete;

  /**
   * @brief Hàm kích hoạt engine chạy toàn bộ các luật kiểm tra.
   * @param engine SearchEngine đã được build chỉ mục (Index) từ Phase 1.
   * @param pool StringPool để hỗ trợ báo cáo (nếu cần).
   */
  void runAll(const SearchEngine &engine, const StringPool &pool);

  /**
   * @brief In báo cáo chi tiết các bất thường ra màn hình Console.
   */
  void printReport(const StringPool &pool) const;

  /**
   * @brief Xuất toàn bộ kết quả chi tiết ra file CSV.
   * @param filepath Đường dẫn file đầu ra (VD: "anomaly_report.csv").
   * @param pool StringPool để dịch ngược userId/deviceId thành chuỗi.
   * @return true nếu ghi file thành công, false nếu lỗi I/O.
   */
  bool exportToCSV(const char *filepath, const StringPool &pool) const;

  /**
   * @brief Trả về mảng kết quả (Dùng cho Unit Test hoặc ghi file CSV).
   */
  const DynamicArray<AnomalyRecord> &getResults() const { return results; }
};

#endif