#ifndef _SQLCONNRAII_H_
#define _SQLCONNRAII_H_
#include "sqlconnpool.h"
#include <assert.h>

// 管理 Sql 连接
class SqlConnRAII {
public:
    SqlConnRAII(SqlConnPool *connpool) {
        assert(connpool);
        sql_ = connpool->GetConn();
        connpool_ = connpool;
    }
    ~SqlConnRAII() {
        if (sql_) {
            connpool_->FreeConn(sql_);
        }
    }
    MYSQL& operator* () const { return *sql_; }
    MYSQL* operator-> () const { return sql_; }
    MYSQL* get() const { return sql_; }

private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

#endif