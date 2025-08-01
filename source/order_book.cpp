#include "order_book.hpp"

// Constructor implementation removed - using default constructor

std::pair<Level, Level> OrderBook::get_BBO() const {
    return {get_bid_level(), get_ask_level()};
}

Level OrderBook::get_bid_level(size_t idx) const {
    if (bids_.size() > idx) {
        auto level_it = bids_.rbegin(); // reverse iterator to get highest bid prices first
        std::advance(level_it, idx);
        return get_price_level(level_it->first, level_it->second);
    }
    return Level{};
}

Level OrderBook::get_ask_level(size_t idx) const {
    if (offers_.size() > idx) {
        auto level_it = offers_.begin();
        std::advance(level_it, idx);
        return get_price_level(level_it->first, level_it->second);
    }
    return Level{};
}

Level OrderBook::get_bid_level_by_px(Price px) const {
    auto level_it = bids_.find(px);
    if (level_it == bids_.end()) {
        throw std::invalid_argument{"No bid level at " + std::to_string(px)};
    }
    return get_price_level(px, level_it->second);
}

Level OrderBook::get_ask_level_by_px(Price px) const {
    auto level_it = offers_.find(px);
    if (level_it == offers_.end()) {
        throw std::invalid_argument{"No ask level at " + std::to_string(px)};
    }
    return get_price_level(px, level_it->second);
}

const MBOMessage& OrderBook::get_order(OrderId order_id) {
    auto order_it = orders_by_id_.find(order_id);
    if (order_it == orders_by_id_.end()) {
        throw std::invalid_argument{"No order with ID " +
                                    std::to_string(order_id)};
    }
    auto& level = get_level(order_it->second.side, order_it->second.price);
    return *get_level_order(level, order_id);
}

uint32_t OrderBook::get_queue_pos(OrderId order_id) {
    auto order_it = orders_by_id_.find(order_id);
    if (order_it == orders_by_id_.end()) {
        throw std::invalid_argument{"No order with ID " +
                                    std::to_string(order_id)};
    }
    const auto& level_it =
        get_level(order_it->second.side, order_it->second.price);
    uint32_t prior_size = 0;
    for (const auto& order : level_it) {
        if (order.order_id == order_id) {
        break;
        }
        prior_size += order.size;
    }
    return prior_size;
}

std::vector<BidAskPair> OrderBook::get_snapshot(std::size_t level_count) const {
    std::vector<BidAskPair> res;
    for (size_t i = 0; i < level_count; ++i) {
        BidAskPair ba_pair{0, 0, 0, 0, 0, 0};
        auto bid = get_bid_level(i);
        if (bid) {
            ba_pair.bid_px = bid.price;
            ba_pair.bid_sz = bid.size;
            ba_pair.bid_ct = bid.count;
        }
        auto ask = get_ask_level(i);
        if (ask) {
            ba_pair.ask_px = ask.price;
            ba_pair.ask_sz = ask.size;
            ba_pair.ask_ct = ask.count;
        }
        res.emplace_back(ba_pair);
    }
    return res;
}

bool OrderBook::top12_levels_changed() {
    auto current_top12 = get_snapshot(12);
    
    // First call - always consider changed to write initial state
    if (previous_top12_.empty()) {
        previous_top12_ = current_top12;
        return true;
    }
    
    // Compare with previous snapshot
    bool changed = false;
    if (previous_top12_.size() != current_top12.size()) {
        changed = true;
    } else {
        for (size_t i = 0; i < previous_top12_.size(); ++i) {
            const auto& prev = previous_top12_[i];
            const auto& curr = current_top12[i];
            if (prev.bid_px != curr.bid_px || prev.bid_sz != curr.bid_sz || prev.bid_ct != curr.bid_ct ||
                prev.ask_px != curr.ask_px || prev.ask_sz != curr.ask_sz || prev.ask_ct != curr.ask_ct) {
                changed = true;
                break;
            }
        }
    }
    
    // Update previous snapshot if changed
    if (changed) {
        previous_top12_ = current_top12;
    }
    
    return changed;
}

