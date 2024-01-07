#include "buffer.h"
#include <assert.h>
#include <unistd.h>


Buffer::Buffer(int initBufSize): buffer_(initBufSize), readPos_(0), writePos_(0) {}

std::size_t Buffer::writableBytes() const {
    return buffer_.size() - writePos_;
}

std::size_t Buffer::readableBytes() const {
    return writePos_ - readPos_;
}

std::size_t Buffer::prependableBytes() const {
    return readPos_;
}

const char* Buffer::peek() const {
    return beginPtr_() + readPos_;
}

void Buffer::ensureWritable(size_t len) {
    if (len <= writableBytes()) return;
    makeSpace_(len);
    assert(writableBytes() >= len);
}

void Buffer::retrive(size_t len) {
    assert(len <= readableBytes());
    readPos_ += len;
}

void Buffer::retriveUntil(const char* end) {
    assert(peek() <= end);
    retrive(end - peek());
}

void Buffer::retriveAll() {
    fill(buffer_.begin(), buffer_.end(), '\0');
    readPos_ = 0;
    writePos_ = 0;
}

std::string Buffer::retriveAllToString() {
    std::string str(peek(), readableBytes());
    retriveAll();
    return str;
}

const char* Buffer::beginWrite() const {
    return beginPtr_() + writePos_;
}

char* Buffer::beginWrite() {
    return beginPtr_() + writePos_;
}

void Buffer::append(const std::string& str) {
    append(str.data(), str.length());
}

void Buffer::append(const char* str, size_t len) {
    assert(str);
    ensureWritable(len);
    std::copy(str, str + len, beginWrite());
    hasWrittern_(len);
}

void Buffer::append(const void* data, size_t len) {
    assert(data);
    append(static_cast<const char*>(data), len);
}

void Buffer::append(const Buffer& buff) {
    append(buff.peek(), buff.readableBytes());
}

ssize_t Buffer::readFd(int fd, int* errno_) {
    char buff[65535];
    struct iovec iov[2];
    // 分散读
    size_t writable = writableBytes();
    iov[0].iov_base = beginWrite();
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);
    ssize_t len = readv(fd, iov, 2);
    if (len < 0) {
        *errno_ = errno;
        return len;
    }
    if (static_cast<size_t>(len) <= writable) {
        hasWrittern_(len);
        return len;
    } else {
        hasWrittern_(writable);
        append(iov[1].iov_base, len - writable);
    }
    return len;
}

ssize_t Buffer::writefd(int fd, int* errno_) {
    size_t readable = readableBytes();
    int len = write(fd, peek(), readable);
    if (len < 0) {
        *errno_ = errno;
        return len;
    }
    readPos_ += len;
    return len;
}

void Buffer::hasWrittern_(size_t len) {
    assert(writePos_ + len <= buffer_.size());
    writePos_ += len;
}

char* Buffer::beginPtr_() {
    return buffer_.data();
}

const char* Buffer::beginPtr_() const {
    return buffer_.data();
}

void Buffer::makeSpace_(size_t len) {
    if (prependableBytes() + writableBytes() < len) {
        buffer_.resize(writePos_ + len + 1);
    } else {
        size_t readable = readableBytes();
        std::copy(peek(), static_cast<const char*>(beginWrite()), beginPtr_());
        readPos_ = 0;
        writePos_ = readable;
        assert(readable == readableBytes());
    }
}
