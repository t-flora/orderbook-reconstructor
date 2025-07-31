#include "order_book.hpp"
#include <algorithm>

OrderBook::OrderBook() {
    // Initialize view arrays to zero
    for (auto& level : bid_levels_view_) {
        level = {0, 0, 0};
    }
    for (auto& level : ask_levels_view_) {
        level = {0, 0, 0};
    }
}

OrderBook::~OrderBook() {
}

OrderBook::ProcessResult OrderBook::process_MBO_msg(const MBOMessage& message) {
    // Handle Clear actions with side 'N' - these represent order book resets/initialization
    if (message.action == Action::Clear && message.side == Side::None) {
        // This is an order book reset/initialization - should generate a snapshot
        return ProcessResult{0, false, MBOMessage{}}; // Return valid index to indicate snapshot should be written
    }
    
    // Ignore other Clear actions as specified in requirements
    if (message.action == Action::Clear) {
        return ProcessResult{-1, false, MBOMessage{}};
    }
    
    // For any other actions with side 'N', don't alter orderbook  
    if (message.side == Side::None) {
        return ProcessResult{-1, false, MBOMessage{}};
    }
    
    // Handle T/F/C sequences
    switch (message.action) {
        case Action::Add:
            return handle_add(message);
        case Action::Cancel:
            // Check if this is part of a T/F/C sequence
            if (pending_tfc_.has_trade && pending_tfc_.has_fill) {
                if (is_tfc_sequence_match(pending_tfc_.fill_msg, message) && 
                    pending_tfc_.fill_msg.order_id == message.order_id) {
                    return handle_tfc_cancel(message);
                }
            }
            // If not part of T/F/C sequence, handle as regular cancel
            reset_tfc_sequence();
            return handle_cancel(message);
        case Action::Trade:
            return handle_trade(message);
        case Action::Fill:
            return handle_fill(message);
        case Action::Clear:
            return handle_clear(message);
        default:
            reset_tfc_sequence();
            return ProcessResult{-1, false, MBOMessage{}};
    }
}

OrderBook::ProcessResult OrderBook::handle_add(const MBOMessage& message) {
    // Check if this add could affect the top 10 before making changes
    bool could_affect_top10 = false;
    
    if (message.side == Side::Bid) {
        // For bids, check if this price is better than the 10th best bid (if it exists)
        if (bid_levels_.size() < 10) {
            could_affect_top10 = true; // Always affects top 10 if we have less than 10 levels
        } else {
            auto it = bid_levels_.begin();
            std::advance(it, 9); // 10th element (0-indexed)
            could_affect_top10 = (message.price >= it->first); // New price is better than or equal to 10th best
        }
        
        // Add to order book
        auto it = bid_levels_.find(message.price);
        if (it != bid_levels_.end()) {
            it->second.size += message.size;
            it->second.count += 1;
        } else {
            bid_levels_[message.price] = {message.price, message.size, 1};
        }
    } else {
        // For asks, check if this price is better than the 10th best ask (if it exists)
        if (ask_levels_.size() < 10) {
            could_affect_top10 = true; // Always affects top 10 if we have less than 10 levels
        } else {
            auto it = ask_levels_.begin();
            std::advance(it, 9); // 10th element (0-indexed)
            could_affect_top10 = (message.price <= it->first); // New price is better than or equal to 10th best
        }
        
        // Add to order book
        auto it = ask_levels_.find(message.price);
        if (it != ask_levels_.end()) {
            it->second.size += message.size;
            it->second.count += 1;
        } else {
            ask_levels_[message.price] = {message.price, message.size, 1};
        }
    }
    
    // If the operation could affect the top 10, do a more precise check
    if (could_affect_top10) {
        // Do a state comparison to see if anything actually changed
        Top10State state_after = capture_top10_state();
        Top10State state_before = capture_top10_state();
        // Temporarily undo the operation to capture the before state
        if (message.side == Side::Bid) {
            auto it = bid_levels_.find(message.price);
            if (it != bid_levels_.end()) {
                if (it->second.count == 1) {
                    // This was a new level, remove it
                    bid_levels_.erase(it);
                } else {
                    // Subtract the added amount
                    it->second.size -= message.size;
                    it->second.count -= 1;
                }
            }
        } else {
            auto it = ask_levels_.find(message.price);
            if (it != ask_levels_.end()) {
                if (it->second.count == 1) {
                    // This was a new level, remove it
                    ask_levels_.erase(it);
                } else {
                    // Subtract the added amount
                    it->second.size -= message.size;
                    it->second.count -= 1;
                }
            }
        }
        state_before = capture_top10_state();
        
        // Restore the operation
        if (message.side == Side::Bid) {
            auto it = bid_levels_.find(message.price);
            if (it != bid_levels_.end()) {
                it->second.size += message.size;
                it->second.count += 1;
            } else {
                bid_levels_[message.price] = {message.price, message.size, 1};
            }
        } else {
            auto it = ask_levels_.find(message.price);
            if (it != ask_levels_.end()) {
                it->second.size += message.size;
                it->second.count += 1;
            } else {
                ask_levels_[message.price] = {message.price, message.size, 1};
            }
        }
        
        return ProcessResult{(!(state_before == state_after)) ? 0 : -1, false, MBOMessage{}};
    } else {
        return ProcessResult{-1, false, MBOMessage{}};
    }
}

