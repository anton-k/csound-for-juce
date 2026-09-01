
#include <catch2/catch_test_macros.hpp>
#include "csd_plugin/audio/FastFifo.h"
#include <vector>
#include <numeric>
#include <cstring>

using namespace csd_plugin;

// A custom trivially copyable struct to test with (simulating RawMidiEvent)
struct TestStruct {
    int32_t id;
    float value;

    bool operator==(const TestStruct& other) const {
        return id == other.id && value == other.value;
    }
};

TEST_CASE("FastFifo: Basic initialization and capacity", "[FastFifo]") {
    FastFifo<int> fifo(10); // Requests 10, should round up to 16 (power of 2)

    REQUIRE(fifo.get_capacity() == 16);
    REQUIRE(fifo.get_size() == 0);
}

TEST_CASE("FastFifo: Empty buffer edge cases", "[FastFifo]") {
    FastFifo<int> fifo(4);
    int val = -1;

    REQUIRE(fifo.read(val) == false);
    REQUIRE(fifo.peek() == nullptr);

    // Popping an empty buffer should not crash or underflow
    fifo.pop();
    REQUIRE(fifo.get_size() == 0);
}

TEST_CASE("FastFifo: Full buffer edge cases", "[FastFifo]") {
    FastFifo<int> fifo(4); // Capacity will be 4

    REQUIRE(fifo.push(1));
    REQUIRE(fifo.push(2));
    REQUIRE(fifo.push(3));
    REQUIRE(fifo.push(4));

    // Buffer is now full
    REQUIRE(fifo.get_size() == 4);
    REQUIRE(fifo.push(5) == false); // Should fail gracefully

    // Block write should also fail if not enough space
    int data[2] = {5, 6};
    REQUIRE(fifo.write_block(data, 2) == false);
}

TEST_CASE("FastFifo: Single item wrap-around", "[FastFifo]") {
    FastFifo<int> fifo(4); // Capacity 4, mask 3

    // Fill and empty to push the read/write pointers to the end
    for (int i = 0; i < 3; ++i) {
        fifo.push(i);
        int val; fifo.read(val);
    }

    // Now write_pos = 3, read_pos = 3.
    // Next push will write to index 3, then wrap to 0.
    REQUIRE(fifo.push(10));
    REQUIRE(fifo.push(20));
    REQUIRE(fifo.push(30)); // This wraps to index 0

    int v1, v2, v3;
    REQUIRE(fifo.read(v1)); REQUIRE(v1 == 10);
    REQUIRE(fifo.read(v2)); REQUIRE(v2 == 20);
    REQUIRE(fifo.read(v3)); REQUIRE(v3 == 30);
}

TEST_CASE("FastFifo: Block operations without wrap-around", "[FastFifo]") {
    FastFifo<int> fifo(16);
    std::vector<int> in_data(8);
    std::iota(in_data.begin(), in_data.end(), 100); // 100, 101, ... 107

    REQUIRE(fifo.write_block(in_data.data(), 8));
    REQUIRE(fifo.get_size() == 8);

    std::vector<int> out_data(8, 0);
    REQUIRE(fifo.read_block(out_data.data(), 8));

    REQUIRE(out_data == in_data);
    REQUIRE(fifo.get_size() == 0);
}

TEST_CASE("FastFifo: Block operations WITH wrap-around (Critical Edge Case)", "[FastFifo]") {
    FastFifo<int> fifo(8); // Capacity 8

    // 1. Advance the pointers so we are near the end of the internal buffer
    int dummy;
    for (int i = 0; i < 6; ++i) {
        fifo.push(i);
        fifo.read(dummy);
    }
    // write_pos is now 6, read_pos is 6.
    // Internal buffer has 2 spaces left at the end (indices 6 and 7).

    // 2. Write a block of 5 items. This MUST wrap around the boundary.
    std::vector<int> in_data = {10, 20, 30, 40, 50};
    REQUIRE(fifo.write_block(in_data.data(), 5));
    REQUIRE(fifo.get_size() == 5);

    // 3. Read it back. This MUST also wrap around the boundary.
    std::vector<int> out_data(5, 0);
    REQUIRE(fifo.read_block(out_data.data(), 5));

    REQUIRE(out_data == in_data);
    REQUIRE(fifo.get_size() == 0);
}

TEST_CASE("FastFifo: Block read/write failure on insufficient space/data", "[FastFifo]") {
    FastFifo<int> fifo(8);
    int data[5] = {1, 2, 3, 4, 5};

    // Write block too large
    REQUIRE(fifo.write_block(data, 9) == false);

    // Write partial, then try to overfill
    REQUIRE(fifo.write_block(data, 5));
    REQUIRE(fifo.write_block(data, 4) == false); // Only 3 spaces left

    // Read block too large
    int out[6];
    REQUIRE(fifo.read_block(out, 6) == false); // Only 5 items available

    // Ensure state wasn't corrupted by failed operations
    REQUIRE(fifo.get_size() == 5);
}

TEST_CASE("FastFifo: Clear and Reset", "[FastFifo]") {
    FastFifo<int> fifo(8);
    fifo.push(1);
    fifo.push(2);

    fifo.clear();
    REQUIRE(fifo.get_size() == 0);
    REQUIRE(fifo.get_capacity() == 8); // Capacity unchanged

    int val;
    REQUIRE(fifo.read(val) == false);

    fifo.reset(16);
    REQUIRE(fifo.get_capacity() == 16);
    REQUIRE(fifo.get_size() == 0);
}

TEST_CASE("FastFifo: Trivially copyable custom struct (RawMidiEvent simulation)", "[FastFifo]")
{
    FastFifo<TestStruct> fifo(4);

    TestStruct s1{1, 1.5f};
    TestStruct s2{2, 2.5f};

    REQUIRE(fifo.push(s1));
    REQUIRE(fifo.push(s2));

    TestStruct out;
    REQUIRE(fifo.read(out));
    REQUIRE(out == s1);

    REQUIRE(fifo.read(out));
    REQUIRE(out == s2);
}

