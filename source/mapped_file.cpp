#include "mapped_file.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

MappedFile::MappedFile(const std::string& filename) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Could not open input file: " + filename + " (" + std::strerror(errno) + ")");
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        int error = errno;
        ::close(fd);
        throw std::runtime_error("Could not read input file size: " + filename + " (" + std::strerror(error) + ")");
    }

    size_ = static_cast<size_t>(st.st_size);
    // an empty file can't be mapped, and is just an empty view
    if (size_ > 0) {
        void* data = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) {
            int error = errno;
            ::close(fd);
            throw std::runtime_error("Could not map input file: " + filename + " (" + std::strerror(error) + ")");
        }
        // the file is read front to back, so the kernel can read ahead aggressively
        madvise(data, size_, MADV_SEQUENTIAL);
        data_ = static_cast<const char*>(data);
    }
    // the mapping stays valid after the descriptor is closed
    ::close(fd);
}

MappedFile::~MappedFile() {
    if (data_ != nullptr) {
        munmap(const_cast<char*>(data_), size_);
    }
}
