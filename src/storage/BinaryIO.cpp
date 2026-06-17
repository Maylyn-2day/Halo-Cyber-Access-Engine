// BinaryIO.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "BinaryIO.h"

#include <cstdio>
#include <cstring>
#include <iostream>

#include "../core/LogChunk.h"
#include "../core/LogEntry.h"
#include "../core/StringPool.h"
#include "LogStore.h"

// Platform-specific headers for file metadata (invalidation check)
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
  uint64_t totalEntries; // Total number of LogEntries
  uint32_t chunkCount;   // Number of chunks
  uint32_t stringCount;  // Number of unique strings
  uint64_t checksum;     // XOR checksum of all entries
  uint64_t csvFileSize;  // Original CSV file size (to detect stale)
  int64_t csvModTime;    // Last modification time of CSV (to detect stale)
  uint8_t reserved[16];  // Reserved for future expansion
};

// ============================================================================
// Helpers
// ============================================================================

/**
 * @brief Calculate XOR checksum on all LogEntry data.
 * Fast, simple, enough to detect corruption.
 */
uint64_t computeChecksum(const LogStore &store) {
  uint64_t checksum = 0;

  for (uint32_t c = 0; c < store.chunkCount(); ++c) {
    const LogChunk *chunk = store.getChunk(c);
    const LogEntry *entries = chunk->raw();

    for (uint32_t i = 0; i < chunk->size(); ++i) {
      // XOR each numeric field
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
 * @brief Get file size and modification time (cross-platform).
 * @return true if metadata retrieved, false if file does not exist.
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
// dump() - Write pre-processed state to binary file
// ============================================================================

bool BinaryIO::dump(const char *filepath, const LogStore &store,
                    const std::string &csvPath) {
  FILE *file = std::fopen(filepath, "wb");
  if (file == nullptr) {
    std::cerr << "[BinaryIO] dump: Cannot create file '" << filepath << "'\n";
    return false;
  }

  // --- Prepare Header ---
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

  // Save CSV metadata to detect stale
  uint64_t csvSize = 0;
  int64_t csvMod = 0;
  if (getFileMetadata(csvPath.c_str(), csvSize, csvMod)) {
    header.csvFileSize = csvSize;
    header.csvModTime = csvMod;
  }

  // Write header (will be updated later if needed)
  if (std::fwrite(&header, sizeof(header), 1, file) != 1) {
    std::cerr << "[BinaryIO] dump: Failed to write header\n";
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

    // Write entire entries[] block - POD struct, safe fwrite
    if (entryCount > 0) {
      std::fwrite(chunk->raw(), sizeof(LogEntry), entryCount, file);
    }
  }

  std::fclose(file);
  return true;
}

// ============================================================================
// load() - Read binary file back into LogStore
// ============================================================================

bool BinaryIO::load(const char *filepath, LogStore &store) {
  FILE *file = std::fopen(filepath, "rb");
  if (file == nullptr) {
    std::cerr << "[BinaryIO] load: Cannot open file '" << filepath << "'\n";
    return false;
  }

  // --- Read and validate Header ---
  BinaryHeader header;
  if (std::fread(&header, sizeof(header), 1, file) != 1) {
    std::cerr << "[BinaryIO] load: Failed to read header (file too small?)\n";
    std::fclose(file);
    return false;
  }

  // Validate magic number
  if (header.magic[0] != 'H' || header.magic[1] != 'A' ||
      header.magic[2] != 'L' || header.magic[3] != 'O') {
    std::cerr << "[BinaryIO] load: Invalid magic number (not a Halo binary)\n";
    std::fclose(file);
    return false;
  }

  // Validate version
  if (header.version != 1) {
    std::cerr << "[BinaryIO] load: Unsupported version " << header.version
              << " (expected 1)\n";
    std::fclose(file);
    return false;
  }

  // --- Section 1: Restore StringPool ---
  uint32_t stringCount = 0;
  if (std::fread(&stringCount, sizeof(uint32_t), 1, file) != 1) {
    std::cerr << "[BinaryIO] load: Failed to read string count\n";
    std::fclose(file);
    return false;
  }

  if (stringCount != header.stringCount) {
    std::cerr << "[BinaryIO] load: String count mismatch (header="
              << header.stringCount << ", section=" << stringCount << ")\n";
    std::fclose(file);
    return false;
  }

  store.stringPool.reserve(stringCount);

  // Temporary buffer to read strings
  char strBuffer[4096];

  for (uint32_t i = 0; i < stringCount; ++i) {
    uint32_t strLen = 0;
    if (std::fread(&strLen, sizeof(uint32_t), 1, file) != 1) {
      std::cerr << "[BinaryIO] load: Failed to read string length at index " << i << "\n";
      std::fclose(file);
      return false;
    }

    if (strLen >= sizeof(strBuffer)) {
      std::cerr << "[BinaryIO] load: String #" << i << " too long ("
                << strLen << " bytes, max 4095)\n";
      std::fclose(file);
      return false;
    }

    if (std::fread(strBuffer, 1, strLen, file) != strLen) {
      std::cerr << "[BinaryIO] load: Failed to read string data at index " << i << "\n";
      std::fclose(file);
      return false;
    }

    // Insert into StringPool (will auto-assign sequential IDs 0, 1, 2, ...)
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
      std::cerr << "[BinaryIO] load: Failed to read chunk #" << c << " metadata\n";
      std::fclose(file);
      return false;
    }

    // Allocate chunk with capacity = entryCount (just enough, no waste)
    LogChunk *chunk = new LogChunk(entryCount);

    // fread entire entries[] block - POD struct, safe
    if (entryCount > 0) {
      if (std::fread(chunk->raw(), sizeof(LogEntry), entryCount, file) !=
          entryCount) {
        std::cerr << "[BinaryIO] load: Failed to read " << entryCount
                  << " entries from chunk #" << c << "\n";
        delete chunk;
        std::fclose(file);
        return false;
      }
    }

    // Restore metadata
    chunk->setCount(entryCount);
    chunk->setTimestampRange(minTs, maxTs);

    // Inject into LogStore
    store.addLoadedChunk(chunk, entryCount);
  }

  std::fclose(file);

  // --- Verify checksum ---
  uint64_t computed = computeChecksum(store);
  if (computed != header.checksum) {
    std::cerr << "[BinaryIO] load: Checksum mismatch — file corrupted\n";
    return false;
  }

  return true;
}

// ============================================================================
// isValid() - Check if binary file is stale compared to CSV
// ============================================================================

bool BinaryIO::isValid(const char *binaryPath, const std::string &csvPath) {
  // Read header from binary file
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

  // Get current metadata of CSV
  uint64_t currentSize = 0;
  int64_t currentMod = 0;
  if (!getFileMetadata(csvPath.c_str(), currentSize, currentMod)) {
    return false; // CSV does not exist
  }

  // Compare: if CSV has changed -> binary stale
  if (currentSize != header.csvFileSize || currentMod != header.csvModTime) {
    return false;
  }

  return true;
}
