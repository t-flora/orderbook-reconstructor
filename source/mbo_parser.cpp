#include "mbo_parser.hpp"

StreamingMBOParser::StreamingMBOParser() {
    fields_.fill(std::string_view());
}

StreamingMBOParser::~StreamingMBOParser() {
}

bool StreamingMBOParser::parse_MBO_line(const std::string_view line, MBOMessage& message) {
    const char* position = line.data();
    const char* end = position + line.size();

    // parse the timestamp
    const char* ts_recv_end = position + 20;
    message.ts_recv = std::string_view(position, ts_recv_end - position);
    position = ts_recv_end;

    // parse the event timestamp
}