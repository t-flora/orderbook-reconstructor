#include "mbo_parser.hpp"
#include "mbp_writer.hpp"
#include "order_book.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file> [output_file]" << std::endl;
        return 1;
    }
    if (argc == 2) {
        std::cout << "No output file provided, using default: data/output.mbp" << std::endl;
    }

    const std::string input_file = argv[1];
    const std::string output_file = argc == 3 ? argv[2] : "data/output.mbp";

    std::ifstream input_file_stream(input_file);
    if (!input_file_stream.is_open()) {
        std::cerr << "Error: Could not open input file: " << input_file << std::endl;
        return 1;
    }

    StreamingMBOParser mbo_parser(input_file_stream);
    OrderBook order_book;
    MBPWriter mbp_writer(output_file);

    std::string line;
    // ignore first line with column headers
    bool first_line = true;
    
    while (std::getline(input_file_stream, line)) {
        // Skip header line
        if (first_line) {
            first_line = false;
            continue;
        }
        
        MBOMessage message;
        if (mbo_parser.parse_MBO_line(line, message)) {
            order_book.process_MBO_msg(message);
            auto view = order_book.get_view();
            mbp_writer.write_snapshot(message, view);
        }
    }

    return 0;
}