#pragma once
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include "blocking_queue.hpp"
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

// rows are formatted into a buffer and written to the file in large chunks
constexpr size_t MBP_OUTPUT_BUFFER_SIZE = 1 << 20;
// two buffers, so one can be filled while the other is written
constexpr size_t MBP_OUTPUT_BUFFER_COUNT = 2;


class MBPWriter final {
    private:
        struct OutputBuffer {
            std::vector<char> data;
            size_t used;
        };

        std::ofstream file_;
        OutputBuffer buffer_;
        size_t row_index_;

        // filled buffers are written to the file on a separate thread, then handed back for reuse
        BlockingQueue<OutputBuffer> free_buffers_;
        BlockingQueue<OutputBuffer> full_buffers_;
        std::thread file_writer_;

        void write_header();
        void flush();
        void write_buffers();

    public:
        MBPWriter(const std::string& filename);
        ~MBPWriter();

        void write_snapshot(const MBOMessage& message, const MBPSnapshot& snapshot, int depth = -1);
};
