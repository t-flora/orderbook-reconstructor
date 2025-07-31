#include "mbp_writer.hpp"
#include <iomanip>
#include <sstream>

// Helper function to format price without trailing zeros
std::string format_price(double price) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(9) << price;
    std::string str = oss.str();
    
    // Remove trailing zeros
    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
    // Remove trailing decimal point if no fractional part
    if (str.back() == '.') {
        str.pop_back();
    }
    
    return str;
}

MBPWriter::MBPWriter(const std::string& filename) : file_(filename), row_index_(0) {
    if (!file_.is_open()) {
        throw std::runtime_error("Could not open output file: " + filename);
    }
    
    // Write header
    write_header();
}

MBPWriter::~MBPWriter() {
    if (file_.is_open()) {
        file_.close();
    }
}

void MBPWriter::write_header() {
    file_ << ",ts_recv,ts_event,rtype,publisher_id,instrument_id,action,side,depth,price,size,flags,ts_in_delta,sequence";
    
    // Write bid/ask level headers
    for (int i = 0; i < 10; ++i) {
        file_ << ",bid_px_" << std::setw(2) << std::setfill('0') << i
              << ",bid_sz_" << std::setw(2) << std::setfill('0') << i
              << ",bid_ct_" << std::setw(2) << std::setfill('0') << i
              << ",ask_px_" << std::setw(2) << std::setfill('0') << i
              << ",ask_sz_" << std::setw(2) << std::setfill('0') << i
              << ",ask_ct_" << std::setw(2) << std::setfill('0') << i;
    }
    
    file_ << ",symbol,order_id\n";
}

void MBPWriter::write_snapshot(const MBOMessage& message, const OrderBookView& view, int depth) {
    // Write row index (unnamed first column)
    file_ << row_index_++;
    
    // Write basic fields from message  
    file_ << "," << message.ts_event  // Use ts_event for both timestamps per expected output
          << "," << message.ts_event
          << ",10" // rtype is always 10 for MBP
          << "," << message.publisher_id_raw
          << "," << message.instrument_id_raw
          << "," << static_cast<char>(message.action)
          << "," << static_cast<char>(message.side)
          << "," << (depth >= 0 ? depth : 0);
    
    // Write price and size - convert back from nanos to decimal
    if (message.price != 0) {
        file_ << "," << format_price(message.price / 1E9);
    } else {
        file_ << ",";
    }
    
    file_ << "," << message.size
          << "," << message.flags_raw
          << "," << message.ts_in_delta_raw
          << "," << message.sequence_raw;
    
    // Write bid/ask levels from order book view
    for (int i = 0; i < 10; ++i) {
        // Bid level
        if (view.bid_levels_[i].price != 0) {
            file_ << "," << format_price(view.bid_levels_[i].price / 1E9);
        } else {
            file_ << ",";
        }
        file_ << "," << view.bid_levels_[i].size
              << "," << view.bid_levels_[i].count;
        
        // Ask level
        if (view.ask_levels_[i].price != 0) {
            file_ << "," << format_price(view.ask_levels_[i].price / 1E9);
        } else {
            file_ << ",";
        }
        file_ << "," << view.ask_levels_[i].size
              << "," << view.ask_levels_[i].count;
    }
    
    // Write symbol and order_id
    file_ << "," << message.symbol
          << "," << message.order_id << "\n";
}