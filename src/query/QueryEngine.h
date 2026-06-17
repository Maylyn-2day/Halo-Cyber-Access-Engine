#ifndef QUERY_ENGINE_H
#define QUERY_ENGINE_H

#include <cstdint>

/**
 * @file QueryEngine.h
 * @brief Provides analytical query operations for the Halo Engine.
 *
 * This class implements static methods to extract insights from the log data,
 * such as tracking user journeys, resource access histories, and calculating
 * top accessed resources within specific time frames.
 */

class SearchEngine;
class LogStore;
class StringPool;

/**
 * @class QueryEngine
 * @brief Engine responsible for executing complex analytical queries.
 */
class QueryEngine {
private:
  /**
   * @struct TopResource
   * @brief Helper structure to track the access count of a resource.
   */
  struct TopResource {
    uint32_t resourceId; ///< The hashed ID of the resource.
    uint32_t count;      ///< The number of times the resource was accessed.

    /**
     * @brief Default constructor. Initializes resourceId and count to 0.
     */
    TopResource();
  };

  /**
   * @brief Checks if a given timestamp falls within a specified time range.
   * @param timestamp The timestamp to check.
   * @param startTime The start of the time range.
   * @param endTime The end of the time range.
   * @return True if timestamp is within [startTime, endTime], false otherwise.
   */
  static bool isWithinRange(int64_t timestamp, int64_t startTime,
                            int64_t endTime);
  
  /**
   * @brief Inserts a resource into the top 10 array if its count is high enough.
   * @param topResources Array of the current top 10 resources.
   * @param resourceId The ID of the resource to consider.
   * @param count The access count of the resource.
   */
  static void insertIntoTop10(TopResource topResources[10], uint32_t resourceId,
                              uint32_t count);

  /**
   * @brief Converts an epoch timestamp to a formatted string (UTC).
   * @param epoch The timestamp in seconds since epoch.
   * @param buffer The buffer to write the formatted string to.
   * @param bufferSize The size of the buffer.
   */
  static void formatTimestamp(int64_t epoch, char *buffer, uint32_t bufferSize);

public:
  /**
   * @brief Prints the chronological access history of a specific user.
   * @param userId The hashed ID of the user.
   * @param startTime The start of the time window to consider.
   * @param endTime The end of the time window to consider.
   * @param engine Reference to the SearchEngine containing the indices.
   * @param pool Reference to the StringPool to resolve hashed IDs to strings.
   */
  static void printUserJourney(uint32_t userId, int64_t startTime,
                               int64_t endTime, const SearchEngine &engine,
                               const StringPool &pool);

  /**
   * @brief Prints the chronological access history of a specific resource.
   * @param resourceId The hashed ID of the resource.
   * @param startTime The start of the time window to consider.
   * @param endTime The end of the time window to consider.
   * @param engine Reference to the SearchEngine containing the indices.
   * @param pool Reference to the StringPool to resolve hashed IDs to strings.
   */
  static void printResourceJourney(uint32_t resourceId, int64_t startTime,
                                   int64_t endTime, const SearchEngine &engine,
                                   const StringPool &pool);

  /**
   * @brief Calculates and prints the top 10 most accessed resources within a time frame.
   * @param startTime The start of the time window to consider.
   * @param endTime The end of the time window to consider.
   * @param store Reference to the LogStore containing all log entries.
   */
  static void printTop10Resources(int64_t startTime, int64_t endTime,
                                  const LogStore &store);
};

#endif