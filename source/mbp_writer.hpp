#include <fstream>
#include "types.hpp"

class MBPWriter final {
    private:
        std::ofstream file_;
    public:
        MBPWriter(const std::string& filename);
        ~MBPWriter();

        void write_snapshot(const MBP10Snapshot& snapshot);
};