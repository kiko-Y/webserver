#include "../code/pool/threadpool.h"
#include "../code/pool/sqlconnpool.h"
#include "../code/pool/sqlconnRAII.h"
#include "../code/timer/heaptimer.h"
#include "../code/log/blockingqueue.h"
#include "../code/buffer/buffer.h"
#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <mutex>

using namespace std;

class Timer {
public:
    using Clock = chrono::steady_clock;
    using TimeStamp = Clock::time_point;
    Timer(): startTime(Clock::now()) {}
    int timeSinceStartMs() const {
        return chrono::duration_cast<chrono::milliseconds>(Clock::now() - startTime).count();
    }
private:
    TimeStamp startTime;
};

void TestThreadPool() {
    cout << "=================Testing ThreadPool=================" << endl;
    unique_ptr<ThreadPool> pool = make_unique<ThreadPool>(2);
    std::mutex mtx;
    chrono::steady_clock::time_point start = chrono::steady_clock::now();
    for (int i = 0; i < 3; i++) {
        pool->AddTask([i, &mtx, start] {
            std::this_thread::sleep_for(chrono::milliseconds(100));
            chrono::steady_clock::time_point end = chrono::steady_clock::now();
            std::lock_guard<mutex> locker(mtx);
            cout << "thread " << i << " finished, currentTime: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << endl;
        });
    }
    std::this_thread::sleep_for(chrono::milliseconds(300));
}

void TestSqlPool() {
    cout << "=================Testing SqlConnPool=================" << endl;
    SqlConnPool* pool = SqlConnPool::Instance();
    string host = "localhost";
    string user = "root";
    string pwd = "1004535809";
    string dbName = "testdb";
    cout << "Init" << endl;
    pool->Init(host, 3306, user, pwd, dbName, 2);
    auto query = [pool] {
        cout << "Get Connection" << endl;
        SqlConnRAII connRAII(pool);
        MYSQL* conn = connRAII.get();
        string sql = "SELECT username, password FROM user";
        mysql_query(conn, sql.c_str());
        MYSQL_RES* res = mysql_store_result(conn);
        int num_fields = mysql_num_fields(res);
        int num_rows = mysql_num_rows(res);
        cout << "num_rows: " <<  num_rows << endl;
        cout << "num_fields: " <<  num_fields << endl;
        while (MYSQL_ROW row = mysql_fetch_row(res)) {
            for (int i = 0; i < num_fields; i++) {
                cout << row[i] << ' ';
            }
            cout << endl;
        }
    };
    for (int i = 0; i < 2; i++) {
        thread([query, i] {
            query();
            cout << "thread " << i << " finished\n";
        }).detach();
    }
    thread([query] {
        this_thread::sleep_for(chrono::milliseconds(100));
        query();
        cout << "thread " << 2 << " finished\n";
    }).detach();
    
}

// g++ -std=c++17 test/test.cc pool/sqlconnpool.cc -pthread -lmysqlclient -o mytest && ./mytest

void TestHeapTimer() {
    {
        cout << "=================Testing HeapTimer1=================" << endl;
        unique_ptr<HeapTimer> ht = make_unique<HeapTimer>();
        ht->add(1, 100, [] { cout << "timer 1 finished\n"; });
        ht->add(2, 50, [] { cout << "timer 2 finished\n"; });
        cout << "# sleep for 75 ms\n";
        std::this_thread::sleep_for(chrono::milliseconds(75));
        int nextTickMs = ht->getNextTickMs();
        cout << "nextTickMs: " << nextTickMs << endl;
        cout << "# sleep for 75 ms\n";
        std::this_thread::sleep_for(chrono::milliseconds(75));
        nextTickMs = ht->getNextTickMs();
        cout << "nextTickMs: " << nextTickMs << endl;
        assert(nextTickMs == -1);
        assert(ht->empty());
    }
    {
        cout << "=================Testing HeapTimer2=================" << endl;
        unique_ptr<HeapTimer> ht = make_unique<HeapTimer>();
        ht->add(1, 300, [] { cout << "timer 1 finished\n"; });
        ht->add(2, 100, [] { cout << "timer 2 finished\n"; });
        ht->add(3, 200, [] { cout << "timer 3 finished\n"; });
        ht->add(4, 50, [] { cout << "timer 4 finished\n"; });
        ht->tick();
        assert(ht->size() == 4);
        cout << "# sleep for 110 ms\n";
        std::this_thread::sleep_for(chrono::milliseconds(110));
        ht->adjust(2, 200);
        cout << "#tick\n";
        ht->tick();
        cout << "# sleep for 220 ms\n";
        std::this_thread::sleep_for(chrono::milliseconds(220));
        cout << "#tick\n";
        ht->tick();
        assert(ht->empty());
    }
}

