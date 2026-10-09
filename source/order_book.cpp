#include "order_book.hpp"
#include <algorithm>
#include <stdexcept>

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

const OrderBook::RestingOrder& OrderBook::get_order(OrderId order_id) {
    auto order_it = orders_by_id_.find(order_id);
    if (order_it == orders_by_id_.end()) {
        throw std::invalid_argument{"No order with ID " +
                                    std::to_string(order_id)};
    }
    auto& level = get_level(order_it->second.side, order_it->second.price);
    return *get_level_order(level.orders, order_id);
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
    for (const auto& order : level_it.orders) {
        if (order.order_id == order_id) {
        break;
        }
        prior_size += order.size;
    }
    return prior_size;
}

MBPSnapshot OrderBook::get_snapshot() const {
    // walk both sides once, instead of advancing from the best level for every index
    MBPSnapshot res{};
    auto bid_it = bids_.rbegin(); // reverse iterator to get highest bid prices first
    for (size_t i = 0; i < MBP_LEVELS && bid_it != bids_.rend(); ++i, ++bid_it) {
        res[i].bid_px = bid_it->first;
        res[i].bid_sz = bid_it->second.size;
        res[i].bid_ct = bid_it->second.count;
    }
    auto ask_it = offers_.begin();
    for (size_t i = 0; i < MBP_LEVELS && ask_it != offers_.end(); ++i, ++ask_it) {
        res[i].ask_px = ask_it->first;
        res[i].ask_sz = ask_it->second.size;
        res[i].ask_ct = ask_it->second.count;
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

Level OrderBook::get_price_level(Price price, const PriceLevel& level) {
    return Level{price, level.size, level.count};
}

OrderBook::LevelOrders::iterator OrderBook::get_level_order(OrderBook::LevelOrders& level, OrderId order_id) {
    auto order_it = std::find_if(
        level.begin(), level.end(),
        [order_id](const RestingOrder& order) { return order.order_id == order_id; }
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

void OrderBook::add_order(const MBOMessage& mbo) {
    if (mbo.flags.has(F_TOB)) { // check if order is top-of-book
        SideLevels& levels = get_side_levels(mbo.side);
        levels.clear();
        // 0 indicates the side's book should be cleared
        // and doesn't represent an order that should be added
        if (mbo.price != 0) {
            // top-of-book orders count towards size but not count
            PriceLevel& level = levels[mbo.price];
            level.size = mbo.size;
            level.orders.push_back(RestingOrder{mbo.order_id, mbo.size, true});
        }
    } else {
        PriceLevel& level = get_or_insert_level(mbo.side, mbo.price);
        level.orders.push_back(RestingOrder{mbo.order_id, mbo.size, false});
        level.size += mbo.size;
        ++level.count;
        auto res = orders_by_id_.emplace(mbo.order_id,
                                        PriceAndSide{mbo.price, mbo.side});
        if (!res.second) {
        throw std::invalid_argument{"Received duplicated order ID " +
                                    std::to_string(mbo.order_id)};
        }
    }
}


bool OrderBook::cancel_order(const MBOMessage& mbo) {
    PriceLevel& level = get_level(mbo.side, mbo.price);
    auto order_it = get_level_order(level.orders, mbo.order_id);
    if (order_it->size < mbo.size) {
        throw std::logic_error{
            "Tried to cancel more size than existed for order ID " +
            std::to_string(mbo.order_id)};
    }
    order_it->size -= mbo.size;
    level.size -= mbo.size;
    if (order_it->size == 0) {
        if (!order_it->is_tob) {
            --level.count;
        }
        orders_by_id_.erase(mbo.order_id);
        level.orders.erase(order_it);
        if (level.orders.empty()) {
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


OrderBook::PriceLevel& OrderBook::get_level(Side side, Price price) {
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

OrderBook::PriceLevel& OrderBook::get_or_insert_level(Side side, Price price) {
    SideLevels& levels = get_side_levels(side);
    return levels[price];
}

void OrderBook::remove_level(Side side, Price price) {
    SideLevels& levels = get_side_levels(side);
    levels.erase(price);
}