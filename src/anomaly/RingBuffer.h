// src/anomaly/RingBuffer.h
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <cstdint>

/**
 * @brief RingBuffer chỉ lưu TIMESTAMP. Dùng cho:
 *   - Luật 1 (Brute Force): push timestamp của FAILED_LOGIN
 *   - Luật 9 (Rapid Session): push timestamp của LOGIN
 */
template <uint32_t CAPACITY> class RingBuffer {
public:
  RingBuffer() : head(0), count(0) {}

  void push(int64_t timestamp) {
    data[head] = timestamp;
    head = (head + 1) % CAPACITY;
    if (count < CAPACITY)
      ++count;
  }

  void clear() {
    head = 0;
    count = 0;
  }

  bool isFull() const { return count == CAPACITY; }

  bool isThresholdBreached(int64_t currentTs, int64_t windowSec) const {
    if (!isFull())
      return false;
    // Khi buffer đầy, 'head' trỏ đúng vào phần tử cũ nhất
    return (currentTs - data[head]) <= windowSec;
  }

private:
  int64_t data[CAPACITY];
  uint32_t head;
  uint32_t count;
};

/**
 * @brief RingBuffer lưu CẶP (timestamp, value). Dùng cho:
 *   - Luật 2 (Device Hop):     push(ts, deviceId)
 *   - Luật 3 (Resource Scan):  push(ts, resourceId)
 *   - Luật 6 (Multi-Country):  push(ts, location)
 *
 * isThresholdBreached() so sánh TIMESTAMP (đúng).
 * countUnique() đếm VALUES unique (đúng).
 */
template <uint32_t CAPACITY> class TimestampedRingBuffer {
public:
  TimestampedRingBuffer() : head(0), count(0) {}

  void push(int64_t timestamp, int64_t value) {
    timestamps[head] = timestamp;
    values[head] = value;
    head = (head + 1) % CAPACITY;
    if (count < CAPACITY)
      ++count;
  }

  void clear() {
    head = 0;
    count = 0;
  }

  bool isFull() const { return count == CAPACITY; }

  bool isThresholdBreached(int64_t currentTs, int64_t windowSec) const {
    if (!isFull())
      return false;
    // So sánh thời gian dựa trên mảng timestamps
    return (currentTs - timestamps[head]) <= windowSec;
  }

  // Thuật toán O(N^2) để đếm số phần tử unique.
  // Vì CAPACITY của chúng ta rất nhỏ (<= 10), O(N^2) tốn chưa tới vài
  // nano-giây.
  uint32_t countUnique() const {
    uint32_t unique = 0;
    for (uint32_t i = 0; i < count; ++i) {
      bool isDuplicate = false;
      for (uint32_t j = 0; j < i; ++j) {
        if (values[i] == values[j]) {
          isDuplicate = true;
          break;
        }
      }
      if (!isDuplicate)
        ++unique;
    }
    return unique;
  }

private:
  int64_t timestamps[CAPACITY];
  int64_t values[CAPACITY];
  uint32_t head;
  uint32_t count;
};

#endif