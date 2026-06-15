// DataLoader.h
#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include <string>

#include "DuplicateHashSet.h"
#include "LogStore.h"

/**
 * @brief Hệ thống Ingestion dữ liệu tốc độ siêu cao (High-speed CSV Ingestion).
 *
 * Quyết định kiến trúc: DataLoader được thiết kế theo nguyên lý
 * "Zero-Allocation Parsing". Thay vì sử dụng `std::getline`,
 * `std::stringstream` hay `std::string` vô cùng đắt đỏ và gây tắc nghẽn bộ nhớ
 * do cấp phát động liên tục, lớp này đọc trực tiếp khối byte lớn (Chunk) thông
 * qua `FILE*` và `fread`. Dữ liệu dòng (line) được bóc tách bằng các con trỏ
 * Raw Character (`FieldView`) trỏ thẳng vào vùng đệm (Buffer). Điều này giúp
 * tiết kiệm hàng triệu syscall xin cấp phát RAM, giảm tải tối đa sức ép dọn rác
 * (Garbage/Page Faults) và đẩy băng thông nạp dữ liệu lên ngưỡng vật lý của ổ
 * cứng.
 */
class DataLoader {
public:
  /**
   * @brief Kích hoạt tiến trình nạp (Load) dữ liệu trực tiếp từ file vật lý vào
   * In-Memory Store.
   *
   * Thuật toán: Dữ liệu được đọc theo từng chunk 256KB vào `readBuffer` nhằm
   * tối ưu độ trễ cho L1/L2 Cache. Tại mỗi chu kỳ phát hiện ngắt dòng `\n`, hệ
   * thống băm chuỗi nội suy và tra cứu tức thì thông qua `gatekeeper` để chặn
   * rác trùng lặp O(1). Nếu hợp lệ, hệ thống trích xuất Metadata và lập chỉ mục
   * qua `LogStore`. Cơ chế này vận hành tuyến tính liên tục với độ phức tạp
   * thời gian O(N) theo tổng số khối byte của tệp log.
   *
   * @param filename Đường dẫn tệp CSV lưu trữ log thô trên đĩa vật lý.
   * @param store Kho lưu trữ nội tại (In-Memory LogStore) chịu trách nhiệm nhận
   * và lập chỉ mục.
   * @param gatekeeper Cấu trúc bộ lọc vân tay dữ liệu (DuplicateHashSet) triệt
   * tiêu dòng trùng lặp.
   * @return true Tiến trình nạp (Ingestion) kết thúc toàn vẹn, hoàn thành xử lý
   * Data Pipeline.
   * @return false Quá trình từ chối I/O do file hỏng, không tồn tại, hoặc mất
   * quyền truy cập.
   */
  static bool load(const std::string &filename, LogStore &store,
                   DuplicateHashSet &gatekeeper);
};

#endif
