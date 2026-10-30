#include "mbp_writer.hpp"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <iomanip>
#include <map>

namespace {
    // append helpers write directly into the output buffer and advance the cursor, avoiding stream formatting overhead
    inline void append_char(char*& out, char c) {
        *out++ = c;
    }

    inline void append_view(char*& out, std::string_view view) {
        std::memcpy(out, view.data(), view.size());
        out += view.size();
    }

    template <typename Int>
    inline void append_int(char*& out, Int value) {
        out = std::to_chars(out, out + 20, value).ptr;
    }

    // format a fixed-point nanos price, trimming trailing zeros but keeping at least one decimal (e.g. 13.0, 5.51, 13.575)
    inline void append_price(char*& out, Price price) {
        constexpr Price PRICE_SCALE = 1'000'000'000;
        constexpr int PRICE_DECIMALS = 9;
        if (price < 0) {
            append_char(out, '-');
            price = -price;
        }
        append_int(out, price / PRICE_SCALE);
        append_char(out, '.');

        Price fractional = price % PRICE_SCALE;
        if (fractional == 0) {
            append_char(out, '0');
            return;
        }
        int digits = PRICE_DECIMALS;
        while (fractional % 10 == 0) {
            fractional /= 10;
            --digits;
        }
        // write the remaining digits right to left, zero-padded to their width
        for (int i = digits - 1; i >= 0; --i) {
            out[i] = static_cast<char>('0' + fractional % 10);
            fractional /= 10;
        }
        out += digits;
    }

    // empty price fields are written as blank, matching the expected output
    inline void append_optional_price(char*& out, Price price) {
        append_char(out, ',');
        if (price != 0) {
            append_price(out, price);
        }
    }

    // fields copied as-is have unbounded length, so they're added to the fixed-width bound
    size_t max_row_size(const MBOMessage& message) {
        return MAX_MBP_LINE_SIZE + 2 * message.ts_event.size() + message.publisher_id_raw.size() +
               message.instrument_id_raw.size() + message.ts_in_delta_raw.size() +
               message.sequence_raw.size() + message.symbol.size();
    }

    // formats the row at out, returning the end of the formatted row
    char* format_row(char* out, size_t row_index, const MBPRow& row) {
        const MBOMessage& message = row.message;
        const MBPSnapshot& snapshot = row.snapshot;
        const int depth = row.depth;

        // Write row index (unnamed first column)
        append_int(out, row_index);

        // Write basic fields from message
        append_char(out, ',');
        append_view(out, message.ts_event); // Use ts_event for both timestamps per expected output
        append_char(out, ',');
        append_view(out, message.ts_event);
        append_view(out, ",10,"); // rtype is always 10 for MBP
        append_view(out, message.publisher_id_raw);
        append_char(out, ',');
        append_view(out, message.instrument_id_raw);
        append_char(out, ',');
        append_char(out, static_cast<char>(message.action));
        append_char(out, ',');
        append_char(out, static_cast<char>(message.side));
        append_char(out, ',');
        append_int(out, depth);

        append_optional_price(out, message.price);
        append_char(out, ',');
        append_int(out, message.size);
        append_char(out, ',');
        append_int(out, static_cast<unsigned>(message.flags.value));
        append_char(out, ',');
        append_view(out, message.ts_in_delta_raw);
        append_char(out, ',');
        append_view(out, message.sequence_raw);

        // Write bid/ask levels from snapshot
        for (const BidAskPair& pair : snapshot) {
            append_optional_price(out, pair.bid_px);
            append_char(out, ',');
            append_int(out, pair.bid_sz);
            append_char(out, ',');
            append_int(out, pair.bid_ct);

            append_optional_price(out, pair.ask_px);
            append_char(out, ',');
            append_int(out, pair.ask_sz);
            append_char(out, ',');
            append_int(out, pair.ask_ct);
        }

        // Write symbol and order_id
        append_char(out, ',');
        append_view(out, message.symbol);
        append_char(out, ',');
        append_int(out, message.order_id);
        append_view(out, "\r\n");

        return out;
    }
}

