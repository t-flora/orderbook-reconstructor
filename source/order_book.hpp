#pragma once
#include "types.hpp"
#include <map>
#include <array>
#include <string>

using namespace std;

class OrderBook {
    public:
        std::pair<Level, Level> get_BBO() const;
    
        Level get_bid_level(size_t idx = 0) const;
        Level get_ask_level(size_t idx = 0) const;
    
        Level get_bid_level_by_px(Price px) const;
        Level get_ask_level_by_px(Price px) const;
    
        const MBOMessage& get_order(OrderId order_id);
        uint32_t get_queue_pos(OrderId order_id);
    
        std::vector<BidAskPair> get_snapshot(size_t level_count = 1) const;
    
        void apply_message(const MBOMessage& mbo);
   
    private:
        using LevelOrders = std::vector<MBOMessage>;
        struct PriceAndSide {
            Price price;
            Side side;
        };
        using Orders = std::unordered_map<OrderId, PriceAndSide>;
        using SideLevels = std::map<Price, LevelOrders>;
    
        static Level get_price_level(Price price, const LevelOrders& level);
        static LevelOrders::iterator get_level_order(LevelOrders& level, OrderId order_id);
    
        void clear_order();
        void add_order(MBOMessage mbo);
        void cancel_order(MBOMessage mbo);
    
        SideLevels& get_side_levels(Side side);
        LevelOrders& get_level(Side side, Price price);
        LevelOrders& get_or_insert_level(Side side, Price price);
    
        void remove_level(Side side, Price price);
    
        Orders orders_by_id_;
        SideLevels offers_;
        SideLevels bids_;
   };