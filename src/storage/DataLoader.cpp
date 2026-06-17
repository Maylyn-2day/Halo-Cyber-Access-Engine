/**
 * @file DataLoader.cpp
 * @brief Implementation of the high-performance DataLoader engine.
 *
 * Utilizes memory-mapped I/O (mmap on POSIX, CreateFileMapping on Windows)
 * combined with zero-copy string parsing (FieldView) to ingest millions of
 * log entries directly into the in-memory LogStore bypassing STL overhead.
 */
#define _CRT_SECURE_NO_WARNINGS
#include "DataLoader.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>

// Platform-specific headers for Memory-Mapped I/O
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
const uint32_t READ_BUFFER_SIZE = 256 * 1024;
const uint32_t MAX_LINE_SIZE = 4096;

/**
 * @struct FieldView
 * @brief Lightweight, non-owning string view for zero-copy parsing.
 */
struct FieldView {
  const char *start; ///< Pointer to the start of the field in mapped memory.
  uint32_t length;   ///< Length of the field in bytes.

  /**
   * @brief Default constructor. Initializes an empty view.
   */
  FieldView() : start(nullptr), length(0) {}

  /**
   * @brief Constructs a FieldView with a given start pointer and length.
   * @param fieldStart Pointer to the start of the field.
   * @param fieldLength Length of the field.
   */
  FieldView(const char *fieldStart, uint32_t fieldLength)
      : start(fieldStart), length(fieldLength) {}
};

/**
 * @struct ParsedLine
 * @brief Temporary structure holding parsed, but not yet pooled, fields of a single log line.
 */
struct ParsedLine {
  FieldView userId;
  FieldView deviceId;
  FieldView appId;
  FieldView resourceId;
  EventType eventType;
  Location location;
  int64_t timestamp;

  ParsedLine()
      : eventType(EVENT_INVALID), location(LOC_INVALID), timestamp(0) {}
};

/**
 * @brief Fast string equality check against a C-style string literal.
 * @param field The FieldView to compare.
 * @param literal The null-terminated C-string literal.
 * @return True if the field contents exactly match the literal.
 */
bool equalsLiteral(const FieldView &field, const char *literal) {
  uint32_t literalLength = static_cast<uint32_t>(std::strlen(literal));

  if (field.length != literalLength) {
    return false;
  }

  return std::memcmp(field.start, literal, literalLength) == 0;
}

/**
 * @brief Parses an event type string into an EventType enum.
 * @param field The FieldView representing the event type.
 * @return The corresponding EventType, or EVENT_INVALID if unknown.
 */
EventType parseEventType(const FieldView &field) {
  if (equalsLiteral(field, "LOGIN")) {
    return EVENT_LOGIN;
  }

  if (equalsLiteral(field, "LOGOUT")) {
    return EVENT_LOGOUT;
  }

  if (equalsLiteral(field, "TOKEN_REFRESH")) {
    return EVENT_TOKEN_REFRESH;
  }

  if (equalsLiteral(field, "ACCESS")) {
    return EVENT_ACCESS;
  }

  if (equalsLiteral(field, "FAILED_LOGIN")) {
    return EVENT_FAILED_LOGIN;
  }

  if (equalsLiteral(field, "OPEN_APP")) {
    return EVENT_OPEN_APP;
  }

  if (equalsLiteral(field, "DOWNLOAD")) {
    return EVENT_DOWNLOAD;
  }

  if (equalsLiteral(field, "ADMIN_ACTION")) {
    return EVENT_ADMIN_ACTION;
  }

  return EVENT_INVALID;
}

/**
 * @brief Parses a location string into a Location enum.
 * @param field The FieldView representing the location.
 * @return The corresponding Location, or LOC_INVALID if unknown.
 */
Location parseLocation(const FieldView &field) {
  if (equalsLiteral(field, "US")) {
    return LOC_US;
  }

  if (equalsLiteral(field, "VN")) {
    return LOC_VN;
  }

  if (equalsLiteral(field, "JP")) {
    return LOC_JP;
  }

  if (equalsLiteral(field, "KR")) {
    return LOC_KR;
  }

  if (equalsLiteral(field, "SG")) {
    return LOC_SG;
  }

  if (equalsLiteral(field, "CN")) {
    return LOC_CN;
  }

  if (equalsLiteral(field, "DE")) {
    return LOC_DE;
  }

  if (equalsLiteral(field, "FR")) {
    return LOC_FR;
  }

  if (equalsLiteral(field, "UK")) {
    return LOC_UK;
  }

  if (equalsLiteral(field, "AU")) {
    return LOC_AU;
  }

  if (equalsLiteral(field, "CA")) {
    return LOC_CA;
  }

  if (equalsLiteral(field, "IN")) {
    return LOC_IN;
  }

  if (equalsLiteral(field, "BR")) {
    return LOC_BR;
  }

  if (equalsLiteral(field, "RU")) {
    return LOC_RU;
  }

  if (equalsLiteral(field, "TH")) {
    return LOC_TH;
  }

  return LOC_INVALID;
}

