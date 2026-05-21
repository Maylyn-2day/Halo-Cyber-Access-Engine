// DynamicArray.h
#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <cstdint>

/*
 * Custom dynamic array.
 *
 * This class intentionally avoids STL containers. It owns a contiguous
 * heap-allocated array and releases it in the destructor.
 */
template <typename T>
class DynamicArray {
private:
    T* data;
    uint32_t length;
    uint32_t capacity;

    /*
     * Internal resize used when pushBack exceeds current capacity.
     * Existing elements are copied into the new contiguous block.
     */
    void resize(uint32_t newCapacity) {
        T* newData = new T[newCapacity];

        for (uint32_t i = 0; i < length; ++i) {
            newData[i] = data[i];
        }

        delete[] data;
        data = newData;
        capacity = newCapacity;
    }

public:
    DynamicArray()
        : data(nullptr),
          length(0),
          capacity(0) {}

    explicit DynamicArray(uint32_t initialCapacity)
        : data(nullptr),
          length(0),
          capacity(0) {
        reserve(initialCapacity);
    }

    /*
     * Releases the owned heap array.
     * delete[] safely calls element destructors when T is non-trivial.
     */
    ~DynamicArray() {
        delete[] data;
        data = nullptr;
        length = 0;
        capacity = 0;
    }

    /*
     * Copying is disabled to prevent accidental expensive copies and
     * double-free bugs from shared ownership of the same raw array.
     */
    DynamicArray(const DynamicArray& other) = delete;
    DynamicArray& operator=(const DynamicArray& other) = delete;

    /*
     * Pre-allocates storage to avoid resizing during large CSV ingestion.
     */
    void reserve(uint32_t requestedCapacity) {
        if (requestedCapacity <= capacity) {
            return;
        }

        T* newData = new T[requestedCapacity];

        for (uint32_t i = 0; i < length; ++i) {
            newData[i] = data[i];
        }

        delete[] data;
        data = newData;
        capacity = requestedCapacity;
    }

    /*
     * Appends one element. If reserve() was sized correctly before loading,
     * this should not resize during the hot ingestion path.
     */
    void pushBack(const T& value) {
        if (length == capacity) {
            uint32_t nextCapacity = capacity == 0 ? 8 : capacity * 2;
            resize(nextCapacity);
        }

        data[length] = value;
        ++length;
    }

    T& operator[](uint32_t index) {
        return data[index];
    }

    const T& operator[](uint32_t index) const {
        return data[index];
    }

    uint32_t size() const {
        return length;
    }

    uint32_t getCapacity() const {
        return capacity;
    }

    T* raw() {
        return data;
    }

    const T* raw() const {
        return data;
    }

    void clear() {
        length = 0;
    }
};

#endif