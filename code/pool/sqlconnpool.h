#ifndef _SQLCONNPOOL_H_
#define _SQLCONNPOOL_H_

#include <mysql/mysql.h>
#include <string_view>
#include <queue>
#include <mutex>
#include <condition_variable>

class SqlConnPool {
public:
    // 获取单例
    static SqlConnPool *Instance();
    
    // 获取 mysql 连接, 没有空闲的会阻塞等待
    MYSQL *GetConn();
    
    // 释放 mysql 连接
    void FreeConn(MYSQL *conn);
    
    // 获取目前的空闲连接数
    int GetFreeConnCount();

    // 初始化连接池
    void Init(std::string_view host, int port,
              std::string_view user, std::string_view pwd,
              std::string_view dbName, int connSize);
    
    // 关闭连接池
    void ClosePool();
private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_;

    std::queue<MYSQL*> connQue_;
    std::mutex mtx_;
    std::condition_variable cv_;
};


#endif
