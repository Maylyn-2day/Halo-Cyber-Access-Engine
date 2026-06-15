// DynamicArray.h
#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <cstdint>

/**
 * @brief Cấu trúc mảng động (Dynamic Array) tuỳ chỉnh tối ưu hoá hiệu suất cao
 * cho In-Memory Analytics.
 *
 * Mảng động này được thiết kế theo tư tưởng "Zero STL", loại bỏ hoàn toàn các
 * meta-data dư thừa và overhead ngữ nghĩa của std::vector. Bằng cách quản lý
 * trực tiếp một khối bộ nhớ liên tục (contiguous memory block) thông qua con
 * trỏ raw, cấu trúc này tối đa hoá spatial locality, tăng tỷ lệ Cache Hit
 * L1/L2/L3 khi CPU thực hiện quét tuần tự (sequential scan) qua hàng triệu dòng
 * log.
 *
 * @tparam T Kiểu dữ liệu của các phần tử.
 *
 * @warning Quyền sở hữu bộ nhớ (Memory Ownership): Cấu trúc này tự quản lý mảng
 * vùng nhớ heap của chính nó. Tuy nhiên, nếu `T` là một con trỏ (ví dụ:
 * `char*`), người gọi (caller) có trách nhiệm phải thực hiện thao tác duyệt
 * mảng và gọi lệnh giải phóng các đối tượng được trỏ tới trước khi cấu trúc này
 * bị hủy để ngăn chặn hoàn toàn rò rỉ bộ nhớ (Memory Leak).
 */