int OrderBook::get_order_depth(const MBOMessage& mbo) const {
    const SideLevels& levels = (mbo.side == Side::Bid) ? bids_ : offers_;
    
    int depth = 0;
    if (mbo.side == Side::Bid) {
        // For bids, count how many levels have better (higher) prices
        for (auto it = levels.rbegin(); it != levels.rend(); ++it) {
            if (it->first > mbo.price) {
                depth++;
            } else {
                break; // Found our price level or worse
            }
        }
    } else if (mbo.side == Side::Ask) {
        // For asks, count how many levels have better (lower) prices  
        for (auto it = levels.begin(); it != levels.end(); ++it) {
            if (it->first < mbo.price) {
                depth++;
            } else {
                break; // Found our price level or worse
            }
        }
    }
    
    return depth;
}

OrderBook::ProcessResult OrderBook::process_message(const MBOMessage& mbo) {
    // Handle T->F->C sequences
    if (mbo.action == Action::Trade) {
        // If side is 'N', ignore as per task requirements
        if (mbo.side == Side::None) {
            return {false, MBOMessage{Action::Add, Side::None, 0, 0, 0, Flag{static_cast<uint8_t>(0)}, "", "", "", "", "", "", ""}, 0};
        }
        // Store the trade for potential T->F->C sequence
        pending_trade_ = mbo;
        has_pending_trade_ = true;
        return {false, MBOMessage{Action::Add, Side::None, 0, 0, 0, Flag{static_cast<uint8_t>(0)}, "", "", "", "", "", "", ""}, 0};
    }
    
    if (mbo.action == Action::Fill && has_pending_trade_) {
        // Skip the Fill action, continue looking for Cancel
        return {false, MBOMessage{Action::Add, Side::None, 0, 0, 0, Flag{static_cast<uint8_t>(0)}, "", "", "", "", "", "", ""}, 0};
    }
    
    if (mbo.action == Action::Cancel && has_pending_trade_) {
        // This completes a T->F->C sequence
        // Check if the order exists in the book first to get its depth before canceling
        auto order_it = orders_by_id_.find(mbo.order_id);
        int depth_before_cancel = 0;
        if (order_it != orders_by_id_.end()) {
            // Create a temporary message with the order's actual price and side for depth calculation
            MBOMessage temp_mbo = mbo;
            temp_mbo.price = order_it->second.price;
            temp_mbo.side = order_it->second.side;
            depth_before_cancel = get_order_depth(temp_mbo);
        }
        
        // Apply the cancel (which reduces the existing order on the book)
        cancel_order(mbo);
        
        // Create a trade message for the MBP output
        MBOMessage trade_message = pending_trade_;
        trade_message.action = Action::Trade;
        // The cancel shows which side is being reduced, so use that side
        trade_message.side = mbo.side;
        trade_message.price = mbo.price;
        trade_message.size = mbo.size;
        trade_message.order_id = mbo.order_id;
        
        has_pending_trade_ = false;
        return {top12_levels_changed(), trade_message, depth_before_cancel};
    }
    
    // Reset pending trade if we encounter other actions
    if (has_pending_trade_ && mbo.action != Action::Fill && mbo.action != Action::Cancel) {
        has_pending_trade_ = false;
    }
    
    // Handle regular messages
    switch (mbo.action) {
        case Action::Clear: {
            clear_order();
            // Clear changes order book state - write to MBP if top 10 levels changed
            return {top12_levels_changed(), mbo, get_order_depth(mbo)};
        }
        case Action::Add: {
            // Calculate depth before adding the order
            int depth_before_add = get_order_depth(mbo);
            add_order(mbo);
            
            // Only trigger MBP update if the ADD affects top 10 levels (depth 0-9)
            if (depth_before_add < 10) {
                return {top12_levels_changed(), mbo, depth_before_add};
            } else {
                // ADD at depth 10+ doesn't affect top 10 levels shown in MBP
                return {false, MBOMessage{Action::Add, Side::None, 0, 0, 0, Flag{static_cast<uint8_t>(0)}, "", "", "", "", "", "", ""}, 0};
            }
        }
        case Action::Cancel: {
            // Check if the order exists in the book first to get its depth before canceling
            auto order_it = orders_by_id_.find(mbo.order_id);
            int depth_before_cancel = 0;
            if (order_it != orders_by_id_.end()) {
                // Create a temporary message with the order's actual price and side for depth calculation
                MBOMessage temp_mbo = mbo;
                temp_mbo.price = order_it->second.price;
                temp_mbo.side = order_it->second.side;
                depth_before_cancel = get_order_depth(temp_mbo);
            }
            
            cancel_order(mbo);
            // Cancel changes order book state - write to MBP if top 10 levels changed
            return {top12_levels_changed(), mbo, depth_before_cancel};
        }
        case Action::Trade: {
            // Trade actions do NOT affect the orderbook per task requirements
            // Only the Cancel in T->F->C sequences affects the orderbook
            // Do not write to MBP for standalone trades
            return {false, MBOMessage{Action::Add, Side::None, 0, 0, 0, Flag{static_cast<uint8_t>(0)}, "", "", "", "", "", "", ""}, 0};
        }
        case Action::Fill: {
            // Fill actions don't directly affect the orderbook
            // They are intermediate actions in T->F->C sequences
            // Do not write to MBP for fills
            return {false, MBOMessage{Action::Add, Side::None, 0, 0, 0, Flag{static_cast<uint8_t>(0)}, "", "", "", "", "", "", ""}, 0};
        }
        default: {
            throw std::invalid_argument{std::string{"Unknown action: "} +
                                        static_cast<char>(mbo.action)};
        }
    }
}


