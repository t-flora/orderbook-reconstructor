#pragma once
#include <string>
#include <string_view>

// read-only memory map of a whole file
// parsed messages hold views into it, so they stay valid for as long as the file is mapped
class MappedFile final {
    private:
        const char* data_{nullptr};
        size_t size_{0};

    public:
        explicit MappedFile(const std::string& filename);
        ~MappedFile();

        MappedFile(const MappedFile&) = delete;
        MappedFile& operator=(const MappedFile&) = delete;

        std::string_view contents() const { return {data_, size_}; }
};
