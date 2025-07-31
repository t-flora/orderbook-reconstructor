#pragma once
#include "types.hpp"
#include <map>

class OrderBook final {
    public:
        struct ProcessResult {
            int affected_level_index = -1;  // The depth/level index affected by the operation (within top 10)
            bool has_combined_trade = false;  // True if this result contains a combined T action from T/F/C sequence
            MBOMessage combined_trade_action;  // The combined T action to be written to MBP output
        };
        
    private:
        // Complete order book with automatic sorting
        // Bids: descending order (highest price first)
        std::map<Price, MBP10Snapshot::Level, std::greater<Price>> bid_levels_;
        // Asks: ascending order (lowest price first)  
        std::map<Price, MBP10Snapshot::Level> ask_levels_;
        
        // Top 10 views for efficient access (updated when get_view() is called)
        mutable std::array<MBP10Snapshot::Level,10> bid_levels_view_;
        mutable std::array<MBP10Snapshot::Level,10> ask_levels_view_;

        // T/F/C sequence tracking
        struct TFCSequence {
            MBOMessage trade_msg;
            MBOMessage fill_msg;
            bool has_trade = false;
            bool has_fill = false;
        };
        TFCSequence pending_tfc_;

        ProcessResult handle_add(const MBOMessage& message);
        ProcessResult handle_cancel(const MBOMessage& message);
        ProcessResult handle_clear(const MBOMessage& message);
        ProcessResult handle_trade(const MBOMessage& message);
        ProcessResult handle_fill(const MBOMessage& message);
        ProcessResult handle_tfc_cancel(const MBOMessage& message);
        bool is_tfc_sequence_match(const MBOMessage& msg1, const MBOMessage& msg2) const;
        void reset_tfc_sequence();
        int find_level_index_in_top10(const std::map<Price, MBP10Snapshot::Level, std::greater<Price>>& levels, Price price) const;
        int find_level_index_in_top10(const std::map<Price, MBP10Snapshot::Level>& levels, Price price) const;
        void update_view_arrays() const;
        
        // Methods for detecting top 10 changes
        struct Top10State {
            std::array<MBP10Snapshot::Level, 10> bid_levels;
            std::array<MBP10Snapshot::Level, 10> ask_levels;
            bool operator==(const Top10State& other) const;
        };
        Top10State capture_top10_state() const;

    public:
        OrderBook();
        ~OrderBook();
        ProcessResult process_MBO_msg(const MBOMessage& message);
        // MBP10Snapshot get_snapshot() const;
        OrderBookView get_view() const;
};