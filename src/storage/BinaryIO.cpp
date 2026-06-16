// BinaryIO.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "BinaryIO.h"

#include <cstdio>
#include <cstring>

#include "../core/LogChunk.h"
#include "../core/LogEntry.h"
#include "../core/StringPool.h"
#include "LogStore.h"

// Platform-specific headers cho file metadata (invalidation check)
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace {

// ============================================================================
// File Header — 64 bytes (padded for alignment)
// ============================================================================
struct BinaryHeader {
  char magic[4];         // "HALO"
  uint32_t version;      // Schema version = 1
  uint64_t totalEntries; // Tổng số LogEntry
  uint32_t chunkCount;   // Số lượng chunks
  uint32_t stringCount;  // Số lượng unique strings
  uint64_t checksum;     // XOR checksum toàn bộ entries
  uint64_t csvFileSize;  // Kích thước file CSV gốc (để detect stale)
  int64_t csvModTime;    // Thời gian sửa đổi cuối của CSV (để detect stale)
  uint8_t reserved[16];  // Dự phòng mở rộng tương lai
};

// ============================================================================
// Helpers
// ============================================================================

/**
 * @brief Tính XOR checksum trên toàn bộ LogEntry data.
 * Nhanh, đơn giản, đủ để detect corruption.
 */
uint64_t computeChecksum(const LogStore &store) {
  uint64_t checksum = 0;

  for (uint32_t c = 0; c < store.chunkCount(); ++c) {
    const LogChunk *chunk = store.getChunk(c);
    const LogEntry *entries = chunk->raw();

    for (uint32_t i = 0; i < chunk->size(); ++i) {
      // XOR từng field số học
      checksum ^= static_cast<uint64_t>(entries[i].timestamp);
      checksum ^= static_cast<uint64_t>(entries[i].userId);
      checksum ^= static_cast<uint64_t>(entries[i].deviceId);
      checksum ^= (static_cast<uint64_t>(entries[i].appId) << 16);
      checksum ^= (static_cast<uint64_t>(entries[i].resourceId) << 32);
    }
  }

  return checksum;
}

/**
 * @brief Lấy file size và modification time (cross-platform).
 * @return true nếu lấy được metadata, false nếu file không tồn tại.
 */
bool getFileMetadata(const char *path, uint64_t &fileSize, int64_t &modTime) {
#if defined(_WIN32)
  WIN32_FILE_ATTRIBUTE_DATA data;
  if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
    return false;
  }

  ULARGE_INTEGER size;
  size.LowPart = data.nFileSizeLow;
  size.HighPart = data.nFileSizeHigh;
  fileSize = size.QuadPart;

  // Combine FILETIME into int64_t
  ULARGE_INTEGER ft;
  ft.LowPart = data.ftLastWriteTime.dwLowDateTime;
  ft.HighPart = data.ftLastWriteTime.dwHighDateTime;
  modTime = static_cast<int64_t>(ft.QuadPart);
#else
  struct stat st;
  if (stat(path, &st) != 0) {
    return false;
  }
  fileSize = static_cast<uint64_t>(st.st_size);
  modTime = static_cast<int64_t>(st.st_mtime);
#endif
  return true;
}

} // namespace

// ============================================================================
// dump() — Ghi trạng thái pre-processed ra file nhị phân
// ============================================================================

bool BinaryIO::dump(const char *filepath, const LogStore &store,
                    const std::string &csvPath) {
  FILE *file = std::fopen(filepath, "wb");
  if (file == nullptr) {
    return false;
  }

  // --- Chuẩn bị Header ---
  BinaryHeader header;
  std::memset(&header, 0, sizeof(header));
  header.magic[0] = 'H';
  header.magic[1] = 'A';
  header.magic[2] = 'L';
  header.magic[3] = 'O';
  header.version = 1;
  header.totalEntries = store.size();
  header.chunkCount = store.chunkCount();
  header.stringCount = store.stringPool.size();
  header.checksum = computeChecksum(store);

  // Lưu metadata CSV để detect stale
  uint64_t csvSize = 0;
  int64_t csvMod = 0;
  if (getFileMetadata(csvPath.c_str(), csvSize, csvMod)) {
    header.csvFileSize = csvSize;
    header.csvModTime = csvMod;
  }

  // Ghi header (sẽ được cập nhật lại nếu cần)
  if (std::fwrite(&header, sizeof(header), 1, file) != 1) {
    std::fclose(file);
    return false;
  }

  // --- Section 1: StringPool ---
  uint32_t stringCount = store.stringPool.size();
  std::fwrite(&stringCount, sizeof(uint32_t), 1, file);

  for (uint32_t i = 0; i < stringCount; ++i) {
    std::string str = store.stringPool.getString(i);
    uint32_t strLen = static_cast<uint32_t>(str.length());
    std::fwrite(&strLen, sizeof(uint32_t), 1, file);
    std::fwrite(str.c_str(), 1, strLen, file);
  }

  // --- Section 2: Log Data (chunk by chunk) ---
  for (uint32_t c = 0; c < store.chunkCount(); ++c) {
    const LogChunk *chunk = store.getChunk(c);

    uint32_t entryCount = chunk->size();
    int64_t minTs = chunk->getMinTimestamp();
    int64_t maxTs = chunk->getMaxTimestamp();

    std::fwrite(&entryCount, sizeof(uint32_t), 1, file);
    std::fwrite(&minTs, sizeof(int64_t), 1, file);
    std::fwrite(&maxTs, sizeof(int64_t), 1, file);

    // Ghi nguyên khối entries[] — POD struct, fwrite an toàn
    if (entryCount > 0) {
      std::fwrite(chunk->raw(), sizeof(LogEntry), entryCount, file);
    }
  }

  std::fclose(file);
  return true;
}

