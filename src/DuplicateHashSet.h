// DuplicateHashSet.h
#ifndef DUPLICATE_HASH_SET_H
#define DUPLICATE_HASH_SET_H

#include <cstdint>
#include <string>

/**
 * @brief Cấu trúc Hash Set chuyên biệt lưu trữ dấu vân tay (fingerprint) 64-bit
 * của dữ liệu CSV thô.
 *
 * Quyết định kiến trúc: Cấu trúc này đóng vai trò như một màng lọc dữ liệu
 * (Data Filter/Bloom Filter thay thế) nhằm loại bỏ các dòng log trùng lặp
 * (duplicate) ngay từ khâu đầu vào (Ingestion Phase). Bằng cách băm (hash) trực
 * tiếp chuỗi thô (raw line) ngay khi vừa đọc từ file, hệ thống sẽ bỏ qua ngay
 * các chuỗi đã tồn tại, tiết kiệm triệt để lượng lớn thời gian CPU cho các tác
 * vụ cắt chuỗi (splitting), kiểm tra tính hợp lệ (validation) và tra cứu từ
 * điển (dictionary lookup) vô cùng đắt đỏ.
 */
class DuplicateHashSet {
private:
  /**
   * @brief Node đơn của danh sách liên kết (Linked-list Node) phục vụ giải
   * quyết đụng độ (Collision Resolution).
   *
   * Quyết định kiến trúc: Lựa chọn kỹ thuật Separate Chaining để xử lý Hash
   * Collision. Khác với Open Addressing (Linear Probing), Separate Chaining đảm
   * bảo độ phức tạp thao tác chèn luôn duy trì ổn định ngay cả khi Load Factor
   * (Hệ số tải) của bảng băm tăng cao trong môi trường bộ nhớ hạn hẹp. Kích
   * thước cấu trúc được ép chặt ở mức 16 bytes (8 bytes fingerprint + 8 bytes
   * pointer) nhằm tối ưu hoá bộ nhớ Cache.
   */
  struct FingerprintNode {
    unsigned long long
        fingerprint; ///< Dấu vân tay 64-bit đại diện cho nguyên một dòng CSV.
    FingerprintNode
        *next; ///< Con trỏ liên kết đến node tiếp theo trong cùng một bucket.

    /**
     * @brief Khởi tạo Node chứa mã băm.
     *
     * @param value Mã băm fingerprint 64-bit đầu vào.
     */
    explicit FingerprintNode(unsigned long long value)
        : fingerprint(value), next(nullptr) {}
  };

  FingerprintNode **buckets; ///< Mảng động chứa các con trỏ gốc (Head Pointers)
                             ///< trỏ tới các danh sách liên kết.
  uint32_t bucketCount; ///< Kích thước (số lượng bucket) của bảng băm.
  uint32_t itemCount; ///< Tổng số lượng fingerprint duy nhất đang được lưu trữ.

  /**
   * @brief Tự động tái phân bổ và băm lại (Rehashing) khi hệ số tải vượt ngưỡng.
   *
   * @param newBucketCount Kích thước mới của bảng băm.
   */
  void rehash(uint32_t newBucketCount) {
    FingerprintNode **newBuckets = new FingerprintNode *[newBucketCount];
    for (uint32_t i = 0; i < newBucketCount; ++i) {
      newBuckets[i] = nullptr;
    }

    for (uint32_t i = 0; i < bucketCount; ++i) {
      FingerprintNode *current = buckets[i];
      while (current != nullptr) {
        FingerprintNode *next = current->next;
        uint32_t newIndex = static_cast<uint32_t>(current->fingerprint % newBucketCount);
        current->next = newBuckets[newIndex];
        newBuckets[newIndex] = current;
        current = next;
      }
    }

    delete[] buckets;
    buckets = newBuckets;
    bucketCount = newBucketCount;
  }

public:
  /**
   * @brief Khởi tạo không gian bảng băm (Hash Table) với lượng bucket ấn định
   * trước.
   *
   * Quyết định kiến trúc: Việc cố định kích thước `bucketSize` thay vì tự động
   * mở rộng (auto-resize/rehashing) nhằm triệt tiêu hoàn toàn rủi ro độ trễ đột
   * biến (Latency Spike) O(N) trong luồng đẩy dữ liệu real-time. Việc chia sẵn
   * vùng nhớ con trỏ giúp thao tác khởi tạo đạt tốc độ cực nhanh, tuy nhiên yêu
   * cầu người lập trình phải dự phóng trước (estimate) lượng log lớn nhất để
   * hạn chế độ dài của các chuỗi Chaining.
   *
   * @param bucketSize Số lượng slot bucket sẽ được cấp phát tĩnh cho bảng băm.
   */
  explicit DuplicateHashSet(uint32_t bucketSize)
      : buckets(nullptr), bucketCount(bucketSize), itemCount(0) {
    if (bucketCount == 0) {
      bucketCount = 1;
    }

    buckets = new FingerprintNode *[bucketCount];

    for (uint32_t i = 0; i < bucketCount; ++i) {
      buckets[i] = nullptr;
    }
  }

