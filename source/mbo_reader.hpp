#pragma once
#include <optional>
#include <string_view>
#include <thread>
#include <vector>
#include "blocking_queue.hpp"
#include "types.hpp"

// messages are handed over in batches, since handing them over one at a time would cost more than parsing them
constexpr size_t MBO_MESSAGES_PER_BATCH = 4096;
constexpr size_t MBO_BATCH_COUNT = 4;

// parses MBO lines on a background thread, handing batches of messages to the caller in input order
// messages are views into the input, which must outlive the reader and every message read from it
class MBOReader final {
    private:
        using Batch = std::vector<MBOMessage>;

        std::string_view input_;
        // batches cycle from free -> parsed on the parser thread -> processed by the caller -> free
        BlockingQueue<Batch> free_batches_;
        BlockingQueue<Batch> parsed_batches_;
        std::thread parser_;

        void parse_batches();

    public:
        explicit MBOReader(std::string_view input);
        ~MBOReader();

        // returns the next batch of messages, or nullopt once the whole input has been read
        std::optional<Batch> next_batch();
        // hands back a batch the caller is done with, so it can be refilled
        void recycle(Batch batch);
};
