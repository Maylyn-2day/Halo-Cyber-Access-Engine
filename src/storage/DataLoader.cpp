// DataLoader.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "DataLoader.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

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

struct FieldView {
  const char *start;
  uint32_t length;

  FieldView() : start(nullptr), length(0) {}

  FieldView(const char *fieldStart, uint32_t fieldLength)
      : start(fieldStart), length(fieldLength) {}
};

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

bool equalsLiteral(const FieldView &field, const char *literal) {
  uint32_t literalLength = static_cast<uint32_t>(std::strlen(literal));

  if (field.length != literalLength) {
    return false;
  }

  return std::memcmp(field.start, literal, literalLength) == 0;
}

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

unsigned long long djb2Raw(const char *data, uint32_t length) {
  unsigned long long hash = 5381ULL;

  for (uint32_t i = 0; i < length; ++i) {
    hash = ((hash << 5) + hash) + static_cast<unsigned char>(data[i]);
  }

  return hash;
}

std::string fieldToString(const FieldView &field) {
  return std::string(field.start, field.length);
}

void trimRightCarriageReturn(FieldView &field) {
  if (field.length > 0 && field.start[field.length - 1] == '\r') {
    --field.length;
  }
}

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

bool DataLoader::load(const std::string &filename, LogStore &store,
                      DuplicateHashSet &gatekeeper) {

  // ================================================================
  // Phase 1: Memory-Map toàn bộ file CSV vào virtual address space
  // ================================================================
  const char *mappedData = nullptr;
  size_t fileSize = 0;

#if defined(_WIN32)
  // --- Windows: CreateFileMapping + MapViewOfFile ---
  HANDLE hFile = CreateFileA(
      filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

  if (hFile == INVALID_HANDLE_VALUE) {
    return false;
  }

  LARGE_INTEGER liSize;
  if (!GetFileSizeEx(hFile, &liSize)) {
    CloseHandle(hFile);
    return false;
  }
  fileSize = static_cast<size_t>(liSize.QuadPart);

  if (fileSize == 0) {
    CloseHandle(hFile);
    return true; // File rỗng — hợp lệ nhưng không có dữ liệu
  }

  HANDLE hMapping =
      CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);

  if (hMapping == nullptr) {
    CloseHandle(hFile);
    return false;
  }

  mappedData = static_cast<const char *>(
      MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0));

  if (mappedData == nullptr) {
    CloseHandle(hMapping);
    CloseHandle(hFile);
    return false;
  }

#else
  // --- Linux/macOS: mmap ---
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd == -1) {
    return false;
  }

  struct stat st;
  if (fstat(fd, &st) == -1) {
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
    close(fd);
    return false;
  }

  // Gợi ý kernel đọc tuần tự (tối ưu prefetch)
  madvise(const_cast<char *>(mappedData), fileSize, MADV_SEQUENTIAL);
#endif

  // ================================================================
  // Phase 2: Quét tuyến tính O(N) trên vùng nhớ đã map
  // FieldView trỏ thẳng vào mapped memory — True Zero-Copy Parsing
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

  // Xử lý dòng cuối nếu file không kết thúc bằng '\n'
  if (success && lineStart < mappedData + fileSize) {
    uint32_t lineLength =
        static_cast<uint32_t>((mappedData + fileSize) - lineStart);
    if (lineLength > 0) {
      success =
          processLine(lineStart, lineLength, firstLine, store, gatekeeper);
    }
  }

  // ================================================================
  // Phase 3: Giải phóng tài nguyên hệ điều hành
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