void TestBlockingQueue() {
    cout << "=================Testing BlockingQueue=================" << endl;
    std::shared_ptr<BlockingQueue<int>> bq = std::make_unique<BlockingQueue<int>>(2);
    assert(bq->capacity() == 2);
    std::shared_ptr<Timer> timer = std::make_shared<Timer>();
    for (int i = 0; i < 4; i++) {
        std::thread([i, bq, &timer] {
            bq->push(i);
            cout << "producer " << i << " finished at " << timer->timeSinceStartMs() << "ms\n";
        }).detach();
    }
    std::this_thread::sleep_for(chrono::milliseconds(10));
    assert(bq->size() == 2);
    assert(bq->full());
    std::thread([bq, &timer] {
        while (!bq->closed()) {
            std::this_thread::sleep_for(chrono::milliseconds(50));
            int val = -1;
            if (bq->pop(val)) {
                cout << "received val " << val << " at " << timer->timeSinceStartMs() << "ms\n";
            }
        }
        cout << "blockingqueue closed, consumer thread finished at " << timer->timeSinceStartMs() << " ms\n";
    }).detach();
    std::this_thread::sleep_for(chrono::milliseconds(500));
    cout << "closing at " << timer->timeSinceStartMs() << "ms\n";
    bq->close();
    cout << "closing finished at " << timer->timeSinceStartMs() << "ms\n";
    assert(bq->empty());
}

void TestBuffer() {
    cout << "=================Testing Buffer=================" << endl;
    shared_ptr<Buffer> buf = make_shared<Buffer>(16);
    int p[2];
    assert(!pipe(p));
    thread([wp = p[1]] {
        int len = write(wp, "hello, world.", 13);
        assert(len == 13);
        cout << "write len from other thread: " << len << '\n';
    }).detach();
    this_thread::sleep_for(chrono::milliseconds(100));
    int curErrno;
    ssize_t readLen = buf->readFd(p[0], &curErrno);
    assert(readLen == 13);

    cout << buf->peek() << endl;
    buf->append("kiko is the best", 16);
    assert(buf->readableBytes() == 29);

    // read from pipe in other thread
    thread([rp = p[0]] {
        char buf[32];
        int readLen = read(rp, buf, 32);
        assert(readLen == 29);
        cout << "read len from other thread: " << readLen << '\n';
    }).detach();
    this_thread::sleep_for(chrono::milliseconds(100));
    ssize_t writeLen = buf->writefd(p[1], &curErrno);
    assert(writeLen == 29);
    assert(buf->readableBytes() == 0);
    assert(buf->writableBytes() == 1);
    assert(buf->prependableBytes() == 29);

    // write from pipe in other thread, and read to buffer
    thread([wp = p[1]] {
        int len = write(wp, "good", 4);
        assert(len == 4);
        cout << "write len from other thread: " << len << '\n';
    }).detach();
    this_thread::sleep_for(chrono::milliseconds(100));
    readLen = buf->readFd(p[0], &curErrno);
    assert(readLen == 4);
    assert(buf->readableBytes() == 4);
    assert(buf->writableBytes() == 26);
    assert(buf->prependableBytes() == 0);
}

int main() {
    // TestThreadPool();
    // TestSqlPool();
    // TestHeapTimer();
    // TestBlockingQueue();
    TestBuffer();
    this_thread::sleep_for(chrono::milliseconds(1000));
    cout << "TEST FINISHED\n";
    return 0;
}