// HashIndex.h
#ifndef HASH_INDEX_H
#define HASH_INDEX_H

#include <cstdint>

#include "DynamicArray.h"
#include "LogEntry.h"

/**
 * @brief Cấu trúc dữ liệu Bảng băm (Hash Table) chuyên biệt, đóng vai trò làm
 * Index Server cho hệ thống.
 *
 * @note Thay vì sử dụng `std::unordered_map` có độ trễ lớn do cấp phát từng
 * node nhỏ (overhead), `HashIndex` được tối ưu hóa cho môi trường Zero STL.
 * Bảng băm sử dụng kỹ thuật Separate Chaining (Liên kết rời) với mảng con trỏ
 * thô (raw pointers) và cấp phát thủ công. Khóa (key) là `uint32_t` (Dictionary
 * ID) và giá trị là một `DynamicArray` chứa các con trỏ đến các `LogEntry` gốc
 * (Zero-Copy), đảm bảo thời gian tra cứu trung bình là O(1) và hạn chế phân
 * mảnh bộ nhớ.
 */
class HashIndex {
private:
  /**
   * @brief Nút (Node) cơ bản cấu thành danh sách liên kết đơn (Singly Linked
   * List) trong mỗi Bucket.
   *
   * @note Việc nhóm tất cả các LogEntry có cùng Khóa vào một `DynamicArray`
   * giúp tăng cường Spatial Locality (tính địa phương không gian) cho CPU Cache
   * khi lặp qua dòng thời gian của một đối tượng, hiệu quả hơn đáng kể so với
   * việc tạo từng Node riêng biệt cho mỗi dòng log.
   */
  struct Node {
    uint32_t key;
    DynamicArray<const LogEntry *> entries;
    Node *next;

    /**
     * @brief Khởi tạo một Node mới trong chuỗi xung đột (Collision Chain).
     *
     * @param nodeKey Khóa định danh 32-bit (Dictionary ID của user, device,
     * v.v.).
     *
     * @note Từ khóa `explicit` ngăn chặn lỗi logic do trình biên dịch tự động
     * ép kiểu (implicit conversion).
     */
    explicit Node(uint32_t nodeKey);
  };

  Node **buckets;
  uint32_t bucketCount;
  uint32_t keyCount;

  /**
   * @brief Hàm băm (Hash Function) nội bộ để phân phối khóa.
   *
   * @param key Khóa định danh 32-bit đầu vào.
   * @return Chỉ số bucket tương ứng (từ 0 đến bucketCount - 1).
   *
   * @note Sử dụng thuật toán băm số nguyên nguyên thủy (như MurmurHash3 hoặc
   * modulo đơn giản) với chi phí tính toán cực thấp O(1).
   */
  uint32_t hashKey(uint32_t key) const;

public:
  /**
   * @brief Khởi tạo cấu trúc bảng băm với dung lượng xác định trước.
   *
   * @param bucketSize Số lượng bucket ban đầu (Nên là số nguyên tố để giảm
   * thiểu xung đột).
   *
   * @note Độ phức tạp O(N) với N là `bucketSize`. Cấp phát mảng con trỏ thô
   * `new Node*[bucketCount]` và khởi tạo rỗng để tránh rác bộ nhớ. Việc xác
   * định trước kích thước giúp vô hiệu hóa hoàn toàn quá trình Rehashing cực kỳ
   * tốn kém trong thời gian chạy.
   */
  explicit HashIndex(uint32_t bucketSize);

  /**
   * @brief Hàm hủy (Destructor) dọn dẹp toàn bộ tài nguyên.
   *
   * @note Độ phức tạp O(B + N) với B là số bucket và N là số lượng khóa (keys).
   * @warning Hàm này chỉ phá hủy cấu trúc Index (giải phóng mảng `buckets` và
   * các `Node`), KHÔNG giải phóng các đối tượng `LogEntry` gốc để tuân thủ
   * nguyên tắc quyền sở hữu (Ownership).
   */
  ~HashIndex();

  // Vô hiệu hóa Copy Constructor và Assignment Operator để tránh lỗi
  // Double-Free do quản lý con trỏ thô.
  HashIndex(const HashIndex &other) = delete;
  HashIndex &operator=(const HashIndex &other) = delete;

  /**
   * @brief Thêm một tham chiếu (con trỏ) `LogEntry` vào chỉ mục.
   *
   * @param key Khóa định danh 32-bit làm tiêu chí phân cụm.
   * @param entry Con trỏ hằng (const pointer) trỏ đến log gốc được phân bổ
   * trong Arena/Chunk.
   *
   * @note Độ phức tạp trung bình O(1), trường hợp xấu nhất O(K) với K là số
   * phần tử xung đột trong bucket. Nếu key đã tồn tại, con trỏ mới sẽ được gắn
   * thêm (append) vào `DynamicArray` ở chế độ Amortized O(1).
   */
  void insert(uint32_t key, const LogEntry *entry);

  /**
   * @brief Truy xuất danh sách toàn bộ các sự kiện ứng với một Khóa.
   *
   * @param key Khóa định danh 32-bit.
   * @return Con trỏ hằng trỏ đến mảng động chứa danh sách `LogEntry*`, hoặc
   * `nullptr` nếu không tìm thấy.
   *
   * @note Độ phức tạp trung bình O(1). Chữ ký hàm sử dụng `const` nghiêm ngặt
   * để đảm bảo tính bất biến (immutability) của Index đối với các luồng truy
   * vấn.
   */
  const DynamicArray<const LogEntry *> *get(uint32_t key) const;

  /**
   * @brief Sắp xếp toàn bộ dữ liệu log của tất cả các khóa theo trình tự thời
   * gian.
   *
   * @note Độ phức tạp hệ thống O(N * K log K) với N là số lượng khóa và K là
   * kích thước mảng trung bình. Được gọi sau khi hoàn tất giai đoạn tải dữ liệu
   * hàng loạt (Bulk Loading) nhằm chuẩn bị cho quá trình phân tích chuỗi hành
   * vi hoặc tìm kiếm nhị phân (Binary Search).
   */
  void sortAllTimelines();

  /**
   * @brief Trả về tổng số lượng khóa duy nhất (unique keys) đã được lập chỉ
   * mục.
   */
  uint32_t size() const;

  /**
   * @brief Trả về dung lượng (số lượng bucket) cấu hình của Index.
   */
  uint32_t getBucketCount() const;
};

#endif
