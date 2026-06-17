/**
 * @file main.cpp
 * @brief Entry point for the Halo Cyber Access Engine.
 *
 * This file orchestrates the high-performance in-memory log analytics pipeline,
 * managing the smart boot process (Binary Snapshot vs CSV), coordinating queries,
 * and presenting the interactive console interface.
 */
#define _CRT_SECURE_NO_WARNINGS
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <limits>
#include <string>

#include "ConsoleColor.h"
#include "anomaly/AnomalyDetector.h"
#include "core/DuplicateHashSet.h"
#include "indexing/SearchEngine.h"
#include "query/QueryEngine.h"
#include "storage/BinaryIO.h"
#include "storage/DataLoader.h"
#include "storage/LogStore.h"


namespace {
const std::string DEFAULT_CSV_PATH = "data.csv";

/**
 * @brief Auto-generates a .bin cache file path from the given CSV path.
 *
 * Ensures the binary cache file is consistently stored adjacent to the CSV file,
 * independently of the executable's current working directory.
 * Example: "data.csv" -> "data_cache.bin", "C:/foo/log.csv" -> "C:/foo/log_cache.bin"
 *
 * @param csvPath The absolute or relative path to the source CSV file.
 * @return The corresponding binary cache file path.
 */
std::string makeBinaryPath(const std::string &csvPath) {
  // Find last dot position
  size_t dotPos = csvPath.rfind('.');
  if (dotPos == std::string::npos) {
    return csvPath + "_cache.bin";
  }
  return csvPath.substr(0, dotPos) + "_cache.bin";
}

/**
 * @brief Generates the anomaly report CSV file path next to the source CSV.
 *
 * Example: "data.csv" -> "data_anomaly_report.csv"
 *
 * @param csvPath The absolute or relative path to the source CSV file.
 * @return The corresponding anomaly report file path.
 */
std::string makeReportPath(const std::string &csvPath) {
  size_t dotPos = csvPath.rfind('.');
  if (dotPos == std::string::npos) {
    return csvPath + "_anomaly_report.csv";
  }
  return csvPath.substr(0, dotPos) + "_anomaly_report.csv";
}
const int64_t DEFAULT_START = 0;
const int64_t DEFAULT_END = 2000000000;

/**
 * @brief Utility to print a horizontal divider for console formatting.
 */
void printDivider() {
  std::cout << ConsoleColor::CYAN
            << "============================================================"
            << ConsoleColor::RESET << '\n';
}

/**
 * @brief Clears invalid inputs from the standard input stream.
 *
 * Used to recover from bad cin states when the user enters invalid characters
 * where an integer was expected.
 */
void clearBadInput() {
  std::cin.clear();
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

/**
 * @brief Parses a date string (YYYY-MM-DD or YYYY-MM-DD HH:MM:SS) into an epoch timestamp (UTC).
 *
 * @param input The date string to parse.
 * @param result Reference to store the resulting epoch timestamp.
 * @return True if parsing was successful, false if the format was invalid.
 */
bool parseDateString(const std::string &input, int64_t &result) {
  struct tm timeStruct = {};
  int year = 0, month = 0, day = 0;
  int hour = 0, minute = 0, second = 0;

  // Try parsing YYYY-MM-DD HH:MM:SS first
  int matched = std::sscanf(input.c_str(), "%d-%d-%d %d:%d:%d", &year, &month,
                            &day, &hour, &minute, &second);

  if (matched < 3) {
    return false; // Not enough minimum YYYY-MM-DD
  }

  // Basic validity check
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

  // mktime handles local time, but we need UTC.
  // Use _mkgmtime on MSVC (Windows), timegm on Linux/Mac.
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
 * @brief Prompts and reads a timestamp from the user.
 *
 * Supports three formats:
 * 1. Empty (press Enter to accept the default value).
 * 2. Epoch integer (e.g., 1714236478).
 * 3. Date string (e.g., 2024-04-27 or 2024-04-27 10:34:38).
 *
 * @param prompt The prompt to display to the user.
 * @param value Reference to store the validated timestamp.
 * @param defaultValue The value to assign if the user simply presses Enter.
 * @param defaultDisplay String representation of the default value for the prompt.
 * @return True on valid input, false otherwise.
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

  // Try parsing as epoch number first
  bool isNumber = true;
  for (size_t i = 0; i < input.size(); ++i) {
    if (i == 0 && input[i] == '-')
      continue; // Allow negative numbers
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

  // Try parsing as date string
  if (parseDateString(input, value)) {
    return true;
  }

  std::cout << "  Invalid format. Use: YYYY-MM-DD, YYYY-MM-DD HH:MM:SS, or "
               "epoch number.\n";
  return false;
}

/**
 * @brief Interactively reads a time range (start and end timestamps) from the user.
 *
 * Automatically handles swapping values if the user inputs a start time that
 * is chronologically later than the end time.
 *
 * @param startTime Reference to store the start timestamp.
 * @param endTime Reference to store the end timestamp.
 */
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

/**
 * @brief Prints the interactive main menu to the console.
 */
void printMenu() {
  printDivider();
  std::cout << ConsoleColor::BCYAN << "Halo - Cyber Access Engine"
            << ConsoleColor::RESET << '\n';
  printDivider();
  std::cout << ConsoleColor::CYAN << "[1]" << ConsoleColor::RESET
            << " User Journey\n";
  std::cout << ConsoleColor::CYAN << "[2]" << ConsoleColor::RESET
            << " Resource History\n";
  std::cout << ConsoleColor::CYAN << "[3]" << ConsoleColor::RESET
            << " Top 10 Resources\n";
  std::cout << ConsoleColor::CYAN << "[4]" << ConsoleColor::RESET
            << " Anomaly Detection\n";
  std::cout << ConsoleColor::YELLOW << "[5]" << ConsoleColor::RESET
            << " Force Reload from CSV\n";
  std::cout << ConsoleColor::GRAY << "[0]" << ConsoleColor::RESET << " Exit\n";
  printDivider();
  std::cout << "Choice: ";
}

/**
 * @brief Calculates and prints the elapsed execution time for a query in microseconds.
 *
 * @param start The time point recorded before the query.
 * @param end The time point recorded after the query.
 */
void printQueryElapsedTime(
    const std::chrono::high_resolution_clock::time_point &start,
    const std::chrono::high_resolution_clock::time_point &end) {
  auto elapsedUs =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();

  std::cout << ConsoleColor::YELLOW << "Query execution time: " << elapsedUs
            << " us" << ConsoleColor::RESET << '\n';
}
} // namespace

/**
 * @brief The main execution loop of the Halo Engine.
 *
 * Handles the boot sequence (Binary vs CSV), initializes data structures,
 * builds indices, and serves the main interactive query loop.
 *
 * @return System exit code (0 for success).
 */
int main() {
  ConsoleColor::init();
  printDivider();
  std::cout << ConsoleColor::BCYAN << "Halo - Cyber Access Engine\n"
            << ConsoleColor::RESET;
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

  bool loaded = false;
  bool fromBinary = false;

  // Binary path generated dynamically next to CSV file
  const std::string BINARY_PATH = makeBinaryPath(filename);

  // --- Smart Boot strategy: Binary first, CSV fallback ---
  if (BinaryIO::isValid(BINARY_PATH.c_str(), filename)) {
    std::cout << ConsoleColor::YELLOW
              << "[*] Found valid binary snapshot: " << BINARY_PATH
              << ConsoleColor::RESET << '\n';
    std::cout << "Loading from binary...\n";

    auto loadStart = std::chrono::high_resolution_clock::now();
    loaded = BinaryIO::load(BINARY_PATH.c_str(), store);
    auto loadEnd = std::chrono::high_resolution_clock::now();

    auto loadMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                      loadEnd - loadStart)
                      .count();

    if (loaded) {
      fromBinary = true;
      std::cout << ConsoleColor::BGREEN << "[INSTANT BOOT]"
                << ConsoleColor::RESET << " Rows loaded: " << store.size()
                << '\n';
      std::cout << ConsoleColor::YELLOW << "Binary load time: " << loadMs
                << " ms" << ConsoleColor::RESET << '\n';
    } else {
      std::cout << ConsoleColor::BRED << "[!] Binary file corrupted."
                << ConsoleColor::RESET << " Falling back to CSV...\n";
      store.reset();
    }
  }

  if (!loaded) {
    // Fallback: Parse CSV
    DuplicateHashSet gatekeeper(2000003);

    std::cout << "\nLoading data from CSV: " << filename << '\n';

    auto loadStart = std::chrono::high_resolution_clock::now();
    loaded = DataLoader::load(filename, store, gatekeeper);
    auto loadEnd = std::chrono::high_resolution_clock::now();

    auto loadMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                      loadEnd - loadStart)
                      .count();

    if (!loaded) {
      std::cout << ConsoleColor::BRED << "Failed to load CSV file."
                << ConsoleColor::RESET << '\n';
      return 1;
    }

    std::cout << ConsoleColor::BYELLOW << "[FIRST RUN]" << ConsoleColor::RESET
              << " Rows loaded: " << store.size() << '\n';
    std::cout << ConsoleColor::YELLOW << "CSV ingestion time: " << loadMs
              << " ms" << ConsoleColor::RESET << '\n';

    // Dump binary snapshot for next run
    if (BinaryIO::dump(BINARY_PATH.c_str(), store, filename)) {
      std::cout << ConsoleColor::GREEN
                << "[+] Binary snapshot saved for next boot: " << BINARY_PATH
                << ConsoleColor::RESET << '\n';
    }
  }

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
      std::cout << ConsoleColor::BCYAN << "Running Anomaly Detection Engine..."
                << ConsoleColor::RESET << '\n';