/**
 * @brief High-performance integer parsing from a string view.
 * @param field The FieldView representing the integer.
 * @param value Reference to store the parsed integer.
 * @return True on success, false if parsing fails (e.g., non-numeric, overflow).
 */
bool parseInt64(const FieldView &field, int64_t &value) {
  if (field.length == 0) {
    return false;
  }

  if (field.start[0] == '-') {
    return false;
  }

  int64_t result = 0;

  for (uint32_t index = 0; index < field.length; ++index) {
    char c = field.start[index];

    if (c < '0' || c > '9') {
      return false;
    }

    int digit = c - '0';

    if (result > (INT64_MAX - digit) / 10) {
      return false;
    }

    result = result * 10 + digit;
  }

  value = result;
  return true;
}

/**
 * @brief Computes a DJB2 hash of a raw byte buffer.
 * @param data Pointer to the buffer.
 * @param length Length of the buffer.
 * @return The 64-bit DJB2 hash value.
 */
unsigned long long djb2Raw(const char *data, uint32_t length) {
  unsigned long long hash = 5381ULL;

  for (uint32_t i = 0; i < length; ++i) {
    hash = ((hash << 5) + hash) + static_cast<unsigned char>(data[i]);
  }

  return hash;
}

/**
 * @brief Converts a FieldView to a std::string (primarily for debugging).
 * @param field The FieldView to convert.
 * @return A std::string containing a copy of the field's data.
 */
std::string fieldToString(const FieldView &field) {
  return std::string(field.start, field.length);
}

/**
 * @brief Removes trailing carriage return ('\r') from a field if present.
 * @param field The FieldView to trim in-place.
 */
void trimRightCarriageReturn(FieldView &field) {
  if (field.length > 0 && field.start[field.length - 1] == '\r') {
    --field.length;
  }
}

/**
 * @brief Splits a comma-separated CSV line into 7 specific log fields and validates them.
 * @param line Pointer to the start of the line.
 * @param length Length of the line.
 * @param parsed Reference to a ParsedLine struct to store the parsed views.
 * @return True if the line was successfully split and validated, false otherwise.
 */
bool splitAndValidateLine(const char *line, uint32_t length,
                          ParsedLine &parsed) {
  FieldView fields[7];
  uint32_t fieldIndex = 0;
  const char *fieldStart = line;

  for (uint32_t i = 0; i <= length; ++i) {
    if (i == length || line[i] == ',') {
      if (fieldIndex >= 7) {
        return false;
      }

      fields[fieldIndex] =
          FieldView(fieldStart, static_cast<uint32_t>(&line[i] - fieldStart));
      ++fieldIndex;
      fieldStart = &line[i + 1];
    }
  }

  if (fieldIndex != 7) {
    return false;
  }

  trimRightCarriageReturn(fields[6]);

  for (uint32_t i = 0; i < 7; ++i) {
    if (fields[i].length == 0) {
      return false;
    }
  }

  parsed.userId = fields[0];
  parsed.deviceId = fields[1];
  parsed.appId = fields[2];
  parsed.resourceId = fields[3];
  parsed.eventType = parseEventType(fields[4]);
  parsed.location = parseLocation(fields[5]);

  if (parsed.eventType == EVENT_INVALID || parsed.location == LOC_INVALID) {
    return false;
  }

  return parseInt64(fields[6], parsed.timestamp);
}

/**
 * @brief Checks if a line is the expected CSV header.
 * @param line Pointer to the start of the line.
 * @param length Length of the line.
 * @return True if it perfectly matches the known header format.
 */
bool looksLikeHeader(const char *line, uint32_t length) {
  const char expected[] =
      "user_id,device_id,app_id,resource_id,event_type,location,timestamp";
  const uint32_t expectedLength = sizeof(expected) - 1;

  if (length == expectedLength) {
    return std::memcmp(line, expected, expectedLength) == 0;
  }

  if (length == expectedLength + 1 && line[length - 1] == '\r') {
    return std::memcmp(line, expected, expectedLength) == 0;
  }

  return false;
}

/**
 * @brief Processes a single line: deduplicates, parses, and inserts into the LogStore.
 * @param line Pointer to the start of the line.
 * @param length Length of the line.
 * @param firstLine Flag indicating if this is the first line (header check).
 * @param store The LogStore to insert the parsed LogEntry into.
 * @param gatekeeper Deduplication set to filter out identical log lines.
 * @return True on success (or skipped duplicate/header), false on critical failure.
 */
