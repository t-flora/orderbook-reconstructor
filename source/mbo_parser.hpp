#pragma once
#include <array>
#include <iostream>
#include <fstream>
#include <string_view>
#include "types.hpp"

constexpr size_t MAX_MBO_LINE_SIZE = 
    1 + // 1 char for action
    1 + // 1 char for side
    20 + // 20 max bytes for price (int64_t)
    10 + // 10 max chars for size (uint32_t)
    20 + // 20 max chars for order ID (uint64_t)
    30 + // 30 bytes for each timestamp
    30 +
    5 + // 5 max chars for publisher ID (uint16_t)
    10 + // 10 max chars for instrument ID (uint32_t)
    3 + // 3 max chars for flags (uint8_t)
    11 + // 11 max chars for ts_in_delta (int32_t)
    10 + // 10 max chars for sequence (uint32_t)
    3; // 3 chars for symbol

constexpr size_t MIN_MBO_BUFFER_SIZE = buffer_utils::minimum_aligned_buffer_size(MAX_MBO_LINE_SIZE);

class StreamingMBOParser final {
    private:
        std::array<std::string_view, 15> fields_;
        char input_buffer_[MIN_MBO_BUFFER_SIZE];

    public:
        StreamingMBOParser(std::ifstream& file);
        ~StreamingMBOParser();

        // return bool so we can iterate over the file until we reach the end
        bool parse_MBO_line(const std::string_view line, MBOMessage& message);

        // minimum version discarding constant fields in sample dataset
        // bool min_parse_MBO_line(const std::string_view line, MBOMessage& message);

        // will split the line into 16 string views, which are used to populate the message
        int split_line_into_views(const std::string_view line, std::array<std::string_view, 15>& views);

        // will parse the price from the string view
        Price parse_price(const std::string_view price_str);
};