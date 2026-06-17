// BinaryIO.h
#ifndef BINARY_IO_H
#define BINARY_IO_H

#include <cstdint>
#include <string>

class LogStore;

/**
 * @brief Binary Serialization read/write module for the
 * pre-processed state of Halo Engine.
 *
 * Architecture decision: Instead of parsing CSV every startup (~2-5 seconds
 * for 10M rows), BinaryIO allows taking a "snapshot" of the entire LogStore
 * + StringPool into a binary file. On the next run, the system reads
 * the byte block straight into RAM using fread - speed is only limited by
 * disk bandwidth (~50ms on SSD). LogEntry is a 32-byte POD struct so it can
 * be fwrite/fread directly as a block without serializing individual fields.
 */
class BinaryIO {
public:
  /**
   * @brief Dump the entire LogStore + StringPool to a binary file.
   *
   * Format: Header (32 bytes) -> StringPool strings -> LogChunk entries
   *
   * @param filepath Output file path (Ex: "halo_db.bin").
   * @param store LogStore containing processed data.
   * @param csvPath Original CSV file path (saved in header to detect stale).
   * @return true if write is successful.
   */
  static bool dump(const char *filepath, const LogStore &store,
                   const std::string &csvPath);

  /**
   * @brief Load the binary file back into LogStore + StringPool.
   *
   * Flow: Validate header -> Restore StringPool -> Restore LogChunks
   *
   * @param filepath Binary file path.
   * @param store Empty LogStore to inject data into.
   * @return true if load is successful (valid magic + version + checksum).
   */
  static bool load(const char *filepath, LogStore &store);

  /**
   * @brief Checks if the binary file is still valid compared to the original CSV file.
   *
   * Compares file size and modification time of CSV at dump vs current.
   *
   * @param binaryPath Binary file path.
   * @param csvPath Current CSV file path.
   * @return true if binary is still fresh, false if stale.
   */
  static bool isValid(const char *binaryPath, const std::string &csvPath);
};

#endif