Level OrderBook::get_price_level(Price price, const LevelOrders& level) {
    Level res{price, 0, 0};
    for (const auto& order : level) {
        if (!order.flags.has(F_TOB)) { // if order is not top-of-book
            ++res.count;
        }
        res.size += order.size;
    }
    return res;
}

OrderBook::LevelOrders::iterator OrderBook::get_level_order(OrderBook::LevelOrders& level, OrderId order_id) {
    auto order_it = std::find_if(
        level.begin(), level.end(),
        [order_id](const MBOMessage& order) { return order.order_id == order_id; }
    );
    if (order_it == level.end()) {
        throw std::invalid_argument{"No order with ID " +
                                    std::to_string(order_id)};
    }
    return order_it;
}

void OrderBook::clear_order() {
    orders_by_id_.clear();
    offers_.clear();
    bids_.clear();
}

void OrderBook::add_order(MBOMessage mbo) {
    if (mbo.flags.has(F_TOB)) { // check if order is top-of-book
        SideLevels& levels = get_side_levels(mbo.side);
        levels.clear();
        // 0 indicates the side's book should be cleared
        // and doesn't represent an order that should be added
        if (mbo.price != 0) {
            LevelOrders level = {mbo};
            levels.emplace(mbo.price, level);
        }
    } else {
        LevelOrders& level = get_or_insert_level(mbo.side, mbo.price);
        level.emplace_back(mbo);
        auto res = orders_by_id_.emplace(mbo.order_id,
                                        PriceAndSide{mbo.price, mbo.side});
        if (!res.second) {
        throw std::invalid_argument{"Received duplicated order ID " +
                                    std::to_string(mbo.order_id)};
        }
    }
}


void OrderBook::cancel_order(MBOMessage mbo) {
    LevelOrders& level = get_level(mbo.side, mbo.price);
    auto order_it = get_level_order(level, mbo.order_id);
    if (order_it->size < mbo.size) {
        throw std::logic_error{
            "Tried to cancel more size than existed for order ID " +
            std::to_string(mbo.order_id)};
    }
    order_it->size -= mbo.size;
    if (order_it->size == 0) {
        orders_by_id_.erase(mbo.order_id);
        level.erase(order_it);
        if (level.empty()) {
        remove_level(mbo.side, mbo.price);
        }
    }
}

OrderBook::SideLevels& OrderBook::get_side_levels(Side side) {
    switch (side) {
        case Side::Ask: {
        return offers_;
        }
        case Side::Bid: {
        return bids_;
        }
        case Side::None:
        default: {
        throw std::invalid_argument{"Invalid side"};
        }
    }
}


OrderBook::LevelOrders& OrderBook::get_level(Side side, Price price) {
    SideLevels& levels = get_side_levels(side);
    auto level_it = levels.find(price);
    if (level_it == levels.end()) {
        throw std::invalid_argument{
            std::string{"Received event for unknown level "} +
            static_cast<char>(side) +
            " " +
            std::to_string(price)};
    }
    return level_it->second;
}

OrderBook::LevelOrders& OrderBook::get_or_insert_level(Side side, Price price) {
    SideLevels& levels = get_side_levels(side);
    return levels[price];
}

void OrderBook::remove_level(Side side, Price price) {
    SideLevels& levels = get_side_levels(side);
    levels.erase(price);
}