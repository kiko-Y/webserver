#include "sqlconnpool.h"
#include <assert.h>
#include <iostream>
using namespace std;

SqlConnPool::SqlConnPool() {
}

SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool connPool;
    return &connPool;
}

MYSQL* SqlConnPool::GetConn() {
    unique_lock<mutex> locker(mtx_);
    MYSQL* sql = nullptr;
    while (connQue_.empty()) {
        cv_.wait(locker);
    }
    sql = connQue_.front();
    connQue_.pop();
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* conn) {
    assert(conn);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(conn);
    cv_.notify_one();
}

int SqlConnPool::GetFreeConnCount() {
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}

void SqlConnPool::Init(std::string_view host, int port,
              std::string_view user, std::string_view pwd,
              std::string_view dbName, int connSize) {
    lock_guard<mutex> locker(mtx_);
    assert(connSize > 0);
    MAX_CONN_ = connSize;
    for (int i = 0; i < connSize; i++) {
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            // TODO: log mysql init error
            assert(sql);
        }
        sql = mysql_real_connect(sql, host.data(), user.data(),
                                 pwd.data(), dbName.data(), 
                                 port, nullptr, 0);

        if (!sql) {
            // TODO: log mysql connect error
            assert(sql);
        }
        connQue_.push(sql);
    }
}

void SqlConnPool::ClosePool() {
    lock_guard<mutex> locker(mtx_);
    while (!connQue_.empty()) {
        auto item = connQue_.front();
        connQue_.pop();
        mysql_close(item);
    }
    mysql_library_end();
}


SqlConnPool::~SqlConnPool() {
    ClosePool();
}