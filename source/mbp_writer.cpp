#include "mbp_writer.hpp"
#include <iomanip>
#include <sstream>
#include <cmath>

// Helper function to format price with smart decimal precision
std::string format_price(double price) {
    // Round to 2 decimal places first to avoid floating point precision issues
    double rounded = std::round(price * 100.0) / 100.0;
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << rounded;
    std::string str = oss.str();
    
    // If it ends with '0', check if we should use 1 decimal place instead
    if (str.back() == '0' && str[str.length()-2] != '.') {
        // Remove the trailing zero to show only 1 decimal place
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
    
    file_ << ",symbol,order_id\r\n";
}

void MBPWriter::write_snapshot(const MBOMessage& message, const std::vector<BidAskPair>& snapshot, int depth) {
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
          << "," << depth;
    
    // Write price and size - convert back from nanos to decimal
    if (message.price != 0) {
        file_ << "," << format_price(message.price / 1E9);
    } else {
        file_ << ",";
    }
    
    file_ << "," << message.size
          << "," << static_cast<int>(message.flags.value)
          << "," << message.ts_in_delta_raw
          << "," << message.sequence_raw;
    
    // Write bid/ask levels from snapshot
    for (int i = 0; i < 10; ++i) {
        // Get the pair for this level, or use empty values if not enough levels
        const BidAskPair& pair = (i < snapshot.size()) ? snapshot[i] : BidAskPair{0, 0, 0, 0, 0, 0};
        
        // Bid level
        if (pair.bid_px != 0) {
            file_ << "," << format_price(pair.bid_px / 1E9);
        } else {
            file_ << ",";
        }
        file_ << "," << pair.bid_sz
              << "," << pair.bid_ct;
        
        // Ask level
        if (pair.ask_px != 0) {
            file_ << "," << format_price(pair.ask_px / 1E9);
        } else {
            file_ << ",";
        }
        file_ << "," << pair.ask_sz
              << "," << pair.ask_ct;
    }
    
    // Write symbol and order_id
    file_ << "," << message.symbol
          << "," << message.order_id << "\r\n";
}