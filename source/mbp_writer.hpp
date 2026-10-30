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

// rows are handed over in batches, since handing them over one at a time would cost more than formatting them
constexpr size_t MBP_ROWS_PER_BATCH = 2048;
// rows average ~360 bytes, so a batch's output usually fits without growing the buffer
constexpr size_t MBP_BATCH_OUTPUT_SIZE = MBP_ROWS_PER_BATCH * 512;

// a row of the MBP output: the message, and the top levels of the book after it was applied
struct MBPRow {
    MBOMessage message;
    MBPSnapshot snapshot;
    int depth;
};

struct MBPBatch {
    size_t sequence{0}; // submission order, so batches formatted out of order are still written in order
    size_t first_row{0}; // row index of rows[0]
    std::vector<MBPRow> rows;
    std::vector<char> output; // formatted rows
    size_t output_size{0};
};


// formats rows on format threads and writes them on a file writer thread, in the order they were submitted
// row messages are views into the input, which must outlive the writer
class MBPWriter final {
    private:
        std::ofstream file_;
        size_t next_sequence_{0};
        size_t next_row_{0};
        bool closed_{false};

        // batches cycle from free -> filled by the caller -> formatted -> written -> free
        BlockingQueue<MBPBatch> free_batches_;
        BlockingQueue<MBPBatch> unformatted_batches_;
        BlockingQueue<MBPBatch> formatted_batches_;
        std::vector<std::thread> formatters_;
        std::thread file_writer_;

        void write_header();
        void format_batches();
        void write_batches();

    public:
        MBPWriter(const std::string& filename, size_t format_threads);
        ~MBPWriter();

        // returns an empty batch to fill with rows, waiting for one to be written if none are free
        MBPBatch acquire_batch();
        // queues the batch to be written after every previously submitted batch
        void submit(MBPBatch batch);
        // writes every submitted batch and closes the file
        void close();
};
