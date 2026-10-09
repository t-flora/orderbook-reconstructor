#include "mbo_parser.hpp"
#include "mbp_writer.hpp"
#include "order_book.hpp"
#include "types.hpp"
#include <cstring>
#include <iostream>
#include <fstream>
#include <string_view>
#include <vector>

// input is read in large chunks, and each line is passed to the callback as a view into the chunk buffer
constexpr size_t INPUT_BUFFER_SIZE = 1 << 20;

template <typename Callback>
void for_each_line(std::istream& input, Callback&& callback) {
    std::vector<char> buffer(INPUT_BUFFER_SIZE);
    size_t filled = 0;
    bool end_of_input = false;

    while (!end_of_input) {
        input.read(buffer.data() + filled, static_cast<std::streamsize>(buffer.size() - filled));
        filled += static_cast<size_t>(input.gcount());
        end_of_input = !input;

        size_t start = 0;
        while (start < filled) {
            const char* data = buffer.data();
            const void* newline = std::memchr(data + start, '\n', filled - start);
            if (newline == nullptr && !end_of_input) {
                break; // incomplete line, read more input first
            }
            size_t end = newline ? static_cast<size_t>(static_cast<const char*>(newline) - data) : filled;

            std::string_view line(data + start, end - start);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            callback(line);
            start = end + 1;
        }

        // keep the incomplete line at the front of the buffer, growing it if a line doesn't fit
        size_t remaining = start < filled ? filled - start : 0;
        std::memmove(buffer.data(), buffer.data() + filled - remaining, remaining);
        filled = remaining;
        if (filled == buffer.size()) {
            buffer.resize(buffer.size() * 2);
        }
    }
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file> [output_file]" << std::endl;
        return 1;
    }

    const std::string input_file = argv[1];
    const std::string output_file = argc == 3 ? argv[2] : "data/actual_output.csv";
    if (argc == 2) {
        std::cout << "No output file provided, using default: " << output_file << std::endl;
    }

    std::ifstream input_file_stream(input_file, std::ios::binary);
    if (!input_file_stream.is_open()) {
        std::cerr << "Error: Could not open input file: " << input_file << std::endl;
        return 1;
    }

    StreamingMBOParser mbo_parser;
    OrderBook orderbook;
    MBPWriter mbp_writer(output_file);

    // Skip header line
    bool first_line = true;

    for_each_line(input_file_stream, [&](std::string_view line) {
        if (first_line) {
            first_line = false;
            return;
        }

        // Create MBOMessage with proper initialization
        MBOMessage message{
            Action::Add, Side::None, 0, 0, 0, Flag{static_cast<uint8_t>(0)},
            "", "", "", "", "", "", ""
        };

        if (mbo_parser.parse_MBO_line(line, message)) {
            // Apply message to orderbook and get processing result
            auto result = orderbook.process_message(message);

            // Write to MBP only for events within the written depth
            if (result.should_write_to_mbp) {
                mbp_writer.write_snapshot(result.message_for_mbp, orderbook.get_snapshot(), result.depth);
            }
        }
    });

    return 0;
}