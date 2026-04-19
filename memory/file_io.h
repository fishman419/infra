#ifndef INFRA_FILE_IO
#define INFRA_FILE_IO

#include <cstdint>
#include <cstring>
#include <string>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"

namespace infra {

// File I/O utility class supporting offset and length specifications
// Thread-safe if ThreadSafe template parameter is true
template <bool ThreadSafe = false>
class FileIO {
public:
    // Construct with file path
    explicit FileIO(const std::string& path)
        : fd_(-1), error_code_(0) {
        open(path);
    }

    // Construct with already-open file descriptor (takes ownership)
    explicit FileIO(int fd)
        : fd_(fd), error_code_(0) {}

    // Destructor - closes file if we own it
    ~FileIO() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    // Disable copy
    FileIO(const FileIO&) = delete;
    FileIO& operator=(const FileIO&) = delete;

    // Enable move
    FileIO(FileIO&& other) noexcept
        : fd_(other.fd_), error_code_(other.error_code_) {
        other.fd_ = -1;
        other.error_code_ = 0;
    }

    FileIO& operator=(FileIO&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                ::close(fd_);
            }
            fd_ = other.fd_;
            error_code_ = other.error_code_;
            other.fd_ = -1;
            other.error_code_ = 0;
        }
        return *this;
    }

    // Open file
    bool open(const std::string& path) {
        LockGuard guard(lock_);
        fd_ = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd_ < 0) {
            error_code_ = errno;
            return false;
        }
        return true;
    }

    // Close file
    void close() {
        LockGuard guard(lock_);
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    // Read from specified offset
    // Returns number of bytes read, or -1 on error
    ssize_t read_at(size_t offset, void* buf, size_t len) {
        LockGuard guard(lock_);
        ssize_t result = ::pread(fd_, buf, len, static_cast<off_t>(offset));
        if (result < 0) {
            error_code_ = errno;
        }
        return result;
    }

    // Write at specified offset
    // Returns number of bytes written, or -1 on error
    ssize_t write_at(size_t offset, const void* buf, size_t len) {
        LockGuard guard(lock_);
        ssize_t result = ::pwrite(fd_, buf, len, static_cast<off_t>(offset));
        if (result < 0) {
            error_code_ = errno;
        }
        return result;
    }

    // Read exactly len bytes from specified offset
    // Returns true if all bytes were read
    bool read_at_exact(size_t offset, void* buf, size_t len) {
        size_t total_read = 0;
        while (total_read < len) {
            ssize_t bytes = read_at(offset + total_read,
                                    static_cast<uint8_t*>(buf) + total_read,
                                    len - total_read);
            if (bytes <= 0) {
                return false;
            }
            total_read += static_cast<size_t>(bytes);
        }
        return true;
    }

    // Write exactly len bytes at specified offset
    // Returns true if all bytes were written
    bool write_at_exact(size_t offset, const void* buf, size_t len) {
        size_t total_written = 0;
        while (total_written < len) {
            ssize_t bytes = write_at(offset + total_written,
                                     static_cast<const uint8_t*>(buf) + total_written,
                                     len - total_written);
            if (bytes <= 0) {
                return false;
            }
            total_written += static_cast<size_t>(bytes);
        }
        return true;
    }

    // Read from current position
    ssize_t read(void* buf, size_t len) {
        LockGuard guard(lock_);
        ssize_t result = ::read(fd_, buf, len);
        if (result < 0) {
            error_code_ = errno;
        }
        return result;
    }

    // Write at current position
    ssize_t write(const void* buf, size_t len) {
        LockGuard guard(lock_);
        ssize_t result = ::write(fd_, buf, len);
        if (result < 0) {
            error_code_ = errno;
        }
        return result;
    }

    // Seek to offset
    off_t seek(off_t offset, int whence = SEEK_SET) {
        LockGuard guard(lock_);
        return ::lseek(fd_, offset, whence);
    }

    // Get current position
    off_t tell() const {
        return ::lseek(fd_, 0, SEEK_CUR);
    }

    // Get file size
    off_t size() const {
        LockGuard guard(lock_);
        off_t pos = ::lseek(fd_, 0, SEEK_CUR);
        off_t sz = ::lseek(fd_, 0, SEEK_END);
        ::lseek(fd_, pos, SEEK_SET);
        return sz;
    }

    // Sync file to disk
    bool sync() {
        LockGuard guard(lock_);
        return fsync(fd_) == 0;
    }

    // Check if file is open
    bool is_open() const {
        return fd_ >= 0;
    }

    // Get last error code
    int error_code() const {
        return error_code_;
    }

    // Get file descriptor
    int fd() const {
        return fd_;
    }

    // Truncate file to specified size
    bool truncate(size_t size) {
        LockGuard guard(lock_);
        return ftruncate(fd_, static_cast<off_t>(size)) == 0;
    }

private:
    using Lock = typename LockTypeSelector<ThreadSafe>::Lock;
    using LockGuard = typename LockTypeSelector<ThreadSafe>::LockGuard;

    mutable Lock lock_;
    int fd_;
    int error_code_;
};

// Typed wrapper for structured reads/writes
template <typename T, bool ThreadSafe = false>
class FileIOTyped {
public:
    explicit FileIOTyped(FileIO<ThreadSafe>& file_io) : file_io_(file_io) {}

    // Read a value at offset
    bool read_at(size_t offset, T* value) {
        return file_io_.read_at_exact(offset, value, sizeof(T));
    }

    // Write a value at offset
    bool write_at(size_t offset, const T& value) {
        return file_io_.write_at_exact(offset, &value, sizeof(T));
    }

private:
    FileIO<ThreadSafe>& file_io_;
};

// Common typedefs
template <bool ThreadSafe>
using FileIOPtr = FileIO<ThreadSafe>*;

}  // namespace infra

#endif  // INFRA_FILE_IO
