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
        uint32_t cap = static_cast<uint32_t>(capacity);
        while (size_ < cap) size_ <<= 1; // Power of 2
        mask_ = size_ - 1;

        buffer_.resize(static_cast<size_t>(size_));
        read_pos_ = 0;
        write_pos_ = 0;
    }

    // --- Single Item Operations ---

    bool push(const T& item) {
        if (write_pos_ - read_pos_ >= size_) return false; // Prevent overwrite
        buffer_[write_pos_ & mask_] = item;
        ++write_pos_;
        return true;
    }

    bool read(T& dest) {
        if (write_pos_ == read_pos_) return false;
        dest = buffer_[read_pos_ & mask_];
        ++read_pos_;
        return true;
    }

    T* peek() {
        if (write_pos_ == read_pos_) return nullptr;
        return &buffer_[read_pos_ & mask_];
    }

    void pop() {
        if (write_pos_ != read_pos_) ++read_pos_;
    }

    // --- Block Operations ---

    bool write_block(const T* data, int num_items) {
        if (num_items <= 0) return true;
        uint32_t n = static_cast<uint32_t>(num_items);
        if (write_pos_ - read_pos_ + n > size_) return false; // Not enough space

        uint32_t write_idx = write_pos_ & mask_;
        uint32_t space_until_end = size_ - write_idx;

        if (space_until_end >= n) {
            std::memcpy(&buffer_[write_idx], data, static_cast<size_t>(n) * sizeof(T));
        } else {
            std::memcpy(&buffer_[write_idx], data, static_cast<size_t>(space_until_end) *
sizeof(T));
            std::memcpy(&buffer_[0], data + space_until_end, static_cast<size_t>(n -
space_until_end) * sizeof(T));
        }
        write_pos_ += n;
        return true;
    }

    bool read_block(T* dest, int num_items) {
        if (num_items <= 0) return true;
        uint32_t n = static_cast<uint32_t>(num_items);
        if (write_pos_ - read_pos_ < n) return false; // Not enough data

        uint32_t read_idx = read_pos_ & mask_;
        uint32_t space_until_end = size_ - read_idx;

        if (space_until_end >= n) {
            std::memcpy(dest, &buffer_[read_idx], static_cast<size_t>(n) * sizeof(T));
        } else {
            std::memcpy(dest, &buffer_[read_idx], static_cast<size_t>(space_until_end) * sizeof(T));
            std::memcpy(dest + space_until_end, &buffer_[0], static_cast<size_t>(n -
space_until_end) * sizeof(T));
        }
        read_pos_ += n;
        return true;
    }

    // --- State ---

    int get_size() const { return static_cast<int>(write_pos_ - read_pos_); }
    int get_capacity() const { return static_cast<int>(size_); }
    void clear() { read_pos_ = write_pos_ = 0; }

private:
    std::vector<T> buffer_;
    uint32_t size_ = 0;
    uint32_t mask_ = 0;
    uint32_t read_pos_ = 0;
    uint32_t write_pos_ = 0;
};

} // namespace csd_plugin
