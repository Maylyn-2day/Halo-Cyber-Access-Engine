#include <iostream>
#include <fstream>
#include <string>
#include <chrono> // Thư viện đo thời gian chuẩn của C++

#include "LogStore.h"
#include "DuplicateHashSet.h"
#include "DataLoader.h"

// // Hàm phụ trợ: Tự động tạo một file CSV mẫu ngay tại thư mục code
// void createMockCSV(const std::string& filename) {
//     std::ofstream file(filename);
//     // Dòng 1: Header (Sẽ bị DataLoader vứt bỏ)
//     file << "user_id,device_id,app_id,resource_id,event_type,location,timestamp\n";
//     // Dòng 2: Hàng xịn
//     file << "user_1,device_1,app_1,res_1,LOGIN,VN,1716537600\n";
//     // Dòng 3: Hàng xịn
//     file << "user_2,device_2,app_2,res_2,DOWNLOAD,US,1716537605\n";
//     // Dòng 4: TRÙNG LẶP HOÀN TOÀN VỚI DÒNG 2 (Sẽ bị DuplicateHashSet đuổi về)
//     file << "user_1,device_1,app_1,res_1,LOGIN,VN,1716537600\n"; 
//     // Dòng 5: Hàng xịn
//     file << "user_3,device_3,app_1,res_1,LOGOUT,JP,1716537610\n";
//     file.close();
//     std::cout << "[+] Da tao xong file mau: " << filename << std::endl;
// }

int main() {
    std::cout << "=== KHOI DONG HALO ENGINE (DATA LOADER TEST) ===\n" << std::endl;

    std::string testFile = "halo_dataset_2000_events_locations.csv";
    // createMockCSV(testFile);

    // 1. Khởi tạo nhà kho và cổng an ninh
    // Khởi tạo trước 10 xe tải trong metadata, và 10,000 cái xô để lọc trùng
    LogStore store(10); 
    DuplicateHashSet gatekeeper(5000); 

    std::cout << "\n[*] Dang nap du lieu tu file: " << testFile << "..." << std::endl;

    // Bắt đầu bấm giờ
    auto start = std::chrono::high_resolution_clock::now();

    // Gọi hàm Parser siêu tốc
    bool success = DataLoader::load(testFile, store, gatekeeper);

    // Kết thúc bấm giờ
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    // In báo cáo
    if (success) {
        std::cout << "[V] NAP DU LIEU THANH CONG!" << std::endl;
        std::cout << "    - Thoi gian chay: " << duration.count() << " ms" << std::endl;
        std::cout << "    - Tong so dong da luu: " << store.size() << " dong" << std::endl;
    } else {
        std::cout << "[X] LỖI: Khong tim thay file hoac file bi loi!" << std::endl;
    }

    std::cout << "\n=== KET THUC TEST ===" << std::endl;
    std::cin.get();
    return 0;
}