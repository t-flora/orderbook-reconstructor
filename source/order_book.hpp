#pragma once
#include "types.hpp"
#include <map>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

class OrderBook {
    public:
        // events at depth 0-10 (the 10 MBP levels, plus the level just past them) are written to the MBP output
        static constexpr int MAX_WRITTEN_DEPTH = 10;


        OrderBook() = default;
        std::pair<Level, Level> get_BBO() const;
    
        Level get_bid_level(size_t idx = 0) const;
        Level get_ask_level(size_t idx = 0) const;
    
        Level get_bid_level_by_px(Price px) const;
        Level get_ask_level_by_px(Price px) const;
    
        // compact resting order, so levels don't store full MBO messages
        struct RestingOrder {
            OrderId order_id;
            Size size;
            bool is_tob;
        };

        const RestingOrder& get_order(OrderId order_id);
        uint32_t get_queue_pos(OrderId order_id);
    
        MBPSnapshot get_snapshot() const;
        int get_order_depth(const MBOMessage& mbo) const;

        struct ProcessResult {
            bool should_write_to_mbp;
            MBOMessage message_for_mbp;
            int depth;
        };
        
        ProcessResult process_message(const MBOMessage& mbo);
   
    private:
        using LevelOrders = std::vector<RestingOrder>;
        // size and count are kept up to date as orders change, so snapshots don't iterate over orders
        struct PriceLevel {
            Size size{0};
            Count count{0};
            LevelOrders orders;
        };
        struct PriceAndSide {
            Price price;
            Side side;
        };
        using Orders = std::unordered_map<OrderId, PriceAndSide>;
        using SideLevels = std::map<Price, PriceLevel>;
    
        static Level get_price_level(Price price, const PriceLevel& level);
        static LevelOrders::iterator get_level_order(LevelOrders& level, OrderId order_id);
    
        void clear_order();
        void add_order(const MBOMessage& mbo);
        // returns true if the cancel removed the order's price level
        bool cancel_order(const MBOMessage& mbo);
    
        SideLevels& get_side_levels(Side side);
        PriceLevel& get_level(Side side, Price price);
        PriceLevel& get_or_insert_level(Side side, Price price);
    
        void remove_level(Side side, Price price);
    
        Orders orders_by_id_;
        SideLevels offers_;
        SideLevels bids_;
        
        // T->F->C sequence tracking
        MBOMessage pending_trade_{Action::Add, Side::None, 0, 0, 0, Flag{static_cast<uint8_t>(0)}, "", "", "", "", "", "", ""};
        bool has_pending_trade_{false};
        // owned copies of the pending trade's string fields, since the views point into the parser's line buffer
        std::array<std::string, 7> pending_trade_strings_;
        void store_pending_trade(const MBOMessage& mbo);
   };