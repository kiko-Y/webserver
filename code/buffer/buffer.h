#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <vector>
#include <string>
#include <string_view>
#include <atomic>
#include <unistd.h>
#include<sys/uio.h>

class Buffer {
public:
    Buffer(int initBufSize = 1024);
    ~Buffer() = default;

    std::size_t writableBytes() const;
    std::size_t readableBytes() const;
    std::size_t prependableBytes() const;

    const char* peek() const;
    void ensureWritable(size_t len);

    void retrieve(size_t len);
    void retrieveUntil(const char* end);

    void retrieveAll();

    std::string retrieveAllToString();

    const char* beginWrite() const;
    char* beginWrite();

    void append(const std::string& str);
    void append(const char* str, size_t len);
    void append(const void* data, size_t len);
    void append(const Buffer& buff);

    ssize_t readFd(int fd, int* errno_);
    ssize_t writefd(int fd, int* errno_);

    void hasWritten(size_t len);


private:
    char* beginPtr_();
    const char* beginPtr_() const;
    void makeSpace_(size_t len);

    std::vector<char> buffer_;
    std::atomic<std::size_t> readPos_;
    std::atomic<std::size_t> writePos_;
};

#endif
