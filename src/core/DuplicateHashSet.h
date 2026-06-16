// DuplicateHashSet.h
#ifndef DUPLICATE_HASH_SET_H
#define DUPLICATE_HASH_SET_H

#include <cstdint>
#include <cstring>
#include <string>

/**
 * @brief Cấu trúc Hash Set chuyên biệt lưu trữ dấu vân tay (fingerprint)
 * 64-bit của dữ liệu CSV thô.
 *
 * Quyết định kiến trúc: Sử dụng kỹ thuật Open-Addressing với Robin Hood
 * Hashing thay vì Separate Chaining. Toàn bộ entry được lưu inline trong
 * một mảng phẳng (flat array), loại bỏ hoàn toàn chi phí cấp phát heap
 * cho từng node (10M+ lần `new` → 0 lần). Điều này tăng Cache Locality
 * đáng kể vì CPU prefetcher chỉ cần quét tuần tự trên 1 vùng nhớ liền mạch
 * thay vì nhảy lung tung trên heap.
 *
 * Robin Hood Hashing: Khi xảy ra collision, entry mới sẽ "cướp" vị trí
 * của entry có khoảng cách thăm dò (probe distance) ngắn hơn. Điều này
 * giữ cho probe distance trung bình cực kỳ thấp (thường < 2), đảm bảo
 * hiệu năng O(1) thực tế ngay cả khi load factor lên tới 70-80%.
 */
class DuplicateHashSet {
private:
  /**
   * @brief Slot inline trong mảng phẳng. Không cần con trỏ `next`.
   * Kích thước: 16 bytes (8 fingerprint + 4 probeDistance + 4 padding/flags).
   */
  struct Slot {
    unsigned long long fingerprint; ///< Dấu vân tay 64-bit.
    uint32_t
        probeDistance; ///< Khoảng cách từ bucket gốc (Robin Hood metadata).
    bool occupied;     ///< Cờ đánh dấu slot đang chứa dữ liệu hợp lệ.
    uint8_t _pad[3];   ///< Padding alignment.

    Slot() : fingerprint(0), probeDistance(0), occupied(false) {
      _pad[0] = _pad[1] = _pad[2] = 0;
    }
  };

  Slot *slots;        ///< Mảng phẳng chứa toàn bộ entries inline.
  uint32_t capacity;  ///< Kích thước mảng (số lượng slots).
  uint32_t itemCount; ///< Số lượng fingerprint duy nhất đang lưu.

  /**
   * @brief Tự động mở rộng và rehash khi load factor vượt ngưỡng 75%.
   *
   * Robin Hood rehash: Duyệt mảng cũ, chỉ insert các slot occupied vào
   * mảng mới. Không cần duyệt linked-list → nhanh hơn Separate Chaining rehash.
   */
  void rehash(uint32_t newCapacity) {
    Slot *oldSlots = slots;
    uint32_t oldCapacity = capacity;

    slots = new Slot[newCapacity]();
    capacity = newCapacity;
    itemCount = 0;

    for (uint32_t i = 0; i < oldCapacity; ++i) {
      if (oldSlots[i].occupied) {
        insertInternal(oldSlots[i].fingerprint);
      }
    }

    delete[] oldSlots;
  }

  /**
   * @brief Insert nội bộ dùng Robin Hood probing.
   * Được gọi bởi cả insertIfAbsent (public) và rehash (private).
   */
  void insertInternal(unsigned long long fingerprint) {
    uint32_t index = static_cast<uint32_t>(fingerprint % capacity);
    uint32_t dist = 0;

    while (true) {
      if (!slots[index].occupied) {
        // Slot trống → chèn vào
        slots[index].fingerprint = fingerprint;
        slots[index].probeDistance = dist;
        slots[index].occupied = true;
        ++itemCount;
        return;
      }

      // Robin Hood: nếu entry hiện tại "giàu hơn" (probe distance ngắn hơn),
      // ta cướp vị trí của nó và đẩy nó đi tìm chỗ mới.
      if (slots[index].probeDistance < dist) {
        // Swap: entry hiện tại bị đẩy ra
        unsigned long long tmpFp = slots[index].fingerprint;
        uint32_t tmpDist = slots[index].probeDistance;

        slots[index].fingerprint = fingerprint;
        slots[index].probeDistance = dist;

        fingerprint = tmpFp;
        dist = tmpDist;
      }

      // Linear probing: tiến sang slot kế tiếp
      index = (index + 1) % capacity;
      ++dist;
    }
  }

public:
  /**
   * @brief Khởi tạo bảng băm Open-Addressing với capacity cho trước.
   *
   * @param bucketSize Kích thước mảng slots (Khuyến nghị số nguyên tố).
   */
  explicit DuplicateHashSet(uint32_t bucketSize)
      : slots(nullptr), capacity(bucketSize), itemCount(0) {
    if (capacity == 0) {
      capacity = 1;
    }

    slots = new Slot[capacity]();
  }

