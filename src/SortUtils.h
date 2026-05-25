// SortUtils.h
#ifndef SORT_UTILS_H
#define SORT_UTILS_H

#include <cstdint>

#include "DynamicArray.h"
#include "LogEntry.h"

/*
 * SortUtils provides custom sorting helpers for Halo timelines.
 *
 * No STL containers or <algorithm> are used. The implementation is a stable
 * Merge Sort over LogEntry pointers, ordered by LogEntry::timestamp.
 */
class SortUtils {
private:
    /*
     * Recursively sorts arr[left..right].
     *
     * temp is allocated once by the public entry point and reused by every
     * recursive call. This avoids repeated heap allocation during sorting.
     */
    static void mergeSort(
        DynamicArray<const LogEntry*>& arr,
        const LogEntry** temp,
        uint32_t left,
        uint32_t right
    ) {
        if (left >= right) {
            return;
        }

        uint32_t middle = left + (right - left) / 2;

        mergeSort(arr, temp, left, middle);
        mergeSort(arr, temp, middle + 1, right);
        merge(arr, temp, left, middle, right);
    }

    /*
     * Merges two sorted ranges:
     * - left half:  arr[left..middle]
     * - right half: arr[middle + 1..right]
     *
     * Stable behavior:
     * If timestamps are equal, the pointer from the left half is chosen first,
     * preserving original relative order for equal timestamps.
     */
    static void merge(
        DynamicArray<const LogEntry*>& arr,
        const LogEntry** temp,
        uint32_t left,
        uint32_t middle,
        uint32_t right
    ) {
        uint32_t leftIndex = left;
        uint32_t rightIndex = middle + 1;
        uint32_t tempIndex = left;

        while (leftIndex <= middle && rightIndex <= right) {
            const LogEntry* leftEntry = arr[leftIndex];
            const LogEntry* rightEntry = arr[rightIndex];

            if (leftEntry->timestamp <= rightEntry->timestamp) {
                temp[tempIndex] = leftEntry;
                ++leftIndex;
            } else {
                temp[tempIndex] = rightEntry;
                ++rightIndex;
            }

            ++tempIndex;
        }

        while (leftIndex <= middle) {
            temp[tempIndex] = arr[leftIndex];
            ++leftIndex;
            ++tempIndex;
        }

        while (rightIndex <= right) {
            temp[tempIndex] = arr[rightIndex];
            ++rightIndex;
            ++tempIndex;
        }

        for (uint32_t i = left; i <= right; ++i) {
            arr[i] = temp[i];
        }
    }

public:
    /*
     * Sorts a DynamicArray of LogEntry pointers by timestamp ascending.
     *
     * A single temporary pointer buffer is allocated once and released once.
     * This prevents recursive heap churn and keeps sorting predictable for
     * large timelines.
     */
    static void sortByTimestamp(DynamicArray<const LogEntry*>& arr) {
        uint32_t count = arr.size();

        if (count < 2) {
            return;
        }

        const LogEntry** temp = new const LogEntry*[count];

        mergeSort(arr, temp, 0, count - 1);

        delete[] temp;
        temp = nullptr;
    }
};

#endif