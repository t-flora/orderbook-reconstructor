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

OrderBook::ProcessResult OrderBook::process_MBO_msg(const MBOMessage& message) {
    // Handle Clear actions with side 'N' - these represent order book resets/initialization
    if (message.action == Action::Clear && message.side == Side::None) {
        // This is an order book reset/initialization - should generate a snapshot
        return ProcessResult{0}; // Return valid index to indicate snapshot should be written
    }
    
    // Ignore other Clear actions as specified in requirements
    if (message.action == Action::Clear) {
        return ProcessResult{};
    }
    
    // For any other actions with side 'N', don't alter orderbook  
    if (message.side == Side::None) {
        return ProcessResult{};
    }
    
    // Handle T/F/C sequences - for now, process each action individually
    // TODO: Implement proper T/F/C sequence detection and combination
    
    switch (message.action) {
        case Action::Add:
            return handle_add(message);
        case Action::Cancel:
            return handle_cancel(message);
        case Action::Clear:
            return handle_clear(message);
        default:
            return ProcessResult{};
    }
}

OrderBook::ProcessResult OrderBook::handle_add(const MBOMessage& message) {
    auto& levels = (message.side == Side::Bid) ? bid_levels_ : ask_levels_;
    bool is_bid = (message.side == Side::Bid);
    
    // Find existing level or insert new one
    for (int i = 0; i < 10; ++i) {
        if (levels[i].price == 0) {
            // Empty level, insert here
            levels[i] = {message.price, message.size, 1};
            sort_levels(levels, is_bid);
            // Find the new position after sorting
            for (int j = 0; j < 10; ++j) {
                if (levels[j].price == message.price) {
                    return ProcessResult{j};
                }
            }
            return ProcessResult{i};
        } else if (levels[i].price == message.price) {
            // Same price level, aggregate
            levels[i].size += message.size;
            levels[i].count += 1;
            return ProcessResult{i};
        }
    }
    
    // If we reach here, all 10 levels are filled
    // Check if this price is better than the worst level
    Price worst_price = levels[9].price;
    bool should_insert = is_bid ? (message.price > worst_price) : (message.price < worst_price);
    
    if (should_insert) {
        levels[9] = {message.price, message.size, 1};
        sort_levels(levels, is_bid);
        // Find the new position after sorting
        for (int i = 0; i < 10; ++i) {
            if (levels[i].price == message.price) {
                return ProcessResult{i};
            }
        }
    }
    
    // Order placed outside top 10 - still generate snapshot showing current top 10
    // TODO: Implement full order book with dynamic allocation
    return ProcessResult{10}; // Indicate order placed outside visible range
}

OrderBook::ProcessResult OrderBook::handle_cancel(const MBOMessage& message) {
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
                return ProcessResult{i}; // Return the level index where cancellation succeeded
            } else {
                // Insufficient size to cancel - no change occurred
                return ProcessResult{-1};
            }
        }
    }
    
    return ProcessResult{-1}; // Price not found
}

OrderBook::ProcessResult OrderBook::handle_clear(const MBOMessage& /*message*/) {
    // Reset the entire order book (both bid and ask levels)
    for (auto& level : bid_levels_) {
        level = {0, 0, 0};
    }
    for (auto& level : ask_levels_) {
        level = {0, 0, 0};
    }
    return ProcessResult{0}; // Clear affects level 0 (or we could use -1 to indicate "all levels")
}

int OrderBook::find_level_index_in_top10(const std::map<Price, MBP10Snapshot::Level, std::greater<Price>>& levels, Price price) const {
    int index = 0;
    for (const auto& [level_price, level] : levels) {
        if (level_price == price) {
            return index;
        }
        if (++index >= 10) {
            break; // Only check top 10
        }
    }
    return -1; // Not in top 10
}

int OrderBook::find_level_index_in_top10(const std::map<Price, MBP10Snapshot::Level>& levels, Price price) const {
    int index = 0;
    for (const auto& [level_price, level] : levels) {
        if (level_price == price) {
            return index;
        }
        if (++index >= 10) {
            break; // Only check top 10
        }
    }
    return -1; // Not in top 10
}

OrderBookView OrderBook::get_view() const {
    return {bid_levels_, ask_levels_};
}