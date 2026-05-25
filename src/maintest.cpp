#include <iostream>
#include <string>
#include <chrono>

#include "LogStore.h"
#include "DuplicateHashSet.h"
#include "DataLoader.h"
#include "SearchEngine.h"

// Hàm phụ trợ: Nén chuỗi thành ID (Mô phỏng lại cơ chế của DataLoader)
uint32_t stringToCompactId(const std::string& str) {
    unsigned long long hash = 5381ULL;
    for (char c : str) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
    }
    return static_cast<uint32_t>(hash & 0xFFFFFFFFU);
}

int main() {
    std::cout << "==================================================\n";
    std::cout << "    HALO ENGINE - TONG DUYET HE THONG (PHASE 1)   \n";
    std::cout << "==================================================\n\n";

    // --- BƯỚC 1: KHỞI TẠO HỆ THỐNG ---
    std::string testFile = "halo_dataset_1000_events_locations.csv"; 
    LogStore store(10); 
    DuplicateHashSet gatekeeper(5000); 

    // --- BƯỚC 2: NẠP DỮ LIỆU (DATA LOADER) ---
    std::cout << "[1] DANG NAP DU LIEU TU FILE CSV...\n";
    auto startLoad = std::chrono::high_resolution_clock::now();
    
    bool loadSuccess = DataLoader::load(testFile, store, gatekeeper);
    
    auto endLoad = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> loadTime = endLoad - startLoad;

    if (!loadSuccess) {
        std::cout << "[X] Loi: Khong doc duoc file CSV!\n";
        return 1;
    }
    std::cout << "    -> Thanh cong! Da nap " << store.size() << " dong (Thoi gian: " << loadTime.count() << " ms)\n\n";

    // --- BƯỚC 3: XÂY DỰNG CHỈ MỤC (INDEXING) ---
    std::cout << "[2] DANG XAY DUNG TU MUC LUC (USER & RESOURCE)...\n";
    auto startBuild = std::chrono::high_resolution_clock::now();
    
    SearchEngine engine(5000, 5000); 
    engine.buildIndices(store);
    
    auto endBuild = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> buildTime = endBuild - startBuild;
    std::cout << "    -> Thanh cong! (Thoi gian: " << buildTime.count() << " ms)\n\n";

    // --- BƯỚC 4: TEST KHAI THÁC - TÌM HÀNH TRÌNH USER ---
    std::string targetUser = "U007"; 
    uint32_t targetUserId = stringToCompactId(targetUser);
    
    std::cout << "[3] TRUY VAN: Lich su hanh dong cua [" << targetUser << "]\n";
    const auto* userResults = engine.searchByUser(targetUserId);

    if (userResults != nullptr && userResults->size() > 0) {
        std::cout << "    -> Tim thay " << userResults->size() << " log.\n";
        // In 5 hành động đầu tiên (Device -> App -> Resource)
        uint32_t printCount = (userResults->size() > 5) ? 5 : userResults->size();
        for (uint32_t i = 0; i < printCount; ++i) {
            const LogEntry* entry = (*userResults)[i];
            std::cout << "       - Timestamp: " << entry->timestamp 
                      << " | Hanh trinh: Device(" << entry->deviceId << ") -> App(" << entry->appId << ") -> Resource(" << entry->resourceId << ")\n";
        }
    } else {
        std::cout << "    -> [X] Khong tim thay du lieu cua " << targetUser << "\n";
    }
    std::cout << "\n";

    // --- BƯỚC 5: TEST KHAI THÁC - TÌM HÀNH TRÌNH RESOURCE ---
    std::string targetResource = "R003"; // Bạn có thể đổi thành tên resource có thật trong CSV
    uint32_t targetResId = stringToCompactId(targetResource);

    std::cout << "[4] TRUY VAN: Ai da truy cap tai nguyen [" << targetResource << "]\n";
    const auto* resResults = engine.searchByResource(targetResId);

    if (resResults != nullptr && resResults->size() > 0) {
        std::cout << "    -> Tim thay " << resResults->size() << " log.\n";
        // In 5 hành động đầu tiên (User -> Device -> App)
        uint32_t printCount = (resResults->size() > 5) ? 5 : resResults->size();
        for (uint32_t i = 0; i < printCount; ++i) {
            const LogEntry* entry = (*resResults)[i];
            std::cout << "       - Timestamp: " << entry->timestamp 
                      << " | Hanh trinh: User(" << entry->userId << ") -> Device(" << entry->deviceId << ") -> App(" << entry->appId << ")\n";
        }
    } else {
        std::cout << "    -> [X] Khong tim thay du lieu cua " << targetResource << "\n";
    }

    std::cout << "\n==================================================\n";
    std::cout << "                 TEST HOAN TAT                    \n";
    std::cout << "==================================================\n";

    std::cin.get();
    return 0;
}