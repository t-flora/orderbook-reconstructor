// Types for the MBO message parser / MBP writer
#include <cstdint>
#include <string>
#include <array>

enum class Action { Add = 'A', Cancel = 'C', Clear = 'R'};
enum class Side { Bid = 'B', Ask = 'A', None = 'N'};

using Price = int64_t;
using Size = uint32_t;
using Count = uint32_t;
using OrderId = uint64_t;

using Depth = uint8_t;

// structs for plain old data
struct MBOMessage {
    std::string ts_recv; // fixed-length string, piped to MBP writer
    std::string ts_event; // fixed-length string, piped to MBP writer
    // uint8_t rtype; // rtype is the same for every MBO message and MBP output row
    // uint16_t publisher_id; // so is publisher ID fixed for input/output
    // uint32_t instrument_id; // and instrument ID...
    Action action;
    Side side;
    Price price;
    Size size;
    OrderId order_id;
    uint8_t flags; // 8-bit integer, piped to MBP writer
    int32_t ts_in_delta; // piped to MBP writer
    uint32_t sequence; // piped to MBP writer
    // char symbol[4]; // symbol is the same for the whole dataset
};

struct MBP10Snapshot {
    std::string ts_recv;
    std::string ts_event;
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