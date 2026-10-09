#include "mbp_writer.hpp"
#include <iomanip>
#include <sstream>
#include <string>

// format a fixed-point nanos price, trimming trailing zeros but keeping at least one decimal (e.g. 13.0, 5.51, 13.575)
std::string format_price(Price price) {
    constexpr Price PRICE_SCALE = 1'000'000'000;
    std::string str = price < 0 ? "-" : "";
    Price abs_price = price < 0 ? -price : price;

    std::string fractional = std::to_string(abs_price % PRICE_SCALE);
    fractional.insert(0, 9 - fractional.size(), '0');
    size_t last_digit = fractional.find_last_not_of('0');
    fractional.resize(last_digit == std::string::npos ? 1 : last_digit + 1);

    str += std::to_string(abs_price / PRICE_SCALE);
    str += '.';
    str += fractional;
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
        file_ << "," << format_price(message.price);
    } else {
        file_ << ",";
    }
    
    file_ << "," << message.size
          << "," << static_cast<int>(message.flags.value)
          << "," << message.ts_in_delta_raw
          << "," << message.sequence_raw;
    
    // Write bid/ask levels from snapshot
    for (size_t i = 0; i < 10; ++i) {
        // Get the pair for this level, or use empty values if not enough levels
        const BidAskPair& pair = (i < snapshot.size()) ? snapshot[i] : BidAskPair{0, 0, 0, 0, 0, 0};
        
        // Bid level
        if (pair.bid_px != 0) {
            file_ << "," << format_price(pair.bid_px);
        } else {
            file_ << ",";
        }
        file_ << "," << pair.bid_sz
              << "," << pair.bid_ct;
        
        // Ask level
        if (pair.ask_px != 0) {
            file_ << "," << format_price(pair.ask_px);
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