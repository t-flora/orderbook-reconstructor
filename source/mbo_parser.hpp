#pragma once
#include <array>
#include <iostream>
#include <fstream>
#include <string_view>
#include "types.hpp"

class StreamingMBOParser final {
    private:
        std::array<std::string_view, 16> fields_;

    public:
        StreamingMBOParser();
        ~StreamingMBOParser();

        // return bool so we can iterate over the file until we reach the end
        bool parse_MBO_line(const std::string_view line, MBOMessage& message);

        // minimum version discarding constant fields in sample dataset
        // bool min_parse_MBO_line(const std::string_view line, MBOMessage& message);

        // will split the line into 16 string views, which are used to populate the message
        int split_line_into_views(const std::string_view line, std::array<std::string_view, 16>& views);

        // will parse the price from the string view
        Price parse_price(const std::string_view price_str);
};