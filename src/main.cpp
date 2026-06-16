// main.cpp
#define _CRT_SECURE_NO_WARNINGS
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <limits>
#include <string>

#include "anomaly/AnomalyDetector.h"
#include "core/DuplicateHashSet.h"
#include "indexing/SearchEngine.h"
#include "query/QueryEngine.h"
#include "storage/DataLoader.h"
#include "storage/LogStore.h"

namespace {
const std::string DEFAULT_CSV_PATH = "data.csv";

const int64_t DEFAULT_START = 0;
const int64_t DEFAULT_END = 2000000000;

void printDivider() {
  std::cout << "============================================================\n";
}

void clearBadInput() {
  std::cin.clear();
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

/**
 * @brief Phân tích chuỗi ngày tháng (YYYY-MM-DD hoặc YYYY-MM-DD HH:MM:SS)
 *        thành epoch timestamp (UTC).
 * @return true nếu parse thành công, false nếu định dạng sai.
 */
bool parseDateString(const std::string &input, int64_t &result) {
  struct tm timeStruct = {};
  int year = 0, month = 0, day = 0;
  int hour = 0, minute = 0, second = 0;

  // Thử parse YYYY-MM-DD HH:MM:SS trước
  int matched = std::sscanf(input.c_str(), "%d-%d-%d %d:%d:%d", &year, &month,
                            &day, &hour, &minute, &second);

  if (matched < 3) {
    return false; // Không đủ tối thiểu YYYY-MM-DD
  }

  // Kiểm tra tính hợp lệ cơ bản
  if (year < 1970 || year > 2100 || month < 1 || month > 12 || day < 1 ||
      day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
      second < 0 || second > 59) {
    return false;
  }

  timeStruct.tm_year = year - 1900;
  timeStruct.tm_mon = month - 1;
  timeStruct.tm_mday = day;
  timeStruct.tm_hour = hour;
  timeStruct.tm_min = minute;
  timeStruct.tm_sec = second;
  timeStruct.tm_isdst = 0;

  // mktime xử lý local time, nhưng ta cần UTC.
  // Dùng _mkgmtime trên MSVC (Windows), timegm trên Linux/Mac.
#if defined(_WIN32)
  time_t epoch = _mkgmtime(&timeStruct);
#else
  time_t epoch = timegm(&timeStruct);
#endif

  if (epoch == -1) {
    return false;
  }

  result = static_cast<int64_t>(epoch);
  return true;
}

/**
 * @brief Đọc giá trị thời gian từ người dùng.
 * Chấp nhận 3 định dạng:
 *   1. Enter (trả về giá trị mặc định)
 *   2. Số nguyên (epoch timestamp, VD: 1714236478)
 *   3. Chuỗi ngày tháng (VD: 2024-04-27 hoặc 2024-04-27 10:34:38)
 */
bool readTimestamp(const std::string &prompt, int64_t &value,
                   int64_t defaultValue, const char *defaultDisplay) {
  std::cout << prompt << " [Enter = " << defaultDisplay << "]: ";

  std::string input;
  std::getline(std::cin, input);

  if (input.empty()) {
    value = defaultValue;
    return true;
  }

  // Thử parse như epoch number trước
  bool isNumber = true;
  for (size_t i = 0; i < input.size(); ++i) {
    if (i == 0 && input[i] == '-')
      continue; // Cho phép số âm
    if (input[i] < '0' || input[i] > '9') {
      isNumber = false;
      break;
    }
  }

  if (isNumber) {
    try {
      value = std::stoll(input);
      return true;
    } catch (...) {
      // Fall through to date parsing
    }
  }

  // Thử parse như chuỗi ngày tháng
  if (parseDateString(input, value)) {
    return true;
  }

  std::cout << "  Invalid format. Use: YYYY-MM-DD, YYYY-MM-DD HH:MM:SS, or "
               "epoch number.\n";
  return false;
}

void readTimeRange(int64_t &startTime, int64_t &endTime) {
  std::cout << "\n  [Time Input Formats Supported]\n"
            << "  1. Epoch timestamp (e.g., 1714236478)\n"
            << "  2. Date string (e.g., 2024-04-27 or 2024-04-27 10:34:38)\n"
            << "  3. Empty (press Enter to select ALL data)\n";

  while (!readTimestamp("Start time", startTime, DEFAULT_START, "ALL")) {
  }
  while (!readTimestamp("End time", endTime, DEFAULT_END, "ALL")) {
  }

  if (startTime > endTime) {
    std::cout
        << "Start timestamp is greater than end timestamp. Swapping values.\n";

    int64_t temp = startTime;
    startTime = endTime;
    endTime = temp;
  }
}

void printMenu() {
  printDivider();
  std::cout << "Halo - Cyber Access Engine\n";
  printDivider();
  std::cout << "[1] User Journey\n";
  std::cout << "[2] Resource History\n";
  std::cout << "[3] Top 10 Resources\n";
  std::cout << "[4] Anomaly Detection\n";
  std::cout << "[0] Exit\n";
  printDivider();
  std::cout << "Choice: ";
}

void printQueryElapsedTime(
    const std::chrono::high_resolution_clock::time_point &start,
    const std::chrono::high_resolution_clock::time_point &end) {
  auto elapsedUs =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();

  std::cout << "Query execution time: " << elapsedUs << " us\n";
}
} // namespace

int main() {
  printDivider();
  std::cout << "Halo - Cyber Access Engine\n";
  std::cout << "High-Performance In-Memory Log Analytics\n";
  printDivider();

  std::cout << "CSV file path [Enter = " << DEFAULT_CSV_PATH << "]: ";

  std::string filename;
  std::getline(std::cin, filename);

  if (filename.empty()) {
    filename = DEFAULT_CSV_PATH;
  }

  LogStore store(16384);
  store.stringPool.reserve(262147);

  DuplicateHashSet gatekeeper(2000003);

  std::cout << "\nLoading data from: " << filename << '\n';

  auto loadStart = std::chrono::high_resolution_clock::now();
  bool loaded = DataLoader::load(filename, store, gatekeeper);
  auto loadEnd = std::chrono::high_resolution_clock::now();

  auto loadMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(loadEnd - loadStart)
          .count();

  if (!loaded) {
    std::cout << "Failed to load CSV file.\n";
    return 1;
  }

  std::cout << "Rows loaded: " << store.size() << '\n';
  std::cout << "Ingestion time: " << loadMs << " ms\n";

  std::string defaultUser = "";
  std::string defaultRes = "";

  if (store.size() > 0 && store.chunkCount() > 0) {
    const LogChunk *firstChunk = store.getChunk(0);

    if (firstChunk != nullptr && firstChunk->size() > 0) {
      const LogEntry *firstEntry = firstChunk->raw();

      if (firstEntry != nullptr) {
        defaultUser = store.stringPool.getString(firstEntry[0].userId);
        defaultRes = store.stringPool.getString(firstEntry[0].resourceId);
      }
    }
  }

  std::cout << "\nBuilding search indices...\n";

  SearchEngine engine(262147, 262147);

  auto indexStart = std::chrono::high_resolution_clock::now();
  engine.buildIndices(store);
  auto indexEnd = std::chrono::high_resolution_clock::now();

  auto indexMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                     indexEnd - indexStart)
                     .count();

