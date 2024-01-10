#include "webserver.h"

using namespace std;



WebServer::WebServer(
    int port, int trigMode, int timeoutMS, bool optLinger, 
    int sqlPort, const std::string& sqlUser, const std::string& sqlPwd, 
    const std::string& dbName, int connPoolNum, int threadNum,
    bool openLog, Log::LogLevel logLevel, int logQueSize): 
    port_(port), openLinger_(optLinger), timeoutMS_(timeoutMS), isClose_(false),
    timer_(new HeapTimer()), threadpool_(new ThreadPool()), epoller_(new Epoller()) {
    if (openLog) {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
    }
    if (port > 65535 || port < 1024) {
        Log_Error("Port:%d error!",  port_);
        isClose_ = true;
        return;
    }
    srcDir_ = getcwd(nullptr, 256);
    assert(!srcDir_.empty());
    srcDir_ += "/resources";
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    
    initEventMode_(trigMode);
    if (!initSocket_()) {
        isClose_ = true;
    }
    if (openLog) {
        if (isClose_) Log_Error("========== Server init error!==========");
        else {
            Log_Info("========== Server init ==========");
            Log_Info("Port:%d, OpenLinger: %s", port_, optLinger ? "true": "false");
            Log_Info("listenFd: %d", listenFd_);
            Log_Info("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            Log_Info("LogSys level: %d", logLevel);
            Log_Info("srcDir: %s", HttpConn::srcDir.c_str());
            Log_Info("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

WebServer::~WebServer() {
    if (!isClose_) {
        isClose_ = true;
        close(listenFd_);
    }
    SqlConnPool::Instance()->ClosePool();
}


void WebServer::start() {
    if(!isClose_) {
        Log_Info("========== Server start =========="); 
    }
    int timeoutMs = -1;
    while (!isClose_) {
        if (timeoutMS_ > 0) {
            timeoutMs = timer_->getNextTickMs();
        }
        int eventCnt = epoller_->wait(timeoutMs);
        Log_Info("epoll_wait get eventCnt: %d", eventCnt);
        for (int i = 0; i < eventCnt; i++) {
            int eventFd = epoller_->getEventFd(i);
            uint32_t events = epoller_->getEvents(i);
            if (eventFd == listenFd_) {
                dealListen_();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(eventFd) > 0);
                closeConn_(&users_[eventFd]);
            } else if (events & EPOLLIN) {
                assert(users_.count(eventFd) > 0);
                dealRead_(&users_[eventFd]);                    
            } else if (events & EPOLLOUT) {
                assert(users_.count(eventFd) > 0);
                dealWrite_(&users_[eventFd]);
            } else {
                Log_Error("Unexpected Event");
            }
        }
    }
}

bool WebServer::initSocket_() {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    struct linger optLinger = {0};
    if (openLinger_) {
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        Log_Error("create socket error!");
        return false;
    }
    int ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0) {
        close(listenFd_);
        Log_Error("Init linger error");
        return false;
    }
    // TODO: read this code SO_REUSEADDR
    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        Log_Error("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        Log_Error("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = listen(listenFd_, 6);
    if (ret < 0) {
        Log_Error("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = epoller_->addFd(listenFd_, listenEvent_ | EPOLLIN);
    if (!ret) {
        Log_Error("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    Log_Info("Server listen at port:%d", port_);
    return true;
}

void WebServer::initEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        connEvent_ |= EPOLLET;
        listenEvent_ |= EPOLLET;
        break;
    default:
        connEvent_ |= EPOLLET;
        listenEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET); 
}

void WebServer::addClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if (timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, [this, fd] {
            closeConn_(&this->users_[fd]);
        });
    }
    epoller_->addFd(fd, connEvent_ | EPOLLIN);
    SetFdNonblock(fd);
    Log_Info("Client[%d] in!", fd);
}

void WebServer::dealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (sockaddr*)&addr, &len);
        if (fd < 0) return;
        else if(HttpConn::userCount >= MAX_FD) {
            string info("Server busy!");
            int ret = send(fd, info.data(), info.length(), 0);
            if (ret < 0) {
                Log_Warn("send error to client[%d] error", fd);
            }
            close(fd);
            Log_Warn("Client is full");
            return;
        }
        addClient_(fd, addr);
    } while (listenEvent_ & EPOLLET);
}

void WebServer::dealWrite_(HttpConn* client) {
    assert(client);
    extentTime_(client);
    threadpool_->AddTask([this, client] {
        onWrite_(client);
    });
}

void WebServer::dealRead_(HttpConn* client) {
    assert(client);
    extentTime_(client);
    threadpool_->AddTask([this, client] {
        onRead_(client);
    });
}

void WebServer::extentTime_(HttpConn* client) {
    assert(client);
    if (timeoutMS_ > 0) timer_->adjust(client->getFd(), timeoutMS_);
}

void WebServer::closeConn_(HttpConn* client) {
    assert(client);
    Log_Info("Client[%d] quit!", client->getFd());
    epoller_->delFd(client->getFd());
    client->close();
}

void WebServer::onRead_(HttpConn* client) {
    assert(client);
    int readErrno = 0;
    ssize_t ret = client->read(&readErrno);
    if (ret < 0 && readErrno != EAGAIN) {
        closeConn_(client);
        Log_Error("client[%d] error when read", client->getFd());
        return;
    }
    onProcess(client);
}

void WebServer::onWrite_(HttpConn* client) {
    assert(client);
    int writeErrno = 0;
    int ret = client->write(&writeErrno);
    if (client->toWriteBytes() == 0) {
        // 写完了
        if(client->isKeepAlive()) {
            onProcess(client);
            return;
        }
    } else if (ret < 0) {
        if (writeErrno == EAGAIN) {
            // 缓冲区写满了, 要重新写
            epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);
        }
    }
    closeConn_(client);
}

void WebServer::onProcess(HttpConn* client) {
    assert(client);
    if (client->process()) {
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);
    } else {
        // 没有读入完成, 需要继续读入
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLIN);
    }
}


int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}