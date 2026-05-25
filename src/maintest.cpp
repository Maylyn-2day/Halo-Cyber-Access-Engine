#include <iostream>
#include <string>
#include <chrono>

#include "LogStore.h"
#include "DuplicateHashSet.h"
#include "DataLoader.h"
#include "SearchEngine.h"

int main() {
    std::cout << "==================================================\n";
    std::cout << "    HALO ENGINE - BAO CAO GIUA KY (PHASE 1)       \n";
    std::cout << "==================================================\n\n";

    // ---------------------------------------------------------
    // BƯỚC 1 & 2: NẠP DỮ LIỆU & XÂY DỰNG MỤC LỤC
    // ---------------------------------------------------------
    std::string testFile = "halo_dataset_1000_events_locations.csv"; // Sửa lại đúng tên file của bạn
    LogStore store(10); 
    DuplicateHashSet gatekeeper(5000); 

    std::cout << "[*] DANG KHOI DONG HE THONG...\n";
    DataLoader::load(testFile, store, gatekeeper);
    
    SearchEngine engine(5000, 5000); 
    engine.buildIndices(store);

    std::cout << "[V] Da nap va danh chi muc thanh cong " << store.size() << " dong log.\n";
    std::cout << "[V] So luong chuoi ky tu (StringPool) da nen: " << store.stringPool.size() << " tu.\n\n";

    // Khai báo một khoảng thời gian giả định cho Yêu cầu 5
    // (Trong thực tế, bạn có thể cho người dùng nhập từ bàn phím)
    int64_t startTime = 0;           // Từ đầu kỷ nguyên máy tính
    int64_t endTime   = 2000000000;  // Đến khoảng năm 2033

    // ---------------------------------------------------------
    // YÊU CẦU 5.1: HÀNH TRÌNH CỦA 1 USER (Device -> App -> Resource)
    // ---------------------------------------------------------
    std::string targetUser = "U007"; // Thay bằng User có thật trong CSV
    uint32_t userId = store.stringPool.getOrCreateId(targetUser);

    std::cout << "--------------------------------------------------\n";
    std::cout << "[YEU CAU 5.1] Hanh trinh cua User: " << targetUser << "\n";
    std::cout << "Thoi gian: Tu " << startTime << " den " << endTime << "\n";
    
    const auto* userResults = engine.searchByUser(userId);
    if (userResults != nullptr && userResults->size() > 0) {
        int count = 0;
        for (uint32_t i = 0; i < userResults->size(); ++i) {
            const LogEntry* entry = (*userResults)[i];
            
            // Lọc theo khoảng thời gian cho trước
            if (entry->timestamp >= startTime && entry->timestamp <= endTime) {
                std::cout << "  - T: " << entry->timestamp << " | "
                          << store.stringPool.getString(entry->deviceId) << " -> "
                          << store.stringPool.getString(entry->appId) << " -> "
                          << store.stringPool.getString(entry->resourceId) << "\n";
                count++;
            }
        }
        std::cout << "-> Tong cong: " << count << " hanh dong.\n";
    } else {
        std::cout << "-> Khong co du lieu.\n";
    }
    std::cout << "\n";

    // ---------------------------------------------------------
    // YÊU CẦU 5.2: HÀNH TRÌNH CỦA 1 RESOURCE (User -> Device -> App)
    // ---------------------------------------------------------
    std::string targetResource = "R006"; // Thay bằng Resource có thật trong CSV
    uint32_t resId = store.stringPool.getOrCreateId(targetResource);

    std::cout << "--------------------------------------------------\n";
    std::cout << "[YEU CAU 5.2] Nguoi dung truy cap Resource: " << targetResource << "\n";
    std::cout << "Thoi gian: Tu " << startTime << " den " << endTime << "\n";

    const auto* resResults = engine.searchByResource(resId);
    if (resResults != nullptr && resResults->size() > 0) {
        int count = 0;
        for (uint32_t i = 0; i < resResults->size(); ++i) {
            const LogEntry* entry = (*resResults)[i];
            
            if (entry->timestamp >= startTime && entry->timestamp <= endTime) {
                std::cout << "  - T: " << entry->timestamp << " | "
                          << store.stringPool.getString(entry->userId) << " -> "
                          << store.stringPool.getString(entry->deviceId) << " -> "
                          << store.stringPool.getString(entry->appId) << "\n";
                count++;
            }
        }
        std::cout << "-> Tong cong: " << count << " hanh dong.\n";
    } else {
        std::cout << "-> Khong co du lieu.\n";
    }
    std::cout << "\n";

    // ---------------------------------------------------------
    // YÊU CẦU 5.3: TOP 10 TÀI NGUYÊN TRUY CẬP NHIỀU NHẤT
    // Dùng thuật toán mảng đếm tần suất (Bucket Counting) cực nhanh
    // ---------------------------------------------------------
    std::cout << "--------------------------------------------------\n";
    std::cout << "[YEU CAU 5.3] Top 10 Tai Nguyen (Tu " << startTime << " den " << endTime << ")\n";
    
    auto startTop10 = std::chrono::high_resolution_clock::now();

    // 1. Tạo mảng đếm tần suất (Kích thước = tổng số từ vựng trong từ điển)
    uint32_t dictionarySize = store.stringPool.size();
    uint32_t* counts = new uint32_t[dictionarySize];
    for (uint32_t i = 0; i < dictionarySize; ++i) counts[i] = 0;

    // 2. Đi qua nhà kho 1 vòng, thấy resource nào tăng biến đếm của resource đó
    for (uint32_t c = 0; c < store.chunkCount(); ++c) {
        const LogChunk* chunk = store.getChunk(c);
        if (!chunk) continue;
        const LogEntry* entries = chunk->raw();
        
        for (uint32_t i = 0; i < chunk->size(); ++i) {
            if (entries[i].timestamp >= startTime && entries[i].timestamp <= endTime) {
                counts[entries[i].resourceId]++;
            }
        }
    }

    // 3. Tìm 10 thằng có count cao nhất bằng Insertion Sort thu nhỏ
    struct TopRes { uint32_t id; uint32_t count; };
    TopRes top[10];
    for (int i = 0; i < 10; ++i) { top[i].id = 0; top[i].count = 0; }

    for (uint32_t i = 0; i < dictionarySize; ++i) {
        if (counts[i] > top[9].count) {
            top[9].id = i;
            top[9].count = counts[i];
            
            // Đẩy dần lên trên để giữ thứ tự giảm dần
            for (int j = 8; j >= 0; --j) {
                if (top[j+1].count > top[j].count) {
                    TopRes temp = top[j];
                    top[j] = top[j+1];
                    top[j+1] = temp;
                } else break;
            }
        }
    }

    // 4. In kết quả Top 10
    int rank = 1;
    for (int i = 0; i < 10; ++i) {
        if (top[i].count > 0) {
            std::cout << "  " << rank++ << ". " 
                      << store.stringPool.getString(top[i].id) 
                      << " (Truy cap: " << top[i].count << " lan)\n";
        }
    }

    delete[] counts; // Dọn rác bộ nhớ mảng đếm

    auto endTop10 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> top10Time = endTop10 - startTop10;
    std::cout << "-> (Thoi gian thong ke Top 10: " << top10Time.count() << " ms)\n";

    std::cout << "==================================================\n";
    std::cout << "           HOAN TACH XUAT SAC PHASE 1!            \n";
    std::cout << "==================================================\n";

    std::cin.get();
    return 0;
}