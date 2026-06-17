// DataLoader.h
#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include <string>

#include "../core/DuplicateHashSet.h"
#include "LogStore.h"

/**
 * @brief High-speed CSV Ingestion system.
 *
 * Architecture decision: DataLoader is designed based on the
 * "True Zero-Copy Parsing" principle using Memory-Mapped I/O. Instead of reading
 * byte blocks via fread and copying to a lineBuffer, this class maps the entire
 * CSV file directly into virtual address space via CreateFileMapping
 * (Windows) or mmap (Linux). Raw Character pointers (FieldView)
 * point directly into the mapped memory - without any intermediate
 * copies. This completely eliminates fread syscall costs,
 * buffer management, and byte-by-byte copying, pushing ingestion bandwidth
 * to the physical limits of the hard drive.
 */
class DataLoader {
public:
  /**
   * @brief Activates the Load process directly from a physical file into
   * the In-Memory Store.
   *
   * Algorithm: Data is read in 256KB chunks into `readBuffer` to
   * optimize L1/L2 Cache latency. At each newline `\n` interrupt cycle, the
   * system hashes the interpolated string and looks up instantly via `gatekeeper` to block
   * O(1) duplicate trash. If valid, the system extracts Metadata and indexes
   * via `LogStore`. This mechanism operates linearly and continuously with a
   * time complexity of O(N) according to the total byte blocks of the log file.
   *
   * @param filename CSV file path storing raw logs on physical disk.
   * @param store Internal storage (In-Memory LogStore) responsible for receiving
   * and indexing.
   * @param gatekeeper Data fingerprint filter structure (DuplicateHashSet) eliminating
   * duplicate lines.
   * @return true Ingestion process completed completely, Data Pipeline
   * processing finished.
   * @return false I/O process rejected due to corrupted file, non-existence, or access
   * denied.
   */
  static bool load(const std::string &filename, LogStore &store,
                   DuplicateHashSet &gatekeeper);
};

#endif
