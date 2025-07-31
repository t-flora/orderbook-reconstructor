// Types for the MBO message parser / MBP writer
#pragma once
#include <cstdint>
#include <string_view>
#include <array>

enum class Action { Add = 'A', Cancel = 'C', Clear = 'R' };
enum class Side { Bid = 'B', Ask = 'A', None = 'N' };

using Price = int64_t;
using Size = uint32_t;
using Count = uint32_t;
using OrderId = uint64_t;

using Depth = uint8_t;

// structs for plain old data
// data that will just be passed to MBP writer without modification will be stored as string views to avoid conversions
struct MBOMessage {
    //// fields that are constant and _NOT_ passed to MBP writer - i.e. ignored ////
    // uint8_t rtype; // rtype is the same for every MBO message and MBP output row
    // uint8_t channel_id; // is constant in sample and not passed to MBP writer/used for order book construction

    //// fields that are passed to MBP writer and used for order book construction ////
    Action action;
    Side side;
    Price price;
    Size size;
    OrderId order_id;

    ////// string views to avoid heap allocations, piped to MBP writer //////
    //// fields that are passed to MBP writer ////
    // uint64_t ts_recv; // passed to MBP writer
    // uint64_t ts_event; // passed to MBP writer
    // uint8_t flags; // 8-bit integer, passed to MBP writer
    // int32_t ts_in_delta; // passed to MBP writer
    // uint32_t sequence; // passed to MBP writer

    //// fields that are constant in input/output samples, passed to MBP writer for flexibility ////
    // uint16_t publisher_id; // publisher ID is fixed for input/output
    // uint32_t instrument_id; // as is instrument ID...
    // char[4] symbol; // and symbol is the same for the whole dataset, but we'll _copy_ as-is to MBP writer for flexibility

    std::string_view ts_recv; // fixed-length string view to avoid heap allocations, piped to MBP writer
    std::string_view ts_event; // piped to MBP writer
    std::string_view publisher_id_raw; // copy as-is to MBP writer
    std::string_view instrument_id_raw; // copy as-is to MBP writer
    std::string_view flags_raw; // copy as-is to MBP writer
    std::string_view ts_in_delta_raw; // copy as-is to MBP writer
    std::string_view sequence_raw; // copy as-is to MBP writer
    std::string_view symbol; // copy as-is to MBP writer
};

struct MBP10Snapshot {
    std::string_view ts_recv;
    std::string_view ts_event;
    Action action;
    Side side;
    Depth depth; // 0-9, indicates level updated
    Price price;
    Size size;
    uint8_t flags;
    int32_t ts_in_delta;
    uint32_t sequence;
    struct Level { // Level struct allows for easy iteration over levels
        Price price;
        Size size;
        Count count;
    };
    std::array<Level,10> bid_levels;
    std::array<Level,10> ask_levels;
};

namespace buffer_utils {
    constexpr size_t next_power_of_2(size_t n) {
        if (n <= 1) return 1;
        
        size_t power = 1;
        while (power < n) { // find the highest set bit position
            power <<= 1;
        }
        return power;
    }

    constexpr size_t minimum_aligned_buffer_size(size_t max_field_size, double safety_margin = 1.5) {
        size_t buffer_size = max_field_size * safety_margin;
        size_t aligned_buffer_size = next_power_of_2(buffer_size);
        constexpr size_t MIN_ALIGNMENT_SIZE = 64;
        return aligned_buffer_size < MIN_ALIGNMENT_SIZE ? MIN_ALIGNMENT_SIZE : aligned_buffer_size;
    }
}