#ifndef STRING_POOL_H
#define STRING_POOL_H

#include <cstdint>
#include <cstring>
#include <string>

#include "DynamicArray.h"

/**
 * @brief Hệ thống Cấu trúc từ điển hoá (Dictionary Encoding / String Pool) để
 * tối ưu hoá cường độ RAM.
 *
 * Quyết định kiến trúc: Sử dụng Open-Addressing với Linear Probing thay vì
 * Separate Chaining. Mỗi slot lưu trực tiếp (hash, id, occupied) inline trong
 * mảng phẳng, loại bỏ hoàn toàn chi phí `new Node` cho từng chuỗi. Hash được
 * cache lại trong slot để tránh phải tính lại khi rehash hoặc so sánh.
 *
 * Mảng `strings` (DynamicArray) vẫn giữ nguyên vai trò reverse-lookup O(1).
 */
class StringPool {
private:
  /**
   * @brief Slot inline trong mảng phẳng Open-Addressing.
   * Lưu cached hash + dictionary ID. Kích thước: 16 bytes.
   */
  struct Slot {
    unsigned long long cachedHash; ///< Hash đã tính sẵn (tránh rehash chuỗi).
    uint32_t id;                   ///< Dictionary ID trỏ vào mảng strings[].
    bool occupied;                 ///< Cờ đánh dấu slot hợp lệ.
    uint8_t _pad[3];               ///< Padding alignment.

    Slot() : cachedHash(0), id(0), occupied(false) {
      _pad[0] = _pad[1] = _pad[2] = 0;
    }
  };

  Slot *slots;                       ///< Mảng phẳng Open-Addressing.
  uint32_t capacity;                 ///< Kích thước mảng slots.
  uint32_t keyCount;                 ///< Số chuỗi unique đã lưu.
  DynamicArray<std::string> strings; ///< Reverse-lookup array: ID → chuỗi gốc.

  /**
   * @brief Hàm băm chuỗi djb2 nội bộ.
   */
  unsigned long long hashString(const std::string &str) const;

  /**
   * @brief Hàm băm thô (Raw Hash) zero-allocation cho hot-path.
   */
  unsigned long long hashRaw(const char *data, uint32_t length) const;

  /**
   * @brief Rehash khi load factor vượt 75%.
   * Duyệt mảng cũ, insert các slot occupied vào mảng mới.
   */
  void rehash(uint32_t newCapacity);

public:
  explicit StringPool(uint32_t bucketSize = 262147);
  ~StringPool();

  // Vô hiệu hóa Copy/Assignment
  StringPool(const StringPool &other) = delete;
  StringPool &operator=(const StringPool &other) = delete;

  void reserve(uint32_t capacity);

  /**
   * @brief Dictionary Encoding: tra cứu hoặc đăng ký ID cho chuỗi std::string.
   */
  uint32_t getOrCreateId(const std::string &str);

  /**
   * @brief Zero-allocation overload: tra cứu/đăng ký ID từ con trỏ thô.
   */
  uint32_t getOrCreateId(const char *data, uint32_t length);

  /**
   * @brief Reverse Lookup: ID → chuỗi gốc. O(1).
   */
  std::string getString(uint32_t id) const;

  uint32_t size() const;
  uint32_t getBucketCount() const;
};

#endif
