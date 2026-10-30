#include "mapped_file.hpp"
#include "mbo_reader.hpp"
#include "mbp_writer.hpp"
#include "order_book.hpp"
#include "types.hpp"
#include <iostream>
#include <optional>
#include <string>
#include <vector>

// the pipeline runs in stages on separate threads:
// parsing (MBOReader) -> order book updates and snapshots (this thread) -> formatting and file writes (MBPWriter)
constexpr size_t FORMAT_THREADS = 1;

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

    try {
        // declared first so it outlives the reader and writer, which hold views into it
        MappedFile input(input_file);
        MBOReader mbo_reader(input.contents());
        OrderBook orderbook;
        MBPWriter mbp_writer(output_file, FORMAT_THREADS);

        MBPBatch batch = mbp_writer.acquire_batch();
        while (std::optional<std::vector<MBOMessage>> messages = mbo_reader.next_batch()) {
            for (const MBOMessage& message : *messages) {
                // Apply message to orderbook and get processing result
                auto result = orderbook.process_message(message);

                // Write to MBP only for events within the written depth
                if (!result.should_write_to_mbp) {
                    continue;
                }
                batch.rows.push_back(MBPRow{result.message_for_mbp, orderbook.get_snapshot(), result.depth});
                if (batch.rows.size() == MBP_ROWS_PER_BATCH) {
                    mbp_writer.submit(std::move(batch));
                    batch = mbp_writer.acquire_batch();
                }
            }
            mbo_reader.recycle(std::move(*messages));
        }
        mbp_writer.submit(std::move(batch));
        mbp_writer.close();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