OrderBook::ProcessResult OrderBook::handle_cancel(const MBOMessage& message) {
    // Check if this price is currently in the top 10 before making changes
    bool affects_top10 = false;
    
    if (message.side == Side::Bid) {
        affects_top10 = (find_level_index_in_top10(bid_levels_, message.price) != -1);
        
        auto it = bid_levels_.find(message.price);
        if (it != bid_levels_.end()) {
            if (it->second.size >= message.size) {
                it->second.size -= message.size;
                if (it->second.count > 0) {
                    it->second.count -= 1;
                }
                if (it->second.size == 0) {
                    // Remove this level completely
                    bid_levels_.erase(it);
                    // If we removed a top 10 level, this definitely affects top 10
                    affects_top10 = true;
                }
            } else {
                // No change occurred (insufficient size)
                return ProcessResult{-1, false, MBOMessage{}};
            }
        } else {
            // Price not found - no change
            return ProcessResult{-1, false, MBOMessage{}};
        }
    } else {
        affects_top10 = (find_level_index_in_top10(ask_levels_, message.price) != -1);
        
        auto it = ask_levels_.find(message.price);
        if (it != ask_levels_.end()) {
            if (it->second.size >= message.size) {
                it->second.size -= message.size;
                if (it->second.count > 0) {
                    it->second.count -= 1;
                }
                if (it->second.size == 0) {
                    // Remove this level completely
                    ask_levels_.erase(it);
                    // If we removed a top 10 level, this definitely affects top 10
                    affects_top10 = true;
                }
            } else {
                // No change occurred (insufficient size)
                return ProcessResult{-1, false, MBOMessage{}};
            }
        } else {
            // Price not found - no change
            return ProcessResult{-1, false, MBOMessage{}};
        }
    }
    
    // Generate snapshot if this operation affects the top 10
    return ProcessResult{affects_top10 ? 0 : -1, false, MBOMessage{}};
}