  /**
   * @brief Giải phóng mảng phẳng. Chỉ 1 lệnh delete[] duy nhất.
   * So với Separate Chaining: tiết kiệm O(N) lần delete node.
   */
  ~DuplicateHashSet() {
    delete[] slots;
    slots = nullptr;
    capacity = 0;
    itemCount = 0;
  }

  // Vô hiệu hóa Copy để bảo vệ quyền sở hữu bộ nhớ.
  DuplicateHashSet(const DuplicateHashSet &other) = delete;
  DuplicateHashSet &operator=(const DuplicateHashSet &other) = delete;

  /**
   * @brief Thuật toán băm chuỗi djb2 chuẩn.
   */
  static unsigned long long djb2(const std::string &line) {
    unsigned long long hash = 5381ULL;

    for (uint32_t i = 0; i < line.length(); ++i) {
      hash = ((hash << 5) + hash) + static_cast<unsigned char>(line[i]);
    }

    return hash;
  }

  /**
   * @brief Thẩm định và chèn fingerprint bằng Robin Hood probing.
   *
   * Thuật toán: O(1) trung bình. Probe distance trung bình với Robin Hood
   * thường < 2 ngay cả ở load factor 70%. Nếu load factor vượt 75%,
   * tự động rehash để duy trì hiệu năng.
   *
   * @param fingerprint Vân tay 64-bit cần thẩm định.
   * @return true nếu fingerprint mới (đã chèn thành công).
   * @return false nếu fingerprint đã tồn tại (từ chối dòng trùng lặp).
   */
  bool insertIfAbsent(unsigned long long fingerprint) {
    // Rehash nếu load factor > 75%
    if (itemCount * 4 >= capacity * 3) {
      rehash(capacity * 2 + 1);
    }

    uint32_t index = static_cast<uint32_t>(fingerprint % capacity);
    uint32_t dist = 0;

    while (true) {
      if (!slots[index].occupied) {
        // Slot trống → fingerprint chưa tồn tại → chèn
        slots[index].fingerprint = fingerprint;
        slots[index].probeDistance = dist;
        slots[index].occupied = true;
        ++itemCount;
        return true;
      }

      // Fingerprint đã tồn tại → từ chối
      if (slots[index].fingerprint == fingerprint) {
        return false;
      }

      // Robin Hood: nếu probe distance hiện tại ngắn hơn của ta,
      // ta chắc chắn fingerprint không nằm sau vị trí này nữa
      // (tính chất Robin Hood invariant).
      // Tuy nhiên, để an toàn với rehash, ta vẫn tiếp tục probing
      // cho đến khi gặp slot trống hoặc match.

      // Nếu entry hiện tại "giàu hơn" → cướp vị trí (Robin Hood swap)
      if (slots[index].probeDistance < dist) {
        // Swap và tiếp tục tìm chỗ cho entry bị đẩy ra
        unsigned long long tmpFp = slots[index].fingerprint;
        uint32_t tmpDist = slots[index].probeDistance;

        slots[index].fingerprint = fingerprint;
        slots[index].probeDistance = dist;

        fingerprint = tmpFp;
        dist = tmpDist;
      }

      index = (index + 1) % capacity;
      ++dist;
    }
  }

  uint32_t size() const { return itemCount; }
  uint32_t getBucketCount() const { return capacity; }
};

#endif
