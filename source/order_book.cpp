#include "order_book.hpp"

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