template <typename T> class DynamicArray {
private:
  T *data;
  uint32_t length;
  uint32_t capacity;

  /**
   * @brief Tự động tái cấp phát (Reallocation) bộ nhớ khi mảng chạm giới hạn
   * sức chứa.
   *
   * Quyết định thuật toán: Chiến lược cấp phát này có chi phí thời gian là O(N)
   * do cần phải di chuyển khối lượng lớn dữ liệu sang vùng nhớ mới kề liền
   * nhau. Việc sao chép trực tiếp bằng toán tử gán thay vì std::move là sự đánh
   * đổi có chủ đích cho các cấu trúc dữ liệu dạng POD/Trivial, nhằm giữ độ phức
   * tạp của logic kiểm soát bộ nhớ ở mức độ nguyên thuỷ và nhanh nhất.
   *
   * @param newCapacity Sức chứa bộ nhớ mới cần xin cấp phát từ hệ điều hành.
   */
  void resize(uint32_t newCapacity) {
    T *newData = new T[newCapacity];

    for (uint32_t i = 0; i < length; ++i) {
      newData[i] = data[i];
    }

    delete[] data;
    data = newData;
    capacity = newCapacity;
  }

public:
  /**
   * @brief Trì hoãn việc cấp phát bộ nhớ (Lazy Initialization) cho đến khi phần
   * tử đầu tiên được chèn.
   *
   * Quyết định kiến trúc: Kỹ thuật này giúp tiết kiệm đáng kể lượng RAM dư thừa
   * nếu hệ thống phải khởi tạo trước hàng ngàn mảng động rỗng dưới dạng các
   * bucket trong phân vùng Hash Table. Thời gian khởi tạo cấu trúc đạt chuẩn
   * O(1) tuyệt đối.
   */
  DynamicArray() : data(nullptr), length(0), capacity(0) {}

  /**
   * @brief Dự phòng sẵn một khối vùng nhớ liên tục (Pre-allocation) để tránh
   * phân mảnh và thắt nút cổ chai.
   *
   * Quyết định kiến trúc: Tác vụ nạp (ingestion) hàng triệu dòng CSV vào RAM
   * yêu cầu loại bỏ hoàn toàn các lệnh gọi syscall xin cấp phát động (dynamic
   * allocation) đắt đỏ đứt quãng. Bằng cách thiết lập mức initialCapacity lớn
   * ngay từ đầu, tốc độ đẩy dữ liệu vào bộ nhớ đạt O(1) tuyến tính thuần tuý,
   * ngăn ngừa triệt để chi phí O(N) lúc resize.
   *
   * @param initialCapacity Kích thước bộ nhớ ban đầu (tính theo số lượng
   * element `T`) cần giành quyền cấp phát trước.
   */
  explicit DynamicArray(uint32_t initialCapacity)
      : data(nullptr), length(0), capacity(0) {
    reserve(initialCapacity);
  }

  /**
   * @brief Đảm bảo giải phóng sạch sẽ khối heap theo nguyên lý RAII (Resource
   * Acquisition Is Initialization).
   *
   * Quyết định kiến trúc: Thời gian dọn dẹp bộ nhớ là O(1) đối với các chuỗi
   * POD data. Lệnh `delete[]` sẽ thực thi ngầm hàm hủy (nếu có) đối với từng
   * phần tử. Tư duy quản trị bộ nhớ thủ công bằng raw pointer giúp triệt tiêu
   * đi các overhead truy xuất và đếm tham chiếu (Reference Counting) từ
   * thread-safety mà các smart pointer (e.g., std::shared_ptr) áp đặt.
   */
  ~DynamicArray() {
    delete[] data;
    data = nullptr;
    length = 0;
    capacity = 0;
  }

  /**
   * @brief Vô hiệu hoá cơ chế Deep Copy tự động để áp đặt chính sách Độc Quyền
   * Quản Trị (Unique Ownership).
   *
   * Quyết định kiến trúc: Việc sao chép khối mảng khổng lồ vô tình dẫn tới sụt
   * giảm hiệu năng ở mức nghiêm trọng (O(N) memory & CPU overhead). Quan trọng
   * hơn, vô hiệu hóa copy logic sẽ giúp cô lập các con trỏ raw, bảo vệ hệ thống
   * tuyệt đối trước các rủi ro sập luồng (Double-Free Crash) khi có sự cố thao
   * tác chéo vùng nhớ.
   *
   * @param other Đối tượng DynamicArray nguyên bản tham chiếu.
   */
  DynamicArray(const DynamicArray &other) = delete;

  /**
   * @brief Vô hiệu hóa toán tử gán (Assignment Operator) nhằm đồng nhất tính
   * nguyên vẹn quyền sở hữu bộ nhớ của cấu trúc.
   *
   * @param other Đối tượng DynamicArray nguyên bản tham chiếu.
   * @return Bị cấm (Deleted).
   */
  DynamicArray &operator=(const DynamicArray &other) = delete;

  /**
   * @brief Khởi tạo di chuyển (Move Constructor). Chuyển giao quyền sở hữu bộ nhớ một cách an toàn.
   *
   * Quyết định kiến trúc: Giúp các bộ phân tích Anomaly có thể trả về một mảng kết quả 
   * (Return by value) mà không bị sụt giảm hiệu năng. Cấu trúc lấy trộm (steal) con trỏ 
   * từ R-value và reset đối tượng cũ về trạng thái rỗng.
   *
   * @param other Đối tượng R-value tham chiếu.
   */
  DynamicArray(DynamicArray &&other) noexcept
      : data(other.data), length(other.length), capacity(other.capacity) {
    other.data = nullptr;
    other.length = 0;
    other.capacity = 0;
  }

  /**
   * @brief Toán tử gán di chuyển (Move Assignment). Giải phóng vùng nhớ cũ và tiếp quản vùng nhớ mới.
   *
   * @param other Đối tượng R-value tham chiếu.
   * @return Tham chiếu đến đối tượng hiện tại.
   */
  DynamicArray &operator=(DynamicArray &&other) noexcept {
    if (this != &other) {
      delete[] data;
      data = other.data;
      length = other.length;
      capacity = other.capacity;

      other.data = nullptr;
      other.length = 0;
      other.capacity = 0;
    }
    return *this;
  }

  /**
   * @brief Can thiệp thủ công vào cơ chế cấp phát không gian vùng nhớ (Manual
   * Capacity Allocation).
   *
   * Thuật toán: Nếu capacity yêu cầu không lớn hơn dung lượng hiện tại, hàm
   * ngắt ngay lập tức ở O(1). Ngược lại, hệ thống sẽ kích hoạt cơ chế
   * Reallocation tốn O(N) thời gian sao chép. Thao tác này tối quan trọng đối
   * với các luồng xử lý hot-path nhằm dọn dẹp nguy cơ phân mảnh không gian bộ
   * nhớ (Memory Fragmentation).
   *
   * @param requestedCapacity Sức chứa bộ nhớ tối đa yêu cầu được cấp phát
   * trước.
   */
  void reserve(uint32_t requestedCapacity) {
    if (requestedCapacity <= capacity) {
      return;
    }

    T *newData = new T[requestedCapacity];

    for (uint32_t i = 0; i < length; ++i) {
      newData[i] = data[i];
    }

    delete[] data;
    data = newData;
    capacity = requestedCapacity;
  }

  /**
   * @brief Chèn dữ liệu tuần tự vào buffer với chiến lược nội suy cấp số nhân
   * (Exponential Growth).
   *
   * Thuật toán: Thời gian chạy phân bổ trung bình (Amortized Time Complexity)
   * đạt chuẩn O(1). Trong trường hợp tải dữ liệu chạm đỉnh (length ==
   * capacity), quá trình di dời dữ liệu nội bộ O(N) với Growth Factor = 2 sẽ tự
   * động kích hoạt. Mức Capacity sơ khai khởi điểm được ấn định cứng là 8 phần
   * tử giúp giảm thiểu không gian thừa thải đối với các tập dữ liệu rải rác
   * siêu nhỏ.
   *
   * @param value Dữ liệu/object cần nối thêm vào đuôi chuỗi vùng nhớ.
   */
  void pushBack(const T &value) {
    if (length == capacity) {
      uint32_t nextCapacity = capacity == 0 ? 8 : capacity * 2;
      resize(nextCapacity);
    }

    data[length] = value;
    ++length;
  }

  /**
   * @brief Giao diện truy xuất ngẫu nhiên (Random Access) tiếp xúc trực tiếp
   * trần với không gian bộ nhớ.
   *
   * Quyết định kiến trúc: Loại bỏ hoàn toàn các lệnh if-else kiểm tra biên an
   * toàn (bounds-checking) để không làm gãy pipeline thực thi lệnh của CPU
   * (branch prediction miss). Việc truy cập index được ánh xạ trực tiếp về
   * phương trình tính offset vùng nhớ O(1) chuẩn ngôn ngữ C: `base_address +
   * index * sizeof(T)`.
   *
   * @warning Cảnh báo luồng: Caller tự chịu hoàn toàn trách nhiệm đảm bảo biến
   * `index < length`. Hành vi sẽ là Không xác định (Undefined Behavior) nếu
   * truy xuất ra ngoài ranh giới vùng nhớ phân bổ (gây Segfault).
   *
   * @param index Vị trí offset cần được truy xuất.
   * @return T& Tham chiếu pointer trực tiếp tới slot dữ liệu thực tế nằm trên
   * RAM.
   */
  T &operator[](uint32_t index) { return data[index]; }

  /**
   * @brief Giao diện truy xuất ngẫu nhiên Read-Only theo chỉ mục bảo đảm độ trễ
   * O(1) cực kì tối thiểu.
   *
   * @param index Vị trí offset cần được đọc thông tin.
   * @return const T& Tham chiếu hằng (const) không cho phép thay đổi dữ liệu
   * gốc nằm trên RAM.
   */
  const T &operator[](uint32_t index) const { return data[index]; }

  /**
   * @brief Query tức thời khối lượng tài nguyên dữ liệu thực sự đang chiếm dụng
   * trong khối buffer.
   *
   * @return uint32_t Chiều dài (Length) hiện tại của tập dữ liệu đang được
   * index hoá (Độ phức tạp O(1)).
   */
  uint32_t size() const { return length; }

  /**
   * @brief Đánh giá quy mô tối đa của khối RAM vật lý (tính theo slot lưu trữ)
   * đã mượn cấp phát thành công.
   *
   * @return uint32_t Mức sức chứa tối đa mà không gây tràn trước khi tái cấp
   * phát (Độ phức tạp O(1)).
   */
  uint32_t getCapacity() const { return capacity; }

  /**
   * @brief Phơi bày con trỏ gốc (Base Pointer) của luồng heap ra bên ngoài cho
   * các thao tác C-Library mức thấp.
   *
   * Quyết định kiến trúc: Là cửa ngõ (gateway) sống còn đối với các dự án
   * Low-level & Zero STL. Chức năng này cho phép truyền toàn bộ khối memory
   * buffer gốc vào thẳng các system API như `write()`, `send()` (Socket I/O)
   * hay `memcpy()` mà không phải thông qua bất kì layer mã hoá trung gian nào.
   *
   * @return T* Con trỏ Raw nắm điểm bắt đầu vùng nhớ liên tục ở ô index 0.
   */
  T *raw() { return data; }

  /**
   * @brief Trích xuất con trỏ gốc dưới hình thức chống ghi đè cho mục đích đẩy
   * dữ liệu read-only thuần túy.
   *
   * @return const T* Con trỏ Raw đảm bảo tính toàn vẹn bộ đệm đọc.
   */
  const T *raw() const { return data; }

  /**
   * @brief Cơ chế tái sử dụng vùng nhớ nhanh chóng triệt tiêu hoàn toàn lệnh
   * tương tác với Kernel (Memory Reuse).
   *
   * Quyết định kiến trúc: Bằng cách chỉ ghi đè con trỏ biến nội `length = 0`
   * (Thời gian chạy O(1) tuyệt đối), toàn bộ khối mảng hiện hữu sẽ sẵn sàng ghi
   * nhận cho một phiên Batch Processing mới. Đây là lõi kĩ thuật đằng sau các
   * mẫu thiết kế Object Pooling và Arena Allocation, giúp cản trở lệnh
   * `delete[]` và `new[]` liên tục bào mòn đi hiệu suất phân bổ từ hệ điều
   * hành.
   */
  void clear() { length = 0; }
};

#endif
