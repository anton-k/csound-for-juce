#pragma once

#include <vector>
#include <cstring>
#include <type_traits>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <cstddef>

namespace csd_plugin {

/// Fast FIFO queue for single-thread use only.
template <typename T>
class FastFifo {
    static_assert(
        std::is_trivially_copyable<T>::value,
        "FastFifo requires trivially copyable types for fast memcpy"
    );

public:
    FastFifo() = default;

    explicit FastFifo(int capacity) {
        reset(capacity);
    }

    void reset(int capacity) {
        const uint64_t cap = (capacity < 1) ? 1 : static_cast<uint64_t>(capacity);

        size_ = 1;
        while (size_ < cap) {
            size_ <<= 1; // Power of 2
        }

        mask_ = size_ - 1;

        buffer_.resize(static_cast<size_t>(size_));
        read_pos_ = 0;
        write_pos_ = 0;
    }

    // --- Single Item Operations ---

    bool push(const T& item) {
        if (write_pos_ - read_pos_ >= size_) {
            return false; // Prevent overwrite
        }

        buffer_[static_cast<size_t>(write_pos_ & mask_)] = item;
        ++write_pos_;
        return true;
    }

    bool read(T& dest) {
        if (write_pos_ == read_pos_) {
            return false;
        }

        dest = buffer_[static_cast<size_t>(read_pos_ & mask_)];
        ++read_pos_;
        return true;
    }

    T* peek() {
        if (write_pos_ == read_pos_) {
            return nullptr;
        }

        return &buffer_[static_cast<size_t>(read_pos_ & mask_)];
    }

    void pop() {
        if (write_pos_ != read_pos_) {
            ++read_pos_;
        }
    }

    // --- Block Operations ---

    bool write_block(const T* data, int num_items) {
        if (num_items <= 0) {
            return true;
        }

        if (data == nullptr) {
            return false;
        }

        const uint64_t n64 = static_cast<uint64_t>(num_items);

        if (write_pos_ - read_pos_ + n64 > size_) {
            return false; // Not enough space
        }

        const uint64_t write_idx64 = write_pos_ & mask_;
        const uint64_t space_until_end64 = size_ - write_idx64;

        const size_t n = static_cast<size_t>(n64);
        const size_t write_idx = static_cast<size_t>(write_idx64);
        const size_t space_until_end = static_cast<size_t>(space_until_end64);

        if (space_until_end >= n) {
            std::memcpy(
                &buffer_[write_idx],
                data,
                n * sizeof(T)
            );
        } else {
            std::memcpy(
                &buffer_[write_idx],
                data,
                space_until_end * sizeof(T)
            );

            std::memcpy(
                &buffer_[0],
                data + space_until_end,
                (n - space_until_end) * sizeof(T)
            );
        }

        write_pos_ += n64;
        return true;
    }

    bool read_block(T* dest, int num_items) {
        if (num_items <= 0) {
            return true;
        }

        if (dest == nullptr) {
            return false;
        }

        const uint64_t n64 = static_cast<uint64_t>(num_items);

        if (write_pos_ - read_pos_ < n64) {
            return false; // Not enough data
        }

        const uint64_t read_idx64 = read_pos_ & mask_;
        const uint64_t space_until_end64 = size_ - read_idx64;

        const size_t n = static_cast<size_t>(n64);
        const size_t read_idx = static_cast<size_t>(read_idx64);
        const size_t space_until_end = static_cast<size_t>(space_until_end64);

        if (space_until_end >= n) {
            std::memcpy(
                dest,
                &buffer_[read_idx],
                n * sizeof(T)
            );
        } else {
            std::memcpy(
                dest,
                &buffer_[read_idx],
                space_until_end * sizeof(T)
            );

            std::memcpy(
                dest + space_until_end,
                &buffer_[0],
                (n - space_until_end) * sizeof(T)
            );
        }

        read_pos_ += n64;
        return true;
    }

    int read_block_partial(T* dest, int num_items) {
        if (num_items <= 0) {
            return 0;
        }

        if (dest == nullptr) {
            return 0;
        }

        const uint64_t available = write_pos_ - read_pos_;

        if (available == 0) {
            return 0;
        }

        uint64_t n64 = static_cast<uint64_t>(num_items);

        if (n64 > available) {
            n64 = available;
        }

        const uint64_t read_idx64 = read_pos_ & mask_;
        const uint64_t space_until_end64 = size_ - read_idx64;

        const size_t n = static_cast<size_t>(n64);
        const size_t read_idx = static_cast<size_t>(read_idx64);
        const size_t space_until_end = static_cast<size_t>(space_until_end64);

        if (space_until_end >= n) {
            std::memcpy(
                dest,
                &buffer_[read_idx],
                n * sizeof(T)
            );
        } else {
            std::memcpy(
                dest,
                &buffer_[read_idx],
                space_until_end * sizeof(T)
            );

            std::memcpy(
                dest + space_until_end,
                &buffer_[0],
                (n - space_until_end) * sizeof(T)
            );
        }

        read_pos_ += n64;

        return static_cast<int>(n64);
    }

    // --- State ---

    int get_size() const {
        const uint64_t used = write_pos_ - read_pos_;
        const uint64_t max_int = static_cast<uint64_t>(std::numeric_limits<int>::max());

        return static_cast<int>(used > max_int ? max_int : used);
    }

    int get_capacity() const {
        const uint64_t max_int = static_cast<uint64_t>(std::numeric_limits<int>::max());

        return static_cast<int>(size_ > max_int ? max_int : size_);
    }

    void clear() {
        read_pos_ = write_pos_ = 0;
    }

    int get_free_space() const {
        const uint64_t free_space = size_ - (write_pos_ - read_pos_);
        const uint64_t max_int = static_cast<uint64_t>(std::numeric_limits<int>::max());

        return static_cast<int>(free_space > max_int ? max_int : free_space);
    }

private:
    std::vector<T> buffer_;
    uint64_t size_ = 0;
    uint64_t mask_ = 0;
    uint64_t read_pos_ = 0;
    uint64_t write_pos_ = 0;
};

} // namespace csd_plugin