MBPWriter::MBPWriter(const std::string& filename, size_t format_threads) : file_(filename, std::ios::binary) {
    if (!file_.is_open()) {
        throw std::runtime_error("Could not open output file: " + filename);
    }

    // Write header
    write_header();

    // enough batches to keep every format thread busy while others wait to be written or are being filled
    const size_t batch_count = 2 * format_threads + 2;
    for (size_t i = 0; i < batch_count; ++i) {
        MBPBatch batch;
        batch.rows.reserve(MBP_ROWS_PER_BATCH);
        batch.output.resize(MBP_BATCH_OUTPUT_SIZE);
        free_batches_.push(std::move(batch));
    }
    for (size_t i = 0; i < format_threads; ++i) {
        formatters_.emplace_back(&MBPWriter::format_batches, this);
    }
    file_writer_ = std::thread(&MBPWriter::write_batches, this);
}

MBPWriter::~MBPWriter() {
    close();
}

MBPBatch MBPWriter::acquire_batch() {
    return std::move(*free_batches_.pop());
}

void MBPWriter::submit(MBPBatch batch) {
    batch.sequence = next_sequence_++;
    batch.first_row = next_row_;
    next_row_ += batch.rows.size();
    unformatted_batches_.push(std::move(batch));
}

void MBPWriter::close() {
    if (closed_) {
        return;
    }
    closed_ = true;
    unformatted_batches_.close();
    for (std::thread& formatter : formatters_) {
        formatter.join();
    }
    formatted_batches_.close();
    file_writer_.join();
    file_.close();
}

void MBPWriter::format_batches() {
    while (std::optional<MBPBatch> batch = unformatted_batches_.pop()) {
        size_t used = 0;
        for (size_t i = 0; i < batch->rows.size(); ++i) {
            const MBPRow& row = batch->rows[i];
            const size_t max_size = max_row_size(row.message);
            if (used + max_size > batch->output.size()) {
                batch->output.resize(std::max(2 * batch->output.size(), used + max_size));
            }
            char* start = batch->output.data();
            used = static_cast<size_t>(format_row(start + used, batch->first_row + i, row) - start);
        }
        batch->output_size = used;
        formatted_batches_.push(std::move(*batch));
    }
}

void MBPWriter::write_batches() {
    // format threads can finish out of order, so batches wait here until every earlier batch is written
    std::map<size_t, MBPBatch> waiting;
    size_t next_to_write = 0;
    while (std::optional<MBPBatch> batch = formatted_batches_.pop()) {
        const size_t sequence = batch->sequence;
        waiting.emplace(sequence, std::move(*batch));
        for (auto it = waiting.begin(); it != waiting.end() && it->first == next_to_write; it = waiting.begin()) {
            file_.write(it->second.output.data(), static_cast<std::streamsize>(it->second.output_size));
            it->second.rows.clear();
            free_batches_.push(std::move(it->second));
            waiting.erase(it);
            ++next_to_write;
        }
    }
}

void MBPWriter::write_header() {
    file_ << ",ts_recv,ts_event,rtype,publisher_id,instrument_id,action,side,depth,price,size,flags,ts_in_delta,sequence";

    // Write bid/ask level headers
    for (size_t i = 0; i < MBP_LEVELS; ++i) {
        file_ << ",bid_px_" << std::setw(2) << std::setfill('0') << i
              << ",bid_sz_" << std::setw(2) << std::setfill('0') << i
              << ",bid_ct_" << std::setw(2) << std::setfill('0') << i
              << ",ask_px_" << std::setw(2) << std::setfill('0') << i
              << ",ask_sz_" << std::setw(2) << std::setfill('0') << i
              << ",ask_ct_" << std::setw(2) << std::setfill('0') << i;
    }

    file_ << ",symbol,order_id\r\n";
}
