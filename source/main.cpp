#include "mbo_parser.hpp"
#include "mbp_writer.hpp"
#include "order_book.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_file>" << std::endl;
        return 1;
    }
    if (argc == 2) {
        std::cout << "No output file provided, using default: data/output.mbp" << std::endl;
    }

    const std::string input_file = argv[1];
    const std::string output_file = argc == 3 ? argv[2] : "data/output.mbp";

    StreamingMBOParser mbo_parser(input_file);
    OrderBook order_book;
    MBPWriter mbp_writer(output_file);

    std::string line;
    while (std::getline(std::cin, line)) {
        MBOMessage message;
        mbo_parser.parse_MBO_line(line, message);
        order_book.process_MBO_message(message);
        mbp_writer.write_snapshot(order_book.get_snapshot());
    }

    return 0;
}