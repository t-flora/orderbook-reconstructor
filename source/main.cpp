#include "mbo_parser.hpp"
#include "mbp_writer.hpp"
#include "order_book.hpp"
#include "types.hpp"
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
    OrderBook orderbook;
    MBPWriter mbp_writer(output_file);

    std::string line;
    
    // Skip header line  
    bool first_line = true;
    
    while (std::getline(input_file_stream, line)) {
        if (first_line) {
            first_line = false;
            continue;
        }
        
        // Create MBOMessage with proper initialization
        MBOMessage message{
            Action::Add, Side::None, 0, 0, 0, Flag{static_cast<uint8_t>(0)},
            "", "", "", "", "", "", ""
        };
        
        if (mbo_parser.parse_MBO_line(line, message)) {
            // Apply message to orderbook and get processing result
            auto result = orderbook.process_message(message);
            
            // Write to MBP only if top 10 levels changed
            if (result.should_write_to_mbp) {
                // Calculate depth AFTER applying changes
                int depth = orderbook.get_order_depth(result.message_for_mbp);
                mbp_writer.write_snapshot(result.message_for_mbp, orderbook.get_snapshot(10), depth);
            }
        }
    }

    return 0;
}