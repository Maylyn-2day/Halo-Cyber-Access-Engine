#include <iostream>
#include <string>
#include "LogEntry.h"
#include "LogChunk.h"
#include "DuplicateHashSet.h"

int main() {
    std::cout << "=== KHOI DONG HE THONG HALO ENGINE ===" << std::endl;

    // ---------------------------------------------------------
    // TEST 1: KIỂM TRA BỘ LỌC TRÙNG LẶP (DuplicateHashSet)
    // ---------------------------------------------------------
    std::cout << "\n[TEST 1] Kiem tra bo loc trung lap (Gatekeeper)..." << std::endl;
    
    // Khởi tạo bộ lọc với 5 cái xô (bucket) để test
    DuplicateHashSet hashSet(5); 

    // Giả lập 3 dòng đọc từ file CSV
    std::string line1 = "2026-05-24,user_1,login,app_1";
    std::string line2 = "2026-05-24,user_2,logout,app_2";
    std::string line3 = "2026-05-24,user_1,login,app_1"; // Cố tình nạp lại line 1 (Trùng rác)

    // Đưa qua hàm băm djb2 để lấy vân tay (fingerprint)
    unsigned long long fp1 = DuplicateHashSet::djb2(line1);
    unsigned long long fp2 = DuplicateHashSet::djb2(line2);
    unsigned long long fp3 = DuplicateHashSet::djb2(line3);

    // Thử nạp vào hệ thống
    std::cout << "Nap dong 1: " << (hashSet.insertIfAbsent(fp1) ? "THANH CONG (Moi)" : "TU CHOI (Trung)") << std::endl;
    std::cout << "Nap dong 2: " << (hashSet.insertIfAbsent(fp2) ? "THANH CONG (Moi)" : "TU CHOI (Trung)") << std::endl;
    std::cout << "Nap dong 3: " << (hashSet.insertIfAbsent(fp3) ? "THANH CONG (Moi)" : "TU CHOI (Trung)") << std::endl;


    // ---------------------------------------------------------
    // TEST 2: KIỂM TRA THÙNG CHỨA CỐ ĐỊNH (LogChunk)
    // ---------------------------------------------------------
    std::cout << "\n[TEST 2] Kiem tra thung chua co dinh (LogChunk)..." << std::endl;
    
    // Khởi tạo một thùng chỉ chứa được tối đa 2 dòng log
    LogChunk chunk(2); 

    // Tạo giả 3 cục dữ liệu sạch (sau khi đã bóc tách)
    LogEntry entry1; entry1.userId = 1;
    LogEntry entry2; entry2.userId = 2;
    LogEntry entry3; entry3.userId = 3;

    // Thử nhét vào thùng
    LogEntry* p1 = chunk.append(entry1);
    std::cout << "Nhet data 1: " << (p1 != nullptr ? "VAO THUNG" : "THUNG DAY!") << std::endl;

    LogEntry* p2 = chunk.append(entry2);
    std::cout << "Nhet data 2: " << (p2 != nullptr ? "VAO THUNG" : "THUNG DAY!") << std::endl;

    LogEntry* p3 = chunk.append(entry3); // Thùng chỉ chứa được 2, nhét cái thứ 3 sẽ bị chặn
    std::cout << "Nhet data 3: " << (p3 != nullptr ? "VAO THUNG" : "THUNG DAY! (Tra ve nullptr)") << std::endl;

    std::cout << "\n=== KET THUC TEST ===" << std::endl;
    
    // Giữ màn hình console không bị tắt chớp nhoáng
    std::cin.get(); 
    return 0;
}