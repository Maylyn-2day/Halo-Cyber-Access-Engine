#ifndef ANOMALY_RULES_H
#define ANOMALY_RULES_H

#include <cstdint>

namespace AnomalyRules {
// --- Nhóm 1: Threshold-based (Ngưỡng) ---
constexpr uint32_t BRUTE_FORCE_THRESHOLD = 5;      // 5 lần thất bại
constexpr int64_t BRUTE_FORCE_WINDOW_SEC = 5 * 60; // trong 5 phút

constexpr uint32_t DEVICE_HOP_THRESHOLD = 3;       // 3 thiết bị
constexpr int64_t DEVICE_HOP_WINDOW_SEC = 10 * 60; // trong 10 phút

constexpr uint32_t RESOURCE_SCAN_THRESHOLD = 10;     // 10 resources
constexpr int64_t RESOURCE_SCAN_WINDOW_SEC = 5 * 60; // trong 5 phút

constexpr int32_t WORK_HOUR_START = 6; // 06:00 sáng
constexpr int32_t WORK_HOUR_END = 22;  // 22:00 đêm

// --- Nhóm 2: Behavior-based (Hành vi) ---
constexpr int64_t IMPOSSIBLE_TRAVEL_MIN_SEC = 2 * 3600; // Dưới 2 giờ

constexpr uint32_t MULTI_COUNTRY_THRESHOLD = 3;         // 3 quốc gia khác nhau
constexpr int64_t MULTI_COUNTRY_WINDOW_SEC = 48 * 3600; // trong 48 giờ

// --- Nhóm 3: Session-based (Phiên) ---
constexpr int64_t MAX_SESSION_DURATION_SEC = 24 * 3600; // 24 giờ
constexpr uint32_t DANGER_CHAIN_THRESHOLD = 3;          // 3 hành động nguy hiểm

constexpr uint32_t RAPID_SESSION_THRESHOLD = 3;       // 3 phiên đăng nhập
constexpr int64_t RAPID_SESSION_WINDOW_SEC = 10 * 60; // trong 10 phút

// --- Nhóm 4: Advanced (Nâng cao) ---
constexpr int64_t DORMANT_THRESHOLD_SEC = 30LL * 24 * 3600; // Ngủ đông 30 ngày
constexpr uint32_t DORMANT_BURST_THRESHOLD = 20;            // 20 sự kiện
constexpr int64_t DORMANT_BURST_WINDOW_SEC =
    10 * 60; // trong 10 phút sau khi thức dậy
} // namespace AnomalyRules

#endif