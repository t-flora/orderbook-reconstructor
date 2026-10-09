#include "mbp_writer.hpp"
#include <charconv>
#include <cstring>
#include <iomanip>

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
}

MBPWriter::MBPWriter(const std::string& filename)
    : file_(filename, std::ios::binary), buffer_(MBP_OUTPUT_BUFFER_SIZE), buffer_used_(0), row_index_(0) {
    if (!file_.is_open()) {
        throw std::runtime_error("Could not open output file: " + filename);
    }

    // Write header
    write_header();
}

MBPWriter::~MBPWriter() {
    flush();
    if (file_.is_open()) {
        file_.close();
    }
}

void MBPWriter::flush() {
    file_.write(buffer_.data(), static_cast<std::streamsize>(buffer_used_));
    buffer_used_ = 0;
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

void MBPWriter::write_snapshot(const MBOMessage& message, const MBPSnapshot& snapshot, int depth) {
    // fields copied as-is have unbounded length, so they're added to the fixed-width bound
    const size_t max_line_size = MAX_MBP_LINE_SIZE + 2 * message.ts_event.size() + message.publisher_id_raw.size() +
                                 message.instrument_id_raw.size() + message.ts_in_delta_raw.size() +
                                 message.sequence_raw.size() + message.symbol.size();
    if (buffer_used_ + max_line_size > buffer_.size()) {
        flush();
        if (max_line_size > buffer_.size()) {
            buffer_.resize(max_line_size);
        }
    }
    char* out = buffer_.data() + buffer_used_;

    // Write row index (unnamed first column)
    append_int(out, row_index_++);

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

    buffer_used_ = static_cast<size_t>(out - buffer_.data());
}