// ============================================================================
// load() — Đọc file nhị phân ngược lại vào LogStore
// ============================================================================

bool BinaryIO::load(const char *filepath, LogStore &store) {
  FILE *file = std::fopen(filepath, "rb");
  if (file == nullptr) {
    return false;
  }

  // --- Đọc và validate Header ---
  BinaryHeader header;
  if (std::fread(&header, sizeof(header), 1, file) != 1) {
    std::fclose(file);
    return false;
  }

  // Validate magic number
  if (header.magic[0] != 'H' || header.magic[1] != 'A' ||
      header.magic[2] != 'L' || header.magic[3] != 'O') {
    std::fclose(file);
    return false;
  }

  // Validate version
  if (header.version != 1) {
    std::fclose(file);
    return false;
  }

  // --- Section 1: Restore StringPool ---
  uint32_t stringCount = 0;
  if (std::fread(&stringCount, sizeof(uint32_t), 1, file) != 1) {
    std::fclose(file);
    return false;
  }

  if (stringCount != header.stringCount) {
    std::fclose(file);
    return false;
  }

  store.stringPool.reserve(stringCount);

  // Buffer tạm để đọc strings
  char strBuffer[4096];

  for (uint32_t i = 0; i < stringCount; ++i) {
    uint32_t strLen = 0;
    if (std::fread(&strLen, sizeof(uint32_t), 1, file) != 1) {
      std::fclose(file);
      return false;
    }

    if (strLen >= sizeof(strBuffer)) {
      std::fclose(file);
      return false; // String quá dài — dữ liệu bất thường
    }

    if (std::fread(strBuffer, 1, strLen, file) != strLen) {
      std::fclose(file);
      return false;
    }

    // Insert vào StringPool (sẽ tự nhận ID tuần tự 0, 1, 2, ...)
    store.stringPool.getOrCreateId(strBuffer, strLen);
  }

  // --- Section 2: Restore Log Data ---
  for (uint32_t c = 0; c < header.chunkCount; ++c) {
    uint32_t entryCount = 0;
    int64_t minTs = 0;
    int64_t maxTs = 0;

    if (std::fread(&entryCount, sizeof(uint32_t), 1, file) != 1 ||
        std::fread(&minTs, sizeof(int64_t), 1, file) != 1 ||
        std::fread(&maxTs, sizeof(int64_t), 1, file) != 1) {
      std::fclose(file);
      return false;
    }

    // Allocate chunk với capacity = entryCount (vừa đủ, không lãng phí)
    LogChunk *chunk = new LogChunk(entryCount);

    // fread nguyên khối entries[] — POD struct, an toàn
    if (entryCount > 0) {
      if (std::fread(chunk->raw(), sizeof(LogEntry), entryCount, file) !=
          entryCount) {
        delete chunk;
        std::fclose(file);
        return false;
      }
    }

    // Restore metadata
    chunk->setCount(entryCount);
    chunk->setTimestampRange(minTs, maxTs);

    // Inject vào LogStore
    store.addLoadedChunk(chunk, entryCount);
  }

  std::fclose(file);

  // --- Verify checksum ---
  uint64_t computed = computeChecksum(store);
  if (computed != header.checksum) {
    return false; // Dữ liệu bị hỏng — caller sẽ fallback về CSV
  }

  return true;
}

// ============================================================================
// isValid() — Kiểm tra binary file có stale so với CSV không
// ============================================================================

bool BinaryIO::isValid(const char *binaryPath, const std::string &csvPath) {
  // Đọc header từ binary file
  FILE *file = std::fopen(binaryPath, "rb");
  if (file == nullptr) {
    return false;
  }

  BinaryHeader header;
  if (std::fread(&header, sizeof(header), 1, file) != 1) {
    std::fclose(file);
    return false;
  }
  std::fclose(file);

  // Validate magic
  if (header.magic[0] != 'H' || header.magic[1] != 'A' ||
      header.magic[2] != 'L' || header.magic[3] != 'O') {
    return false;
  }

  // Lấy metadata hiện tại của CSV
  uint64_t currentSize = 0;
  int64_t currentMod = 0;
  if (!getFileMetadata(csvPath.c_str(), currentSize, currentMod)) {
    return false; // CSV không tồn tại
  }

  // So sánh: nếu CSV đã thay đổi → binary stale
  if (currentSize != header.csvFileSize || currentMod != header.csvModTime) {
    return false;
  }

  return true;
}
