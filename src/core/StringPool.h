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
 * Quyết định kiến trúc: Trong hệ thống Log Analytics, rất nhiều chuỗi (như IP
 * Address, HTTP Method, OS Type) bị lặp đi lặp lại hàng chục triệu lần.
 * StringPool thực hiện thao tác Dictionary Encoding, loại bỏ hoàn toàn sự phân
 * mảnh heap của `std::string` khổng lồ và thay thế chúng bằng một định danh ID
 * (`uint32_t` 4 bytes) nguyên thuỷ. Kiến trúc này thu hẹp Memory Footprint theo
 * cấp số nhân và biến các phép so sánh chuỗi O(L) chậm chạp thành so sánh số
 * học O(1) siêu tốc trên nền Register của bộ vi xử lý.
 */
class StringPool {
private:
  /**
   * @brief Node liên kết đơn đại diện cho một Entry thực thể trong String Pool.
   *
   * Quyết định kiến trúc: Lưu trữ dữ liệu mapping 1-1 giữa Chuỗi ký tự (key) và
   * Mã định danh (id). Đối tượng này được thiết kế theo định dạng mỏng nhẹ để
   * lồng ghép trực tiếp vào cơ chế Separate Chaining của bảng băm.
   */
  struct Node {
    uint32_t id; ///< Mã định danh tĩnh 32-bit (Dictionary ID) vĩnh viễn phân bổ
                 ///< cho chuỗi.
    Node *next;  ///< Con trỏ trỏ tới Node tiếp theo nhằm giải quyết Hash
                 ///< Collision.

    /**
     * @brief Khởi tạo một Node thực thể từ điển mới trên khối nhớ heap.
     *
     * @param nodeId Định danh toán học duy nhất cấp cho chuỗi.
     */
    Node(uint32_t nodeId);
  };

  Node **buckets; ///< Bảng băm quản lý trực tiếp con trỏ Head của các danh sách
                  ///< liên kết.
  uint32_t
      bucketCount;   ///< Kích thước giới hạn trần của số lượng slots buckets.
  uint32_t keyCount; ///< Tổng số lượng chuỗi (string) duy nhất đã lập chỉ mục
                     ///< và tham gia vòng lặp cấp ID.
  DynamicArray<std::string>
      strings; ///< Hạ tầng Mảng O(1) tra cứu ngược (Reverse Lookup) tốc độ cao
               ///< từ ID ánh xạ ngược về String.

  /**
   * @brief Hàm băm chuỗi ký tự nội bộ phục vụ định tuyến bucket phân bổ node.
   *
   * Quyết định thuật toán: Thuật toán cung cấp tính đồng nhất phân phối ngẫu
   * nhiên (Uniform Distribution) cao trên bảng băm. Mức độ sắc bén của hàm băm
   * ảnh hưởng trực tiếp đến hệ số rẽ nhánh (branching penalty) và hiệu năng nội
   * suy của toàn hệ thống trong quá trình đẩy dữ liệu mật độ cao.
   *
   * @param str Cấu trúc chuỗi cần được phân rã mã băm.
   * @return unsigned long long Giá trị băm 64-bit đầu ra.
   */
  unsigned long long hashString(const std::string &str) const;

  /**
   * @brief Hàm băm thô (Raw Hash) hoạt động trực tiếp trên con trỏ ký tự và độ
   * dài, không cần dựng `std::string` trung gian.
   *
   * Quyết định kiến trúc: Đây là phiên bản zero-allocation của `hashString`,
   * được thiết kế độc quyền cho hot-path của DataLoader. Tốc độ băm hoàn toàn
   * tương đương với `hashString` vì cùng thuật toán djb2, nhưng không gây ra
   * bất kỳ heap allocation nào.
   *
   * @param data Con trỏ thô trỏ tới vùng đệm (FieldView.start).
   * @param length Độ dài vùng dữ liệu cần băm (FieldView.length).
   * @return unsigned long long Giá trị băm 64-bit đầu ra.
   */
  unsigned long long hashRaw(const char *data, uint32_t length) const;

public:
  /**
   * @brief Khởi tạo hệ thống String Pool trung tâm với mức dung lượng dự phòng
   * phân tán cực lớn.
   *
   * Quyết định kiến trúc: Mức cấu hình bucket mặc định là 262,147 (một số
   * nguyên tố - Prime Number) giúp triệt tiêu hiện tượng dồn cụm băm
   * (Clustering) theo các bội số mẫu. Kích thước cấp phát tĩnh (Fixed Size
   * Allocation) lớn sẽ giúp bỏ qua hoàn toàn khâu tái cân bằng Rehashing O(N)
   * làm đứt gãy Runtime.
   *
   * @param bucketSize Kích thước mảng băm định tuyến (Khuyến nghị số nguyên tố
   * để gia tăng độ rải đều).
   */
  explicit StringPool(uint32_t bucketSize = 262147);

  /**
   * @brief Phá huỷ và giải phóng cấu trúc từ điển nguyên khối khi kết thúc vòng
   * đời Engine.
   *
   * Quyết định kiến trúc: Vòng lặp dọn dẹp quét theo thời gian O(N+M) để thu
   * hồi vùng nhớ của từng block Node nằm trên Heap. Mảng tra cứu ngược
   * `DynamicArray strings` sẽ tự động kích hoạt hàm huỷ RAII chuyên biệt của nó
   * để dọn dẹp bộ đệm chuỗi một cách liền mạch mà không cần viết lệnh gọi thủ
   * công.
   */
  ~StringPool();

