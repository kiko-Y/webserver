#ifndef _HTTPCONN_H_
#define _HTTPCONN_H_

#include "../log/log.h"
#include "httprequest.h"
#include "httpresponse.h"


class HttpConn {
public:
    HttpConn();

    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);

    ssize_t read(int* saveErrno);

    ssize_t write(int* saveErrno);

    void close();

    int getFd() const;

    int getPort() const;

    std::string getIp() const;

    sockaddr_in getAddr() const;

    bool process();

    int toWriteBytes() const;

    bool isKeepAlive() const;


    static bool isET;

    static const char* srcDir_;

    static std::atomic<int> userCount;

private:
    int fd_;
    sockaddr_in addr_;

    bool isClose_;

    int iovCnt_;
    iovec iov_[2];

    Buffer readBuff_;
    Buffer writeBuff_;

    HttpRequest request_;
    HttpResponse response_;    

};

#endif
