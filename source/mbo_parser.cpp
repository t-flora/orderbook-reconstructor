#include "mbo_parser.hpp"
#include <cstring>

StreamingMBOParser::StreamingMBOParser(std::ifstream& file) {
    fields_.fill(std::string_view());
}

StreamingMBOParser::~StreamingMBOParser() {
}

bool StreamingMBOParser::parse_MBO_line(const std::string_view line, MBOMessage& message) {
    if (split_line_into_views(line, fields_) != 0) {
        return false;
    }
    
    // Parse fields from CSV: ts_recv,ts_event,rtype,publisher_id,instrument_id,action,side,price,size,channel_id,order_id,flags,ts_in_delta,sequence,symbol
    message.ts_recv = fields_[0];
    message.ts_event = fields_[1];
    // Skip rtype (fields_[2]) - constant
    message.publisher_id_raw = fields_[3];
    message.instrument_id_raw = fields_[4];
    
    // Parse action
    if (!fields_[5].empty()) {
        message.action = static_cast<Action>(fields_[5][0]);
    }
    
    // Parse side
    if (!fields_[6].empty()) {
        message.side = static_cast<Side>(fields_[6][0]);
    }
    
    // Parse price
    message.price = parse_price(fields_[7]);
    
    // Parse size
    if (!fields_[8].empty()) {
        message.size = static_cast<Size>(std::stoul(std::string(fields_[8])));
    }
    
    // Skip channel_id (fields_[9]) - constant
    
    // Parse order_id
    if (!fields_[10].empty()) {
        message.order_id = static_cast<OrderId>(std::stoull(std::string(fields_[10])));
    }
    
    message.flags_raw = fields_[11];
    message.ts_in_delta_raw = fields_[12];
    message.sequence_raw = fields_[13];
    message.symbol = fields_[14];
    
    return true;
}

int StreamingMBOParser::split_line_into_views(const std::string_view line, std::array<std::string_view, 15>& views) {
    size_t field_idx = 0;
    size_t start = 0;
    
    for (size_t i = 0; i < line.size() && field_idx < 15; ++i) {
        if (line[i] == ',') {
            views[field_idx] = line.substr(start, i - start);
            start = i + 1;
            field_idx++;
        }
    }
    
    // Handle last field (no trailing comma)
    if (field_idx < 15) {
        views[field_idx] = line.substr(start);
        field_idx++;
    }
    
    return field_idx == 15 ? 0 : -1; // Success if we found all 15 fields
}

Price StreamingMBOParser::parse_price(const std::string_view price_str) {
    if (price_str.empty()) {
        return 0;
    }
    
    // Convert price from double string to fixed-point integer (multiply by 1e9 for nano precision)
    std::string price_string(price_str);
    double price_double = std::stod(price_string);
    return static_cast<Price>(price_double * 1E9); // Convert to nanos
}