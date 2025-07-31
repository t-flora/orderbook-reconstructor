#include "order_book.hpp"
#include <algorithm>
#include <map>

OrderBook::OrderBook() {
    // Initialize all levels to zero
    for (auto& level : bid_levels_) {
        level = {0, 0, 0};
    }
    for (auto& level : ask_levels_) {
        level = {0, 0, 0};
    }
}

OrderBook::~OrderBook() {
}

void OrderBook::process_MBO_msg(const MBOMessage& message) {
    // Ignore Clear actions as specified in requirements
    if (message.action == Action::Clear) {
        return;
    }
    
    // For any actions with side 'N', don't alter orderbook  
    if (message.side == Side::None) {
        return;
    }
    
    // Handle T/F/C sequences - for now, process each action individually
    // TODO: Implement proper T/F/C sequence detection and combination
    
    switch (message.action) {
        case Action::Add:
            handle_add(message);
            break;
        case Action::Cancel:
            handle_cancel(message);
            break;
        case Action::Clear:
            handle_clear(message);
            break;
        default:
            break;
    }
}

void OrderBook::handle_add(const MBOMessage& message) {
    auto& levels = (message.side == Side::Bid) ? bid_levels_ : ask_levels_;
    bool is_bid = (message.side == Side::Bid);
    
    // Find existing level or insert new one
    for (int i = 0; i < 10; ++i) {
        if (levels[i].price == 0) {
            // Empty level, insert here
            levels[i] = {message.price, message.size, 1};
            sort_levels(levels, is_bid);
            return;
        } else if (levels[i].price == message.price) {
            // Same price level, aggregate
            levels[i].size += message.size;
            levels[i].count += 1;
            return;
        }
    }
    
    // If we reach here, all 10 levels are filled
    // Check if this price is better than the worst level
    Price worst_price = levels[9].price;
    bool should_insert = is_bid ? (message.price > worst_price) : (message.price < worst_price);
    
    if (should_insert) {
        levels[9] = {message.price, message.size, 1};
        sort_levels(levels, is_bid);
    }
}

void OrderBook::handle_cancel(const MBOMessage& message) {
    auto& levels = (message.side == Side::Bid) ? bid_levels_ : ask_levels_;
    
    // Find the level with matching price and reduce size
    for (int i = 0; i < 10; ++i) {
        if (levels[i].price == message.price) {
            if (levels[i].size >= message.size) {
                levels[i].size -= message.size;
                if (levels[i].count > 0) {
                    levels[i].count -= 1;
                }
                if (levels[i].size == 0) {
                    // Remove this level
                    for (int j = i; j < 9; ++j) {
                        levels[j] = levels[j + 1];
                    }
                    levels[9] = {0, 0, 0};
                }
            }
            break;
        }
    }
}

void OrderBook::handle_clear(const MBOMessage& /*message*/) {
    // Reset the entire order book (both bid and ask levels)
    for (auto& level : bid_levels_) {
        level = {0, 0, 0};
    }
    for (auto& level : ask_levels_) {
        level = {0, 0, 0};
    }
}

void OrderBook::sort_levels(std::array<MBP10Snapshot::Level,10>& levels, bool is_bid) {
    // Sort levels: bids descending (highest first), asks ascending (lowest first)
    if (is_bid) {
        std::sort(levels.begin(), levels.end(), [](const auto& a, const auto& b) {
            if (a.price == 0) return false; // Empty levels go to end
            if (b.price == 0) return true;
            return a.price > b.price; // Descending for bids
        });
    } else {
        std::sort(levels.begin(), levels.end(), [](const auto& a, const auto& b) {
            if (a.price == 0) return false; // Empty levels go to end
            if (b.price == 0) return true;
            return a.price < b.price; // Ascending for asks
        });
    }
}

OrderBookView OrderBook::get_view() const {
    return {bid_levels_, ask_levels_};
}