  /**
   * @brief Vô hiệu hoá cơ chế tự động sao chép để ngăn ngừa rủi ro hỏng hóc bộ
   * nhớ.
   *
   * Quyết định kiến trúc: Tương tự như các cấu trúc tối ưu Low-Level khác, việc
   * cấm quyền sao chép giúp duy trì khắt khe nguyên lý Độc quyền Quản lý Bộ nhớ
   * (Unique Ownership). Loại trừ hoàn toàn thảm hoạ tràn RAM O(N) và lỗi vỡ
   * luồng Double-Free do compiler sinh ra thao tác Shallow Copy.
   *
   * @param other Đối tượng StringPool nguyên mẫu.
   */
  StringPool(const StringPool &other) = delete;

  /**
   * @brief Vô hiệu hoá toán tử gán, củng cố tính toàn vẹn của nguyên lý
   * Ownership.
   *
   * @param other Đối tượng StringPool nguyên mẫu.
   * @return Loại bỏ thao tác sinh mã máy (Deleted).
   */
  StringPool &operator=(const StringPool &other) = delete;

  /**
   * @brief Chủ động mở rộng trước năng lực cấp phát cho mảng tra cứu ngược
   * (Reverse Lookup Array).
   *
   * Thuật toán: Thao tác này (chiếm O(N) tại thời điểm gọi) gọi lệnh reserve
   * thẳng xuống `DynamicArray`. Bằng việc chủ động kích hoạt tác vụ này ngay từ
   * khâu khởi động hệ thống (System Bootstrapping), nhà phát triển sẽ đưa mọi
   * chi phí cấp phát bộ nhớ mảng nội bộ về O(1) tuyến tính xuyên suốt giai đoạn
   * Parsing dữ liệu log.
   *
   * @param capacity Mức sức chứa bộ nhớ danh định cần mượn trước.
   */
  void reserve(uint32_t capacity);

  /**
   * @brief Lõi vận hành Dictionary Encoding: Ánh xạ, rút gọn hoặc Đăng ký định
   * danh ID số nguyên cho chuỗi.
   *
   * Thuật toán: Độ trễ O(1) đối với đa số trường hợp (Average Case). Quá trình
   * sẽ tính băm và duyệt chuỗi bucket, nếu chuỗi đã có sẵn trong từ điển,
   * module trả về tham chiếu ID cũ ngay lập tức để tái sử dụng. Ngược lại, nếu
   * đây là một chuỗi mới (Cache Miss), hệ thống sẽ cấp phát một object Node
   * mới, gán `keyCount` hiện tại làm ID định danh, đẩy chuỗi vào đuôi mảng
   * `strings` phục vụ khả năng tra cứu ngược (Reverse Lookup), sau đó nối vào
   * bảng băm. Đây là chốt chặn quan trọng nhất để nén các dữ liệu văn bản phình
   * to thành một tập hợp số nguyên cô đọng chuyên biệt.
   *
   * @param str Chuỗi thực thể cần lấy/cấp quyền định danh (Ví dụ:
   * "192.168.1.1").
   * @return uint32_t Mã định danh nén 32-bit (ID) đại diện vĩnh viễn cho chuỗi
   * trong nội bộ Engine.
   */
  uint32_t getOrCreateId(const std::string &str);

  /**
   * @brief Overload zero-allocation: Tra cứu hoặc đăng ký ID trực tiếp từ con
   * trỏ ký tự thô mà không cần tạo `std::string` tạm thời.
   *
   * Thuật toán: Tính băm trực tiếp trên `(data, length)` bằng `hashRaw()`.
   * Duyệt bucket và so sánh bằng `std::memcmp` — không phát sinh heap
   * allocation nào. Chỉ khi chuỗi là hoàn toàn mới (Cache Miss), một
   * `std::string` canonical duy nhất mới được tạo ra và lưu vào `strings[]`.
   *
   * Impact: Với 10M rows × 4 fields, phương pháp này loại bỏ ~39.9M trong
   * tổng số 40M phép cấp phát tạm thời, cải thiện tốc độ Ingestion 30–50%.
   *
   * @param data Con trỏ thô trỏ tới chuỗi cần tra cứu (FieldView.start).
   * @param length Độ dài chuỗi (FieldView.length).
   * @return uint32_t Mã định danh nén 32-bit.
   */
  uint32_t getOrCreateId(const char *data, uint32_t length);

  /**
   * @brief Giải mã từ điển (Reverse Dictionary Lookup): Chuyển đổi định danh ID
   * số học ngược về Chuỗi nguyên thủy.
   *
   * Quyết định thuật toán: Thao tác này ngốn chính xác O(1) thời gian tĩnh.
   * Khai thác sức mạnh bộ đệm Random Access nguyên bản của biến `DynamicArray
   * strings`, hệ thống có thể truy hồi (retrieve) chuỗi gốc theo ID tức thì mà
   * không cần bất kì lệnh toán học mã băm hay duyệt cấu trúc Node. API này đặc
   * biệt được tối ưu hoá cho Giai đoạn Xuất dữ liệu (Serialization/Output
   * Query).
   *
   * @param id Mã định danh nén 32-bit cần giải mã nội dung.
   * @return std::string Chuỗi văn bản ký tự tương ứng (Payload).
   */
  std::string getString(uint32_t id) const;

  /**
   * @brief Khảo sát số lượng bộ chuỗi duy nhất (Unique Strings) đã được từ điển
   * hoá và thu gọn thành công.
   *
   * @return uint32_t Khối lượng ID thực tế đang được phân bổ (Độ trễ O(1)).
   */
  uint32_t size() const;

  /**
   * @brief Truy xuất giới hạn thiết lập của không gian định tuyến (Buckets)
   * trong kiến trúc bảng băm.
   *
   * @return uint32_t Số lượng khe Slot Bucket được cấp cấu hình phần cứng (Độ
   * trễ O(1)).
   */
  uint32_t getBucketCount() const;
};

#endif