  /**
   * @brief Quét (walk) toàn bộ cấu trúc mảng và giải phóng bộ nhớ động.
   *
   * Quyết định kiến trúc: Độ phức tạp thời gian thực thi là O(N + M) với N là
   * số bucket, M là tổng số node. Vòng lặp dọn dẹp bộ nhớ thủ công này là bắt
   * buộc (Zero Memory Leak) để rà soát và tiêu huỷ toàn bộ các FingerprintNode
   * đang rải rác trên vùng heap, trước khi phá huỷ nốt khối mảng con trỏ
   * `buckets`.
   */
  ~DuplicateHashSet() {
    for (uint32_t i = 0; i < bucketCount; ++i) {
      FingerprintNode *current = buckets[i];

      while (current != nullptr) {
        FingerprintNode *next = current->next;
        delete current;
        current = next;
      }

      buckets[i] = nullptr;
    }

    delete[] buckets;
    buckets = nullptr;
    bucketCount = 0;
    itemCount = 0;
  }

  /**
   * @brief Cấm hoàn toàn hành vi sao chép tự động (Copy) để bảo vệ quyền sở hữu
   * bộ nhớ.
   *
   * Quyết định kiến trúc: Cấu trúc này chứa các con trỏ Raw quản lý Linked-list
   * và vùng Mảng động phân mảnh. Nếu để C++ thực hiện Shallow Copy mặc định, hệ
   * thống sẽ rơi vào lỗi Double-Free Crash không thể phục hồi khi 2 object độc
   * lập cùng cố gắng giải phóng chung vùng nhớ ở hàm huỷ (Destructor).
   *
   * @param other Đối tượng nguyên bản.
   */
  DuplicateHashSet(const DuplicateHashSet &other) = delete;

  /**
   * @brief Vô hiệu hoá hoàn toàn toán tử gán (Assignment) vì lý do an toàn bộ
   * nhớ tương tự cấu trúc Copy.
   *
   * @param other Đối tượng nguyên bản.
   * @return Xóa thao tác trả về (Deleted).
   */
  DuplicateHashSet &operator=(const DuplicateHashSet &other) = delete;

  /**
   * @brief Thuật toán băm chuỗi djb2 tiêu chuẩn do Dan Bernstein thiết kế.
   *
   * Quyết định thuật toán: Thuật toán djb2 cung cấp tốc độ băm cực kỳ mãnh liệt
   * (O(L) với L là độ dài chuỗi) kết hợp với tỷ lệ đụng độ (collision rate)
   * thấp đáng kinh ngạc, rất tối ưu cho cấu trúc mã ASCII. Các phép toán
   * bitwise (dịch trái 5 bit rồi cộng dồn) rẻ hơn rất nhiều so với lệnh nhân
   * vật lý, giúp tận dụng tối đa năng lực Arithmetic Logic Unit (ALU) của CPU.
   * Đầu ra 64-bit (unsigned long long) đủ sức làm định danh an toàn.
   *
   * @param line Chuỗi CSV thô (Row) cần được băm thành số.
   * @return unsigned long long Vân tay 64-bit đại diện duy nhất cho chuỗi.
   */
  static unsigned long long djb2(const std::string &line) {
    unsigned long long hash = 5381ULL;

    for (uint32_t i = 0; i < line.length(); ++i) {
      hash = ((hash << 5) + hash) + static_cast<unsigned char>(line[i]);
    }

    return hash;
  }

  /**
   * @brief Thẩm định và chèn nguyên bản mã vân tay (fingerprint) vào bảng băm.
   *
   * Thuật toán: Độ phức tạp trung bình (Average Time Complexity) là O(1) nếu Hệ
   * số tải (Load Factor) được cấu hình hợp lý. Trường hợp tồi tệ nhất (Worst
   * Case) là O(K) với K là độ dài chuỗi liên kết tập trung tại một node đụng
   * độ. Bằng cách sáp nhập vòng lặp tra cứu (search) và chèn (insert) vào chung
   * một Single Pass (Một lần quét), hệ thống tiết kiệm được 1/2 chi phí tính
   * toán Hash Offset so với việc gọi hàm tra cứu (contains) trước rồi mới chèn.
   *
   * @param fingerprint Vân tay 64-bit cần thẩm định.
   * @return true Cờ phản hồi nếu hệ thống ghi nhận sự vắng mặt và chèn thành
   * công vân tay mới.
   * @return false Cờ phản hồi nếu vân tay đã có mặt từ trước (Từ chối Ingestion
   * dòng Log này).
   */
  bool insertIfAbsent(unsigned long long fingerprint) {
    if (itemCount >= bucketCount * 2) {
      rehash(bucketCount * 2 + 1);
    }

    uint32_t index = static_cast<uint32_t>(fingerprint % bucketCount);
    FingerprintNode *current = buckets[index];

    while (current != nullptr) {
      if (current->fingerprint == fingerprint) {
        return false;
      }

      current = current->next;
    }

    FingerprintNode *created = new FingerprintNode(fingerprint);
    created->next = buckets[index];
    buckets[index] = created;
    ++itemCount;

    return true;
  }

  /**
   * @brief Truy vấn số lượng vân tay (dữ liệu log không trùng lặp) hiện đang
   * được lưu trữ bảo lưu trong bảng.
   *
   * @return uint32_t Trả về tốc độ O(1) trạng thái bộ đếm `itemCount`.
   */
  uint32_t size() const { return itemCount; }

  /**
   * @brief Truy vấn kích thước hạ tầng mảng Bucket đã cấp phát phân tán.
   *
   * @return uint32_t Lượng bucket cấu hình cứng trên RAM (O(1)).
   */
  uint32_t getBucketCount() const { return bucketCount; }
};

#endif
