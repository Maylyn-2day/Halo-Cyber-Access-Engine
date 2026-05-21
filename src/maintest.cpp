#include <iostream>
#include "LogEntry.h"
#include "DynamicArray.h"

int main() {
    std::cout << "Halo Engine - Khoi tao bo xuong thanh cong!" << std::endl;
    
    // Test thử gọi mảng động
    DynamicArray<LogEntry*> testArray(10);
    std::cout << "Dung luong mang test: " << testArray.getCapacity() << std::endl;
    
    return 0;
}