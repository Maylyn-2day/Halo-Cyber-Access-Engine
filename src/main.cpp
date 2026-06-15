// main.cpp
#include <chrono>
#include <iostream>
#include <limits>
#include <string>


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

bool readInt64WithDefault(const std::string &prompt, int64_t &value,
                          int64_t defaultValue) {
  std::cout << prompt << " [Enter = " << defaultValue << "]: ";

  std::string input;
  std::getline(std::cin, input);

  if (input.empty()) {
    value = defaultValue;
    return true;
  }

  try {
    value = std::stoll(input);
    return true;
  } catch (...) {
    std::cout << "Invalid number. Please try again.\n";
    return false;
  }
}

void readTimeRange(int64_t &startTime, int64_t &endTime) {
  while (!readInt64WithDefault("Start timestamp", startTime, DEFAULT_START)) {
  }
  while (!readInt64WithDefault("End timestamp", endTime, DEFAULT_END)) {
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

    std::cout << "Unknown option. Please choose 1, 2, 3, or 0.\n";
  }

  return 0;
}