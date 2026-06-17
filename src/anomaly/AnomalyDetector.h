// src/anomaly/AnomalyDetector.h
#ifndef ANOMALY_DETECTOR_H
#define ANOMALY_DETECTOR_H

#include "../core/DynamicArray.h"
#include "../core/LogEntry.h"
#include "AnomalyRecord.h"
#include "DeviceContext.h"
#include "UserContext.h"
#include <cstdint>

// Forward Declaration to reduce compilation time
// and avoid circular dependency
class SearchEngine;
class StringPool;

/**
 * @brief Orchestrator class for the anomaly detection process.
 * Applies Batch Processing model: iterates through all user/device timelines.
 */
class AnomalyDetector {
private:
  // --- State Stores (Direct-Address Tables) ---
  // Context storage array. The array index is the Dictionary ID.
  UserContext *userContexts;
  DeviceContext *deviceContexts;
  uint32_t contextCapacity;

  // Storage for all detected anomalies
  DynamicArray<AnomalyRecord> results;

  // --- Internal Helper Methods ---

  // Processes a single log line, triggering all 11 rules
  void processEvent(const LogEntry *entry);

  // [Group 1]: Threshold-based Anomalies
  void checkBruteForce(UserContext &ctx, const LogEntry *e);
  void checkDeviceHopping(UserContext &ctx, const LogEntry *e);
  void checkResourceScan(DeviceContext &dctx, const LogEntry *e);
  void checkOutOfHours(
      UserContext &ctx,
      const LogEntry *e); // Added ctx to check outOfHoursReported flag

  // [Group 2]: Behavior-based Anomalies
  void checkImpossibleTravel(UserContext &ctx, const LogEntry *e);
  void checkMultiCountryHopping(UserContext &ctx, const LogEntry *e);

  // [Group 3]: Session-based Anomalies
  void checkLongSession(UserContext &ctx, const LogEntry *e);
  void checkDangerChain(UserContext &ctx, const LogEntry *e);
  void checkRapidSession(UserContext &ctx, const LogEntry *e);

  // [Group 4]: Advanced
  void checkBruteForceSuccess(UserContext &ctx, const LogEntry *e);
  void checkDormantAccount(UserContext &ctx, const LogEntry *e);

  // [Group 5]: Custom (New proposals)
  void checkDataExfiltration(UserContext &ctx, const LogEntry *e);
  void checkCompromisedDevice(DeviceContext &dctx, const LogEntry *e);
  void checkLateralMovement(UserContext &ctx, const LogEntry *e);

  // Extracts hour (0-23) from UTC Timestamp
  static int32_t extractHourUTC(int64_t timestamp);

  // Converts AnomalyType enum to C string (used for both console and CSV)
  static const char *anomalyTypeToString(AnomalyType type);

  // Records a new anomaly to the results array
  void recordAnomaly(AnomalyType type, const LogEntry *e);

public:
  /**
   * @brief Constructor that allocates array memory.
   * @param poolSize Maximum number of strings in StringPool (used as capacity for
   * the array)
   */
  explicit AnomalyDetector(uint32_t poolSize);

  /**
   * @brief Destructor that frees all Context memory. Crucial to prevent
   * Memory Leaks!
   */
  ~AnomalyDetector();

  // Disable Copy Constructor and Copy Assignment (Rule of Three)
  // Ensures no one can mistakenly copy the dynamic memory of this class
  AnomalyDetector(const AnomalyDetector &) = delete;
  AnomalyDetector &operator=(const AnomalyDetector &) = delete;

  /**
   * @brief Triggers the engine to run all inspection rules.
   * @param engine SearchEngine built with Index from Phase 1.
   * @param pool StringPool to support reporting (if needed).
   */
  void runAll(const SearchEngine &engine, const StringPool &pool);

  /**
   * @brief Prints detailed report of anomalies to the Console.
   */
  void printReport(const StringPool &pool) const;

  /**
   * @brief Exports all detailed results to a CSV file.
   * @param filepath Output file path (e.g., "anomaly_report.csv").
   * @param pool StringPool to decode userId/deviceId back to strings.
   * @return true if file written successfully, false if I/O error.
   */
  bool exportToCSV(const char *filepath, const StringPool &pool) const;

  /**
   * @brief Returns the results array (Used for Unit Tests or writing CSV files).
   */
  const DynamicArray<AnomalyRecord> &getResults() const { return results; }
};

#endif