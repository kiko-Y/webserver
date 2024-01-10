#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h"



class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool optLinger, 
        int sqlPort, const std::string& sqlUser, const std::string& sqlPwd, 
        const std::string& dbName, int connPoolNum, int threadNum,
        bool openLog, Log::LogLevel logLevel, int logQueSize);

    ~WebServer();
    void start();

private:
    bool initSocket_(); 
    void initEventMode_(int trigMode);
    void addClient_(int fd, sockaddr_in addr);
  
    void dealListen_();
    void dealWrite_(HttpConn* client);
    void dealRead_(HttpConn* client);

    void extentTime_(HttpConn* client);
    void closeConn_(HttpConn* client);

    void onRead_(HttpConn* client);
    void onWrite_(HttpConn* client);
    void onProcess(HttpConn* client);

    static const int MAX_FD = 65536;

    static int SetFdNonblock(int fd);

    int port_;
    bool openLinger_;
    int timeoutMS_;  /* 毫秒MS */
    bool isClose_;
    int listenFd_;
    std::string srcDir_;
    
    uint32_t listenEvent_;
    uint32_t connEvent_;
   
    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> users_;
};

#endif
