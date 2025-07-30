#include "types.hpp"

class OrderBook final {
    private:
        std::array<MBP10Snapshot::Level,10> bid_levels_;
        std::array<MBP10Snapshot::Level,10> ask_levels_;

    public:
        OrderBook();
        ~OrderBook();
        void process_MBO_message(const MBOMessage& message);
        MBP10Snapshot get_snapshot() const;
};