bool processLine(const char *line, uint32_t length, bool &firstLine,
                 LogStore &store, DuplicateHashSet &gatekeeper) {
  if (length == 0) {
    return true;
  }

  if (firstLine) {
    firstLine = false;

    if (looksLikeHeader(line, length)) {
      return true;
    }
  }

  unsigned long long fingerprint = djb2Raw(line, length);

  if (!gatekeeper.insertIfAbsent(fingerprint)) {
    return true;
  }

  ParsedLine parsed;

  if (!splitAndValidateLine(line, length, parsed)) {
    return true;
  }

  LogEntry entry(
      store.stringPool.getOrCreateId(parsed.userId.start, parsed.userId.length),
      store.stringPool.getOrCreateId(parsed.deviceId.start,
                                     parsed.deviceId.length),
      store.stringPool.getOrCreateId(parsed.appId.start, parsed.appId.length),
      store.stringPool.getOrCreateId(parsed.resourceId.start,
                                     parsed.resourceId.length),
      parsed.eventType, parsed.location, parsed.timestamp);

  return store.insert(entry) != nullptr;
}
} // namespace

/**
 * @brief Loads log data from a CSV file into the provided LogStore.
 *
 * Uses memory-mapped I/O for direct kernel-to-user-space reading, 
 * bypassing std::ifstream overhead. Employs zero-copy parsing to slice 
 * the file directly in memory.
 *
 * @param filename Path to the CSV log file.
 * @param store Reference to the core LogStore to populate.
 * @param gatekeeper DuplicateHashSet to reject duplicate entries at parse-time.
 * @return True if the file was completely and successfully processed.
 */
bool DataLoader::load(const std::string &filename, LogStore &store,
                      DuplicateHashSet &gatekeeper) {

  // ================================================================
  // Phase 1: Memory-Map entire CSV file into virtual address space
  // ================================================================
  const char *mappedData = nullptr;
  size_t fileSize = 0;

#if defined(_WIN32)
  // --- Windows: CreateFileMapping + MapViewOfFile ---
  HANDLE hFile = CreateFileA(
      filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

  if (hFile == INVALID_HANDLE_VALUE) {
    std::cerr << "[DataLoader] Cannot open file '" << filename << "'\n";
    return false;
  }

  LARGE_INTEGER liSize;
  if (!GetFileSizeEx(hFile, &liSize)) {
    std::cerr << "[DataLoader] Cannot get file size for '" << filename << "'\n";
    CloseHandle(hFile);
    return false;
  }
  fileSize = static_cast<size_t>(liSize.QuadPart);

  if (fileSize == 0) {
    CloseHandle(hFile);
    return true; // Empty file - valid but no data
  }

  HANDLE hMapping =
      CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);

  if (hMapping == nullptr) {
    std::cerr << "[DataLoader] CreateFileMapping failed for '" << filename << "'\n";
    CloseHandle(hFile);
    return false;
  }

  mappedData = static_cast<const char *>(
      MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0));

  if (mappedData == nullptr) {
    std::cerr << "[DataLoader] MapViewOfFile failed for '" << filename << "'\n";
    CloseHandle(hMapping);
    CloseHandle(hFile);
    return false;
  }

#else
  // --- Linux/macOS: mmap ---
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd == -1) {
    std::cerr << "[DataLoader] Cannot open file '" << filename << "'\n";
    return false;
  }

  struct stat st;
  if (fstat(fd, &st) == -1) {
    std::cerr << "[DataLoader] Cannot stat file '" << filename << "'\n";
    close(fd);
    return false;
  }
  fileSize = static_cast<size_t>(st.st_size);

  if (fileSize == 0) {
    close(fd);
    return true;
  }

  mappedData = static_cast<const char *>(
      mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0));

  if (mappedData == MAP_FAILED) {
    std::cerr << "[DataLoader] mmap failed for '" << filename << "'\n";
    close(fd);
    return false;
  }

  // Suggest kernel to read sequentially (prefetch optimization)
  madvise(const_cast<char *>(mappedData), fileSize, MADV_SEQUENTIAL);
#endif

  // ================================================================
  // Phase 2: O(N) linear scan on mapped memory
  // FieldView points directly to mapped memory - True Zero-Copy Parsing
  // ================================================================
  bool firstLine = true;
  bool success = true;
  const char *lineStart = mappedData;

  for (size_t i = 0; i < fileSize && success; ++i) {
    if (mappedData[i] == '\n') {
      uint32_t lineLength = static_cast<uint32_t>(&mappedData[i] - lineStart);

      success =
          processLine(lineStart, lineLength, firstLine, store, gatekeeper);

      lineStart = &mappedData[i + 1];
    }
  }

  // Process last line if file doesn't end with '\n'
  if (success && lineStart < mappedData + fileSize) {
    uint32_t lineLength =
        static_cast<uint32_t>((mappedData + fileSize) - lineStart);
    if (lineLength > 0) {
      success =
          processLine(lineStart, lineLength, firstLine, store, gatekeeper);
    }
  }

  // ================================================================
  // Phase 3: Free OS resources
  // ================================================================
#if defined(_WIN32)
  UnmapViewOfFile(mappedData);
  CloseHandle(hMapping);
  CloseHandle(hFile);
#else
  munmap(const_cast<char *>(mappedData), fileSize);
  close(fd);
#endif

  return success;
}