#include "httpconn.h"

using namespace std;

bool HttpConn::isET;
string HttpConn::srcDir;
atomic<int> HttpConn::userCount;

HttpConn::HttpConn() {
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
}

HttpConn::~HttpConn() {
    close();
}


void HttpConn::init(int sockFd, const sockaddr_in& addr) {
    assert(sockFd > 0);
    userCount++;
    addr_ = addr;
    fd_ = sockFd;
    writeBuff_.retrieveAll();
    readBuff_.retrieveAll();
    isClose_ = false;
    Log_Info("Client[%d](%s:%d) in, userCount:%d", fd_, getIp().c_str(), getPort(), (int)userCount);
}

void HttpConn::close() {
    response_.unmapFile();
    if (isClose_ == false) {
        isClose_ = true;
        ::close(fd_);
        userCount--;
        Log_Info("Client[%d](%s:%d) quit, UserCount:%d", fd_, getIp().c_str(), getPort(), (int)userCount);
    }
}

int HttpConn::getFd() const {
    return fd_;
}

int HttpConn::getPort() const {
    return addr_.sin_port;
}


string HttpConn::getIp() const {
    return inet_ntoa(addr_.sin_addr);
}

sockaddr_in HttpConn::getAddr() const {
    return addr_;
}


ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = readBuff_.readFd(fd_, saveErrno);
        if (len <= 0) break;
    } while (isET);
    return len;
}

ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_);
        if(len <= 0) {
            *saveErrno = errno;
            break;
        }
        if(static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.retrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.retrieve(len);
        }
    } while (isET || toWriteBytes() > 10240 || toWriteBytes() == 0);
    return len;
}

bool HttpConn::process() {
    request_.init();
    if (readBuff_.readableBytes() <= 0) return false;
    HttpRequest::HTTP_CODE httpCode = request_.parse(readBuff_);
    switch (httpCode) {
    case HttpRequest::HTTP_CODE::NO_REQUEST:
        return false;
    case HttpRequest::HTTP_CODE::GET_REQUEST:
        Log_Debug("process response with path; %s", request_.path().c_str());
        response_.init(srcDir, request_.path(), request_.isKeepAlive(), 200);
        break;
    case HttpRequest::HTTP_CODE::BAD_REQUEST:
        response_.init(srcDir, request_.path(), false, 400);
        break;
    default:
        Log_Error("Error in handle http_code %d", httpCode);
    }
    response_.makeResponse(writeBuff_);
    /* 响应头 */
    iov_[0].iov_base = const_cast<char*>(writeBuff_.peek());
    iov_[0].iov_len = writeBuff_.readableBytes();
    iovCnt_ = 1;

    /* 文件 */
    if(response_.file() && response_.fileLen() > 0) {
        iov_[1].iov_base = response_.file();
        iov_[1].iov_len = response_.fileLen();
        iovCnt_ = 2;
    }
    Log_Debug("filesize:%d, %d  to %d", response_.fileLen() , iovCnt_, toWriteBytes());
    return true;
}

int HttpConn::toWriteBytes() const {
    return iov_[0].iov_len + iov_[1].iov_len;
}

bool HttpConn::isKeepAlive() const {
    return request_.isKeepAlive();
}




