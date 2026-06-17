#ifndef ANOMALY_RULES_H
#define ANOMALY_RULES_H

#include <cstdint>

namespace AnomalyRules {
// --- Group 1: Threshold-based ---
constexpr uint32_t BRUTE_FORCE_THRESHOLD = 5;      // 5 failures
constexpr int64_t BRUTE_FORCE_WINDOW_SEC = 5 * 60; // within 5 minutes

constexpr uint32_t DEVICE_HOP_THRESHOLD = 3;       // 3 devices
constexpr int64_t DEVICE_HOP_WINDOW_SEC = 10 * 60; // within 10 minutes

constexpr uint32_t RESOURCE_SCAN_THRESHOLD = 10;     // 10 resources
constexpr int64_t RESOURCE_SCAN_WINDOW_SEC = 5 * 60; // within 5 minutes

constexpr int32_t WORK_HOUR_START = 6; // 06:00 AM
constexpr int32_t WORK_HOUR_END = 22;  // 10:00 PM

// --- Group 2: Behavior-based ---
constexpr int64_t IMPOSSIBLE_TRAVEL_MIN_SEC = 2 * 3600; // Less than 2 hours

constexpr uint32_t MULTI_COUNTRY_THRESHOLD = 3;         // 3 different countries
constexpr int64_t MULTI_COUNTRY_WINDOW_SEC = 48 * 3600; // within 48 hours

// --- Group 3: Session-based ---
constexpr int64_t MAX_SESSION_DURATION_SEC = 24 * 3600; // 24 hours
constexpr uint32_t DANGER_CHAIN_THRESHOLD = 3;          // 3 dangerous actions

constexpr uint32_t RAPID_SESSION_THRESHOLD = 3;       // 3 login sessions
constexpr int64_t RAPID_SESSION_WINDOW_SEC = 10 * 60; // within 10 minutes

// --- Group 4: Advanced ---
// Real threshold is usually 30 days, but since the dataset only covers 15 days
// we lower it to 10 days to catch anomalies in this specific dataset.
constexpr int64_t DORMANT_THRESHOLD_SEC = 10LL * 24 * 3600; // Dormant for 10 days
constexpr uint32_t DORMANT_BURST_THRESHOLD = 20;            // 20 events
constexpr int64_t DORMANT_BURST_WINDOW_SEC =
    10 * 60; // within 10 minutes after waking up

// --- Group 5: Custom (New proposals) ---
// Rule 12: Data Exfiltration — Download multiple different resources out of hours
constexpr uint32_t EXFILTRATION_THRESHOLD = 5;       // 5 different resources
constexpr int64_t EXFILTRATION_WINDOW_SEC = 10 * 60; // within 10 minutes

// Rule 13: Compromised Device — Multiple users log in on the same device
constexpr uint32_t COMPROMISED_DEVICE_THRESHOLD = 3;      // 3 different users
constexpr int64_t COMPROMISED_DEVICE_WINDOW_SEC = 5 * 60; // within 5 minutes

// Rule 14: Lateral Movement — Jumping between multiple different Apps
constexpr uint32_t LATERAL_MOVEMENT_THRESHOLD = 4;      // 4 different apps
constexpr int64_t LATERAL_MOVEMENT_WINDOW_SEC = 2 * 60; // within 2 minutes
} // namespace AnomalyRules

#endif