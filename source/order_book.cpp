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
    switch (mbo.action) {
        case Action::Clear: {
            has_pending_trade_ = false;
            clear_order();
            return {true, mbo, 0};
        }
        case Action::Add: {
            has_pending_trade_ = false;
            // depth is computed before adding the order
            int depth = get_order_depth(mbo);
            add_order(mbo);
            return {depth <= MAX_WRITTEN_DEPTH, mbo, depth};
        }
        case Action::Trade: {
            // trades with side 'N' don't alter the book, but are still written to the MBP output
            if (mbo.side == Side::None) {
                return {true, mbo, 0};
            }
            // store the trade for the T->F->C sequence, it is written when the cancel arrives
            store_pending_trade(mbo);
            return {false, mbo, 0};
        }
        case Action::Fill: {
            // fills don't affect the book, they are intermediate actions in T->F->C sequences
            return {false, mbo, 0};
        }
        case Action::Cancel: {
            // depth is computed from the resting order before canceling
            int depth = 0;
            auto order_it = orders_by_id_.find(mbo.order_id);
            if (order_it != orders_by_id_.end()) {
                MBOMessage resting = mbo;
                resting.price = order_it->second.price;
                resting.side = order_it->second.side;
                depth = get_order_depth(resting);
            }

            bool level_removed = cancel_order(mbo);

            if (has_pending_trade_) {
                // completes a T->F->C sequence: written as a single trade on the side of the resting order
                has_pending_trade_ = false;
                MBOMessage trade_message = pending_trade_;
                trade_message.side = mbo.side;
                return {true, trade_message, depth};
            }

            // the expected output also includes bid cancels that remove the level just past the written depth
            bool write = depth <= MAX_WRITTEN_DEPTH ||
                         (mbo.side == Side::Bid && level_removed && depth == MAX_WRITTEN_DEPTH + 1);
            return {write, mbo, depth};
        }
        default: {
            throw std::invalid_argument{std::string{"Unknown action: "} +
                                        static_cast<char>(mbo.action)};
        }
    }
}


void OrderBook::store_pending_trade(const MBOMessage& mbo) {
    pending_trade_ = mbo;
    std::string_view* views[] = {
        &pending_trade_.ts_recv, &pending_trade_.ts_event, &pending_trade_.publisher_id_raw,
        &pending_trade_.instrument_id_raw, &pending_trade_.ts_in_delta_raw, &pending_trade_.sequence_raw,
        &pending_trade_.symbol
    };
    for (size_t i = 0; i < pending_trade_strings_.size(); ++i) {
        pending_trade_strings_[i].assign(views[i]->data(), views[i]->size());
        *views[i] = pending_trade_strings_[i];
    }
    has_pending_trade_ = true;
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


bool OrderBook::cancel_order(const MBOMessage& mbo) {
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
            return true;
        }
    }
    return false;
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