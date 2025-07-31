#pragma once
#include "types.hpp"
#include <map>

class OrderBook final {
    private:
        // Complete order book with automatic sorting
        // Bids: descending order (highest price first)
        std::map<Price, MBP10Snapshot::Level, std::greater<Price>> bid_levels_;
        // Asks: ascending order (lowest price first)  
        std::map<Price, MBP10Snapshot::Level> ask_levels_;
        
        // Top 10 views for efficient access (updated when get_view() is called)
        mutable std::array<MBP10Snapshot::Level,10> bid_levels_view_;
        mutable std::array<MBP10Snapshot::Level,10> ask_levels_view_;

    public:
        OrderBook();
        ~OrderBook();
        void process_MBO_msg(const MBOMessage& message);
        // MBP10Snapshot get_snapshot() const;
        OrderBookView get_view() const;
};