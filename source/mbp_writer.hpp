#pragma once
#include <fstream>
#include <string>
#include <vector>
#include "types.hpp"

// max size of the fixed-width part of an MBP output line, excluding fields copied as-is from the MBO message
constexpr size_t MAX_MBP_LINE_SIZE =
    20 + // 20 max chars for row index (size_t)
    3 + // 3 chars for rtype
    1 + // 1 char for action
    1 + // 1 char for side
    11 + // 11 max chars for depth (int)
    21 + // 21 max chars for price (int64_t nanos as decimal)
    10 + // 10 max chars for size (uint32_t)
    3 + // 3 max chars for flags (uint8_t)
    20 + // 20 max chars for order ID (uint64_t)
    10*2*(21+10+10) + // 10 levels * 2 (bid/ask) * (21 max chars for price + 10 max chars for size + 10 max chars for count)
    80 + // 80 commas
    2 // \r\n
;

// rows are formatted into this buffer and written to the file in large chunks
constexpr size_t MBP_OUTPUT_BUFFER_SIZE = 1 << 20;


class MBPWriter final {
    private:
        std::ofstream file_;
        std::vector<char> buffer_;
        size_t buffer_used_;
        size_t row_index_;

        void write_header();
        void flush();

    public:
        MBPWriter(const std::string& filename);
        ~MBPWriter();

        void write_snapshot(const MBOMessage& message, const MBPSnapshot& snapshot, int depth = -1);
};
