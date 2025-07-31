#pragma once
#include "types.hpp"

class OrderBook final {
    private:
        std::array<MBP10Snapshot::Level,10> bid_levels_;
        std::array<MBP10Snapshot::Level,10> ask_levels_;

        void handle_add(const MBOMessage& message);
        void handle_cancel(const MBOMessage& message);
        void handle_clear(const MBOMessage& message);
        void sort_levels(std::array<MBP10Snapshot::Level,10>& levels, bool is_bid);

    public:
        OrderBook();
        ~OrderBook();
        void process_MBO_msg(const MBOMessage& message);
        // MBP10Snapshot get_snapshot() const;
        OrderBookView get_view() const;
};