      auto queryStart = std::chrono::high_resolution_clock::now();

      AnomalyDetector detector(store.stringPool.size());
      detector.runAll(engine, store.stringPool);

      auto queryEnd = std::chrono::high_resolution_clock::now();
      auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           queryEnd - queryStart)
                           .count();

      detector.printReport(store.stringPool);

      const std::string reportPath = makeReportPath(filename);
      if (detector.exportToCSV(reportPath.c_str(), store.stringPool)) {
        std::cout << ConsoleColor::GREEN
                  << "\n[+] Detailed anomaly report exported to: "
                  << reportPath
                  << ConsoleColor::RESET << '\n';
      } else {
        std::cout << ConsoleColor::BRED
                  << "\n[!] Failed to export anomaly report."
                  << ConsoleColor::RESET << '\n';
      }

      std::cout << ConsoleColor::YELLOW
                << "[*] Anomaly scanning time: " << elapsedMs << " ms"
                << ConsoleColor::RESET << '\n';

      continue;
    }

    if (choice == 5) {
      printDivider();
      std::cout << "This will discard the binary cache and re-parse CSV.\n";
      std::cout << "Are you sure? [y/N]: ";

      std::string confirm;
      std::getline(std::cin, confirm);

      if (confirm.empty() || (confirm[0] != 'y' && confirm[0] != 'Y')) {
        std::cout << "Cancelled.\n";
        continue;
      }

      std::cout << "Force reloading from CSV: " << filename << "\n";

      store.reset();

      DuplicateHashSet gatekeeper(2000003);

      auto loadStart = std::chrono::high_resolution_clock::now();
      bool reloaded = DataLoader::load(filename, store, gatekeeper);
      auto loadEnd = std::chrono::high_resolution_clock::now();

      auto loadMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        loadEnd - loadStart)
                        .count();

      if (!reloaded) {
        std::cout << ConsoleColor::BRED << "[!] Failed to reload CSV."
                  << ConsoleColor::RESET << '\n';
        continue;
      }

      std::cout << ConsoleColor::BGREEN << "[OK]" << ConsoleColor::RESET
                << " Rows loaded: " << store.size() << '\n';
      std::cout << ConsoleColor::YELLOW << "CSV ingestion time: " << loadMs
                << " ms" << ConsoleColor::RESET << '\n';

      // Rebuild indices
      engine.reset();

      auto idxStart = std::chrono::high_resolution_clock::now();
      engine.buildIndices(store);
      auto idxEnd = std::chrono::high_resolution_clock::now();

      auto idxMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                       idxEnd - idxStart)
                       .count();

      std::cout << "Indexing time: " << idxMs << " ms\n";

      // Update defaults
      if (store.size() > 0 && store.chunkCount() > 0) {
        const LogChunk *fc = store.getChunk(0);
        if (fc != nullptr && fc->size() > 0) {
          const LogEntry *fe = fc->raw();
          if (fe != nullptr) {
            defaultUser = store.stringPool.getString(fe[0].userId);
            defaultRes = store.stringPool.getString(fe[0].resourceId);
          }
        }
      }

      // Save new binary snapshot
      if (BinaryIO::dump(BINARY_PATH.c_str(), store, filename)) {
        std::cout << ConsoleColor::GREEN
                  << "[+] Binary snapshot updated: " << BINARY_PATH
                  << ConsoleColor::RESET << '\n';
      }

      continue;
    }

    std::cout << ConsoleColor::BRED << "Unknown option." << ConsoleColor::RESET
              << " Please choose 0-5.\n";
  }

  return 0;
}