// DataLoader.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "DataLoader.h"

#include <cstdint>
#include <cstdio>
#include <cstring>


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
      store.stringPool.getOrCreateId(parsed.deviceId.start, parsed.deviceId.length),
      store.stringPool.getOrCreateId(parsed.appId.start, parsed.appId.length),
      store.stringPool.getOrCreateId(parsed.resourceId.start, parsed.resourceId.length),
      parsed.eventType, parsed.location, parsed.timestamp);

  return store.insert(entry) != nullptr;
}
} // namespace

bool DataLoader::load(const std::string &filename, LogStore &store,
                      DuplicateHashSet &gatekeeper) {
  FILE *file = std::fopen(filename.c_str(), "rb");

  if (file == nullptr) {
    return false;
  }

  char *readBuffer = new char[READ_BUFFER_SIZE];
  char *lineBuffer = new char[MAX_LINE_SIZE];

  uint32_t lineLength = 0;
  bool firstLine = true;
  bool success = true;

  while (success) {
    size_t bytesRead = std::fread(readBuffer, 1, READ_BUFFER_SIZE, file);

    if (bytesRead == 0) {
      break;
    }

    for (size_t i = 0; i < bytesRead; ++i) {
      char c = readBuffer[i];

      if (c == '\n') {
        success =
            processLine(lineBuffer, lineLength, firstLine, store, gatekeeper);

        lineLength = 0;

        if (!success) {
          break;
        }

        continue;
      }

      if (lineLength >= MAX_LINE_SIZE) {
        lineLength = 0;

        while (i < bytesRead && readBuffer[i] != '\n') {
          ++i;
        }

        break;
      }

      lineBuffer[lineLength] = c;
      ++lineLength;
    }

    if (bytesRead < READ_BUFFER_SIZE) {
      break;
    }
  }

  if (success && lineLength > 0) {
    success = processLine(lineBuffer, lineLength, firstLine, store, gatekeeper);
  }

  std::fclose(file);

  delete[] readBuffer;
  delete[] lineBuffer;

  return success;
}