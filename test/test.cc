#include "../code/pool/threadpool.h"
#include "../code/pool/sqlconnpool.h"
#include "../code/pool/sqlconnRAII.h"
#include "../code/timer/heaptimer.h"
#include "../code/log/blockingqueue.h"
#include "../code/log/log.h"
#include "../code/buffer/buffer.h"
#include "../code/http/httprequest.h"
#include "../code/http/httpresponse.h"
#include "../code/http/httpconn.h"
#include "../code/server/epoller.h"
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

void TestAsyncLog() {
    cout << "=================Testing AsyncLog=================" << endl;
    Log* log = Log::Instance();
    log->init(Log::LogLevel::INFO, "./log", ".log", 1);
    for (int i = 0; i < 5; i++) {
        thread([i, log] {
            Log_Info("thread %d write a log", i);
        }).detach();
    }
    this_thread::sleep_for(chrono::milliseconds(100));
    string s;
    for (int i = 0; i < 128; i++) s.append("x");
    s.append("o");
    Log_Info(s);
}

void TestSyncLog() {
    cout << "=================Testing SyncLog=================" << endl;
    Log* log = Log::Instance();
    log->init(Log::LogLevel::INFO, "./log", ".log", 0);
    for (int i = 0; i < 5; i++) {
        thread([i, log] {
            Log_Info("thread %d write a log", i);
        }).detach();
    }
    this_thread::sleep_for(chrono::milliseconds(100));
}

void TestHttpRequestGetLine() {
    cout << "=================Testing HttpRequestGetLine=================" << endl;
    int p[2];
    assert(pipe(p) != -1);
    HttpRequest req;
    req.init();
    Buffer buff;
    int err;

    thread([wp = p[1]] {
        assert(write(wp, "line1\r\nline2\r\n", 14) == 14);
    }).detach();
    buff.readFd(p[0], &err);
    
    string line;
    HttpRequest::LINE_STATE lineState = req.getLine(buff, line);
    assert(lineState == HttpRequest::LINE_OK);
    assert(line == "line1");
    lineState = req.getLine(buff, line);
    assert(lineState == HttpRequest::LINE_OK);
    assert(line == "line1");
    buff.retrieve(line.size() + 2);
    lineState = req.getLine(buff, line);
    assert(lineState == HttpRequest::LINE_OK);
    assert(line == "line2");
}

void TestHttpRequestParse() {
    cout << "=================Testing HttpRequestParse=================" << endl;
    {
        int p[2];
        assert(pipe(p) != -1);
        HttpRequest req;
        Buffer buff;
        int err;


        thread([wp = p[1]] {
            // string s("POST /index HTTP/1.1\r\n"
            //          "Content-Length: 33\r\n"
            //          "Connection: keep-alive\r\n"
            //          "Content-Type: application/x-www-form-urlencoded\r\n"
            //          "\r\n"
            //          "username=kiko&password=1004535809");
            string s("POST /index HTTP/1.1\r\n"
                     "Content-Length: 43\r\n"
                     "Connection: keep-alive\r\n"
                     "Content-Type: application/x-www-form-urlencoded\r\n"
                     "\r\n"
                     "username=kiko&password=1004535809&arg=X%20X");
            assert(write(wp, s.c_str(), s.length()) == static_cast<ssize_t>(s.length()));
        }).detach();
        buff.readFd(p[0], &err);
        HttpRequest::HTTP_CODE code = req.parse(buff);
        assert(code == HttpRequest::GET_REQUEST);
        cout << req.method() << endl;
        cout << req.path() << endl;
        cout << req.version() << endl;
        cout << req.body() << endl;
        cout << "username in post: " << req.getPost("username") << endl;
        cout << "password in post: " << req.getPost("password") << endl;
        cout << "arg in post: " << req.getPost("arg") << endl;
    }
}

void TestHttpResponse() {
    cout << "=================Testing HttpResponse=================" << endl;
    {
        HttpResponse resp;
        char* srcDir = getcwd(nullptr, 256);
        strncat(srcDir, "/resources", 16);
        resp.init(srcDir, "/index.html");
        cout << "after init" << endl;
        Buffer buff;
        resp.makeResponse(buff);
        string respContent = buff.retrieveAllToString();
        cout << respContent << endl;
        
    }
}

void TestHttpConn() {
    cout << "=================Testing HttpConn=================" << endl;
    {
        HttpConn conn;
        int p[2];
        assert(pipe(p) != -1);
        HttpConn::isET = true;
        // TODO: TEST
    }

}

int setFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


void TestEpoller() {
    cout << "=================Testing Epoller=================" << endl;
    {
        Epoller epoller(16);
        atomic<bool> isClosed = false;
        thread([&isClosed, &epoller] {
            char buf[64];
            while(!isClosed) {
                int n = epoller.wait(200);
                if (n != -1) {
                    cout << "EPOLL trigered with " << n << endl;;
                }
                for (int i = 0; i < n; i++) {
                    uint32_t events = epoller.getEvents(i);
                    int fd = epoller.getEventFd(i);
                    if (events & EPOLLIN) {
                        int ret = read(fd, buf, sizeof(buf));
                        cout << "epoll thread read: " << buf << ", len: " << ret << endl;
                    } else if (events & EPOLLOUT) {
                        int ret = write(fd, "hello", 5);
                        cout << "epoll thread write: " << "hello" << ", len: " << ret << endl;
                    }
                }
            }
        }).detach();
        this_thread::sleep_for(chrono::milliseconds(50));
        int p1[2];
        int p2[2];
        assert(!pipe(p1));
        assert(!pipe(p2));
        int ret = write(p1[1], "kiko's data", 11);
        cout << "main thread write: " << "kiko's data" << ", len: " << ret << endl;
        epoller.addFd(p1[0], EPOLLIN | EPOLLONESHOT);

        this_thread::sleep_for(chrono::milliseconds(50));
        epoller.addFd(p2[1], EPOLLOUT | EPOLLONESHOT);
        this_thread::sleep_for(chrono::milliseconds(50));
        char buf[64];
        int len = read(p2[0], buf, 64);
        cout << "main thread read: " << buf << ", len: " << len << endl;
        this_thread::sleep_for(chrono::milliseconds(500));
        epoller.delFd(p1[0]);
        epoller.delFd(p2[1]);
        isClosed = true;

    }
}

int main() {
    // TestThreadPool();
    // TestSqlPool();
    // TestHeapTimer();
    // TestBlockingQueue();
    // TestBuffer();
    // TestAsyncLog();
    // TestSyncLog();
    // TestHttpRequestGetLine();
    // TestHttpRequestParse();
    // TestHttpResponse();
    TestEpoller();
    this_thread::sleep_for(chrono::milliseconds(500));
    cout << "TEST FINISHED\n";
    return 0;
}