OrderBook::ProcessResult OrderBook::handle_clear(const MBOMessage& /*message*/) {
    // Reset the entire order book (both bid and ask levels)
    bid_levels_.clear();
    ask_levels_.clear();
    return ProcessResult{0, false, MBOMessage{}}; // Clear affects level 0 (or we could use -1 to indicate "all levels")
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

void OrderBook::update_view_arrays() const {
    // Clear view arrays
    for (auto& level : bid_levels_view_) {
        level = {0, 0, 0};
    }
    for (auto& level : ask_levels_view_) {
        level = {0, 0, 0};
    }
    
    // Populate bid levels (top 10)
    int index = 0;
    for (const auto& [price, level] : bid_levels_) {
        if (index >= 10) break;
        bid_levels_view_[index] = level;
        ++index;
    }
    
    // Populate ask levels (top 10)
    index = 0;
    for (const auto& [price, level] : ask_levels_) {
        if (index >= 10) break;
        ask_levels_view_[index] = level;
        ++index;
    }
}

OrderBookView OrderBook::get_view() const {
    update_view_arrays();
    return {bid_levels_view_, ask_levels_view_};
}

OrderBook::ProcessResult OrderBook::handle_trade(const MBOMessage& message) {
    // Start a new T/F/C sequence
    reset_tfc_sequence();
    pending_tfc_.trade_msg = message;
    pending_tfc_.has_trade = true;
    
    // T actions don't affect the orderbook immediately
    return ProcessResult{-1, false, MBOMessage{}};
}

OrderBook::ProcessResult OrderBook::handle_fill(const MBOMessage& message) {
    // Check if this Fill matches the pending Trade
    if (pending_tfc_.has_trade && is_tfc_sequence_match(pending_tfc_.trade_msg, message)) {
        pending_tfc_.fill_msg = message;
        pending_tfc_.has_fill = true;
        
        // F actions don't affect the orderbook
        return ProcessResult{-1, false, MBOMessage{}};
    }
    
    // If no matching Trade, reset and ignore this Fill
    reset_tfc_sequence();
    return ProcessResult{-1, false, MBOMessage{}};
}

OrderBook::ProcessResult OrderBook::handle_tfc_cancel(const MBOMessage& message) {
    // This is the C action that completes a T/F/C sequence
    // Process the Cancel action normally to affect the orderbook
    auto result = handle_cancel(message);
    
    // Create the combined Trade action for MBP output
    // The Trade should be on the opposite side from where it appears in raw data
    MBOMessage combined_trade = pending_tfc_.trade_msg;
    
    // Switch the side to opposite (trade consumes liquidity on the opposite side)
    if (pending_tfc_.trade_msg.side == Side::Bid) {
        combined_trade.side = Side::Ask;
    } else if (pending_tfc_.trade_msg.side == Side::Ask) {
        combined_trade.side = Side::Bid;
    }
    
    result.has_combined_trade = true;
    result.combined_trade_action = combined_trade;
    
    reset_tfc_sequence();
    return result;
}

bool OrderBook::is_tfc_sequence_match(const MBOMessage& msg1, const MBOMessage& msg2) const {
    // Check if two messages are part of the same T/F/C sequence
    // They should have the same timestamp, price, and size
    return msg1.ts_recv == msg2.ts_recv &&
           msg1.ts_event == msg2.ts_event &&
           msg1.price == msg2.price &&
           msg1.size == msg2.size &&
           msg1.sequence_raw == msg2.sequence_raw;
}

void OrderBook::reset_tfc_sequence() {
    pending_tfc_.has_trade = false;
    pending_tfc_.has_fill = false;
}

OrderBook::Top10State OrderBook::capture_top10_state() const {
    Top10State state;
    
    // Initialize with zeros
    for (auto& level : state.bid_levels) {
        level = {0, 0, 0};
    }
    for (auto& level : state.ask_levels) {
        level = {0, 0, 0};
    }
    
    // Populate bid levels (top 10, highest price first)
    int index = 0;
    for (const auto& [price, level] : bid_levels_) {
        if (index >= 10) break;
        state.bid_levels[index] = level;
        ++index;
    }
    
    // Populate ask levels (top 10, lowest price first)
    index = 0;
    for (const auto& [price, level] : ask_levels_) {
        if (index >= 10) break;
        state.ask_levels[index] = level;
        ++index;
    }
    
    return state;
}

bool OrderBook::Top10State::operator==(const Top10State& other) const {
    return bid_levels == other.bid_levels && ask_levels == other.ask_levels;
}