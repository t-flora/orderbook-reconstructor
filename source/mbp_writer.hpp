#include <fstream>
#include "types.hpp"

// max size of MBP output line, computed from MBP10Snapshot struct
constexpr size_t MAX_MBP_LINE_SIZE = 
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

constexpr size_t MIN_MBP_BUFFER_SIZE = buffer_utils::minimum_aligned_buffer_size(MAX_MBP_LINE_SIZE);


class MBPWriter final {
    private:
        std::ofstream file_;
        char output_buffer_[MIN_MBP_BUFFER_SIZE];

    public:
        MBPWriter(const std::string& filename);
        ~MBPWriter();

        void write_snapshot(const MBP10Snapshot& snapshot);
};