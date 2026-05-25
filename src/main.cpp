#include <iostream>
#include <string>
#include <chrono>

#include "LogStore.h"
#include "DuplicateHashSet.h"
#include "DataLoader.h"
#include "SearchEngine.h"
#include "QueryEngine.h"

int main() {
    std::cout << "==================================================\n";
    std::cout << "    HALO ENGINE - GIAI DOAN 1 (MIDTERM)           \n";
    std::cout << "    Ho va ten: Le Nguyen Thuy Linh                \n";
    std::cout << "==================================================\n\n";

    // --- 1. KHỞI TẠO HỆ THỐNG ---
    std::string testFile = "halo_dataset_1m.csv"; 
    LogStore store(10); 
    DuplicateHashSet gatekeeper(5000); 

    // --- 2. NẠP DỮ LIỆU ---
    std::cout << "[*] Dang nap du lieu tu file: " << testFile << "...\n";
    auto startLoad = std::chrono::high_resolution_clock::now();
    
    if (!DataLoader::load(testFile, store, gatekeeper)) {
        std::cout << "[X] Loi: Khong doc duoc file!\n";
        return 1;
    }
    
    auto endLoad = std::chrono::high_resolution_clock::now();
    std::cout << "[V] Da nap " << store.size() << " dong (Thoi gian: " 
              << std::chrono::duration<double, std::milli>(endLoad - startLoad).count() << " ms)\n";

    // --- 3. XÂY DỰNG CHỈ MỤC & SẮP XẾP ---
    std::cout << "[*] Dang xay dung chi muc va sap xep thoi gian...\n";
    auto startBuild = std::chrono::high_resolution_clock::now();
    
    SearchEngine engine(5000, 5000); 
    engine.buildIndices(store);
    
    auto endBuild = std::chrono::high_resolution_clock::now();
    std::cout << "[V] Hoan tat chi muc (Thoi gian: " 
              << std::chrono::duration<double, std::milli>(endBuild - startBuild).count() << " ms)\n\n";

    // --- 4. TRUY VẤN NGHIỆP VỤ (YÊU CẦU 5) ---
    // Giả lập dữ liệu đầu vào (Bạn có thể đổi thành std::cin nếu thầy yêu cầu nhập tay)
    std::string targetUser = "U06619";
    std::string targetResource = "R01685";
    int64_t startTime = 0;
    int64_t endTime = 2000000000;

    uint32_t targetUserId = store.stringPool.getOrCreateId(targetUser);
    uint32_t targetResId = store.stringPool.getOrCreateId(targetResource);

    std::cout << "==================================================\n";
    std::cout << "[YEU CAU 5.1] Kiem tra hanh trinh cua User\n";
    std::cout << "==================================================\n";
    QueryEngine::printUserJourney(targetUserId, startTime, endTime, engine, store.stringPool);
    std::cout << "\n";

    std::cout << "==================================================\n";
    std::cout << "[YEU CAU 5.2] Kiem tra lich su truy cap Resource\n";
    std::cout << "==================================================\n";
    QueryEngine::printResourceJourney(targetResId, startTime, endTime, engine, store.stringPool);
    std::cout << "\n";

    std::cout << "==================================================\n";
    std::cout << "[YEU CAU 5.3] Top 10 Tai nguyen truy cap nhieu nhat\n";
    std::cout << "==================================================\n";
    QueryEngine::printTop10Resources(startTime, endTime, store);
    std::cout << "\n";

    std::cout << "==================================================\n";
    std::cout << "    HOAN TAT XUAT SAC KIEM TRA GIUA KY!           \n";
    std::cout << "==================================================\n";

    std::cin.get();
    return 0;
}