  std::cout << "Indexing time: " << indexMs << " ms\n";

  while (true) {
    int choice = -1;

    std::cout << '\n';
    printMenu();

    if (!(std::cin >> choice)) {
      clearBadInput();
      std::cout << "Invalid choice. Please enter a number.\n";
      continue;
    }

    clearBadInput();

    if (choice == 0) {
      std::cout << "Exiting Halo Engine.\n";
      break;
    }

    if (choice == 1) {
      std::string userText;
      int64_t startTime = 0;
      int64_t endTime = 0;

      std::cout << "User ID [Enter = " << defaultUser << "]: ";
      std::getline(std::cin, userText);

      if (userText.empty()) {
        userText = defaultUser;
      }

      uint32_t userId = store.stringPool.getOrCreateId(userText);
      readTimeRange(startTime, endTime);

      printDivider();

      auto queryStart = std::chrono::high_resolution_clock::now();
      QueryEngine::printUserJourney(userId, startTime, endTime, engine,
                                    store.stringPool);
      auto queryEnd = std::chrono::high_resolution_clock::now();

      printQueryElapsedTime(queryStart, queryEnd);
      continue;
    }

    if (choice == 2) {
      std::string resourceText;
      int64_t startTime = 0;
      int64_t endTime = 0;

      std::cout << "Resource ID [Enter = " << defaultRes << "]: ";
      std::getline(std::cin, resourceText);

      if (resourceText.empty()) {
        resourceText = defaultRes;
      }

      uint32_t resourceId = store.stringPool.getOrCreateId(resourceText);
      readTimeRange(startTime, endTime);

      printDivider();

      auto queryStart = std::chrono::high_resolution_clock::now();
      QueryEngine::printResourceJourney(resourceId, startTime, endTime, engine,
                                        store.stringPool);
      auto queryEnd = std::chrono::high_resolution_clock::now();

      printQueryElapsedTime(queryStart, queryEnd);
      continue;
    }

    if (choice == 3) {
      int64_t startTime = 0;
      int64_t endTime = 0;

      readTimeRange(startTime, endTime);

      printDivider();

      auto queryStart = std::chrono::high_resolution_clock::now();
      QueryEngine::printTop10Resources(startTime, endTime, store);
      auto queryEnd = std::chrono::high_resolution_clock::now();

      printQueryElapsedTime(queryStart, queryEnd);
      continue;
    }

    if (choice == 4) {
      printDivider();
      std::cout << "Running Anomaly Detection Engine...\n";

      auto queryStart = std::chrono::high_resolution_clock::now();

      AnomalyDetector detector(store.stringPool.size());
      detector.runAll(engine, store.stringPool);

      auto queryEnd = std::chrono::high_resolution_clock::now();
      auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           queryEnd - queryStart)
                           .count();

      detector.printReport(store.stringPool);

      if (detector.exportToCSV("anomaly_report.csv", store.stringPool)) {
        std::cout << "\n[+] Detailed anomaly report exported to: "
                     "anomaly_report.csv\n";
      } else {
        std::cout << "\n[!] Failed to export anomaly report.\n";
      }

      std::cout << "[*] Anomaly scanning time: " << elapsedMs << " ms\n";

      continue;
    }

    std::cout << "Unknown option. Please choose 1, 2, 3, or 0.\n";
  }

  return 0;
}