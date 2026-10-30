#include "mbo_reader.hpp"
#include "mbo_parser.hpp"

MBOReader::MBOReader(std::string_view input) : input_(input) {
    for (size_t i = 0; i < MBO_BATCH_COUNT; ++i) {
        Batch batch;
        batch.reserve(MBO_MESSAGES_PER_BATCH);
        free_batches_.push(std::move(batch));
    }
    parser_ = std::thread(&MBOReader::parse_batches, this);
}

MBOReader::~MBOReader() {
    // stops the parser early if the caller stopped reading, e.g. after an error
    free_batches_.close();
    parser_.join();
}

std::optional<MBOReader::Batch> MBOReader::next_batch() {
    return parsed_batches_.pop();
}

void MBOReader::recycle(Batch batch) {
    batch.clear();
    free_batches_.push(std::move(batch));
}

void MBOReader::parse_batches() {
    StreamingMBOParser parser;
    std::optional<Batch> batch = free_batches_.pop();
    std::string_view remaining = input_;
    bool first_line = true; // Skip header line

    while (batch && !remaining.empty()) {
        size_t end = remaining.find('\n');
        std::string_view line = remaining.substr(0, end);
        remaining.remove_prefix(end == std::string_view::npos ? remaining.size() : end + 1);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (first_line) {
            first_line = false;
            continue;
        }

        MBOMessage message{
            Action::Add, Side::None, 0, 0, 0, Flag{static_cast<uint8_t>(0)},
            "", "", "", "", "", "", ""
        };
        if (!parser.parse_MBO_line(line, message)) {
            continue;
        }
        batch->push_back(message);
        if (batch->size() == MBO_MESSAGES_PER_BATCH) {
            parsed_batches_.push(std::move(*batch));
            batch = free_batches_.pop();
        }
    }

    if (batch && !batch->empty()) {
        parsed_batches_.push(std::move(*batch));
    }
    parsed_batches_.close();
}
