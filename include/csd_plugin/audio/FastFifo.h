#pragma once

#include <vector>
#include <cstring>
#include <type_traits>
#include <cstdint>

namespace csd_plugin {

template <typename T>
class FastFifo {
    static_assert(std::is_trivially_copyable_v<T>, "FastFifo requires trivially copyable types for fast memcpy");

public:
    FastFifo() = default;

    explicit FastFifo(int capacity) {
        reset(capacity);
    }

    void reset(int capacity) {
        size_ = 1;
        while (size_ < capacity) size_ <<= 1; // Power of 2
        mask_ = size_ - 1;

        buffer_.resize(size_);
        read_pos_ = 0;
        write_pos_ = 0;
    }

    // --- Single Item Operations ---

    bool push(const T& item) {
        if (get_size() >= size_) return false; // Prevent overwrite
        buffer_[write_pos_ & mask_] = item;
        ++write_pos_;
        return true;
    }

    bool read(T& dest) {
        if (get_size() == 0) return false;
        dest = buffer_[read_pos_ & mask_];
        ++read_pos_;
        return true;
    }

    T* peek() {
        if (get_size() == 0) return nullptr;
        return &buffer_[read_pos_ & mask_];
    }

    void pop() {
        if (get_size() > 0) ++read_pos_;
    }

    // --- Block Operations ---

    bool write_block(const T* data, int num_items) {
        if (get_size() + num_items > size_) return false; // Not enough space

        uint32_t write_idx = write_pos_ & mask_;
        uint32_t space_until_end = size_ - write_idx;

        if (space_until_end >= num_items) {
            std::memcpy(&buffer_[write_idx], data, num_items * sizeof(T));
        } else {
            std::memcpy(&buffer_[write_idx], data, space_until_end * sizeof(T));
            std::memcpy(&buffer_[0], data + space_until_end, (num_items - space_until_end) * sizeof(T));
        }
        write_pos_ += num_items;
        return true;
    }

    bool read_block(T* dest, int num_items) {
        if (get_size() < num_items) return false; // Not enough data

        uint32_t read_idx = read_pos_ & mask_;
        uint32_t space_until_end = size_ - read_idx;

        if (space_until_end >= num_items) {
            std::memcpy(dest, &buffer_[read_idx], num_items * sizeof(T));
        } else {
            std::memcpy(dest, &buffer_[read_idx], space_until_end * sizeof(T));
            std::memcpy(dest + space_until_end, &buffer_[0], (num_items - space_until_end) * sizeof(T));
        }
        read_pos_ += num_items;
        return true;
    }

    // --- State ---

    int get_size() const { return static_cast<int>(write_pos_ - read_pos_); }
    int get_capacity() const { return size_; }
    void clear() { read_pos_ = write_pos_ = 0; }

private:
    std::vector<T> buffer_;
    int size_ = 0;
    uint32_t mask_ = 0;
    uint32_t read_pos_ = 0;
    uint32_t write_pos_ = 0;
};

} // namespace csd_plugin

