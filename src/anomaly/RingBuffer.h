// src/anomaly/RingBuffer.h
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <cstdint>

/**
 * @brief RingBuffer that only stores TIMESTAMPs. Used for:
 *   - Rule 1 (Brute Force): pushes timestamp of FAILED_LOGIN
 *   - Rule 9 (Rapid Session): pushes timestamp of LOGIN
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
    // When the buffer is full, 'head' points exactly to the oldest element
    return (currentTs - data[head]) <= windowSec;
  }

private:
  int64_t data[CAPACITY];
  uint32_t head;
  uint32_t count;
};

/**
 * @brief RingBuffer that stores PAIRS of (timestamp, value). Used for:
 *   - Rule 2 (Device Hop):     push(ts, deviceId)
 *   - Rule 3 (Resource Scan):  push(ts, resourceId)
 *   - Rule 6 (Multi-Country):  push(ts, location)
 *
 * isThresholdBreached() compares TIMESTAMPs.
 * countUnique() counts unique VALUES.
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
    // Compare time based on the timestamps array
    return (currentTs - timestamps[head]) <= windowSec;
  }

  // O(N^2) algorithm to count unique elements.
  // Since our CAPACITY is very small (<= 10), O(N^2) takes less than a few nano-seconds.
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