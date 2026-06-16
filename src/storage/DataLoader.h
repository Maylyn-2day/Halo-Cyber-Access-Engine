// DataLoader.h
#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include <string>

#include "../core/DuplicateHashSet.h"
#include "LogStore.h"

/**
 * @brief Hệ thống Ingestion dữ liệu tốc độ siêu cao (High-speed CSV Ingestion).
 *
 * Quyết định kiến trúc: DataLoader được thiết kế theo nguyên lý
 * "True Zero-Copy Parsing" sử dụng Memory-Mapped I/O. Thay vì đọc
 * từng khối byte qua fread rồi copy vào lineBuffer, lớp này map toàn bộ
 * file CSV thẳng vào virtual address space thông qua CreateFileMapping
 * (Windows) hoặc mmap (Linux). Các con trỏ Raw Character (FieldView)
 * trỏ trực tiếp vào vùng nhớ đã map — không tồn tại bất kỳ bản sao
 * trung gian nào. Điều này loại bỏ hoàn toàn chi phí syscall fread,
 * quản lý buffer, và copy byte-by-byte, đẩy băng thông nạp dữ liệu
 * lên ngưỡng vật lý của ổ cứng.
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
