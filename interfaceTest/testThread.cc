#include <thread>
#include <iostream>
#include <mutex>
#include <memory>
using namespace std;

void TestSimpleThread() {
    thread t1([] {
        cout <<  "Hello, cpp!\n";
    });
    t1.join();
}

void TestThreadDetachJoin() {
    thread t1([] {
        cout <<  "Hello, cpp!\n";
    });
    // 只能 detach 或者 join, 不能都使用
    // t1.detach();
    t1.join();
}

void TestCalculate() {
    int sum = 0;
    mutex mtx;
    thread threads[10];
    for (int i = 0; i < 10; i++) {
        threads[i] = thread([&sum, &mtx] {
            lock_guard<mutex> lk(mtx);
            for (int i = 0; i < 10000; i++) sum++;
        });
    }
    for (int i = 0; i < 10; i++) {
        threads[i].join();
    }
    cout << sum << endl;
}


class ThreadObject {
public:
    explicit ThreadObject(int val): val(val) {}
    ~ThreadObject() {
        cout << "deconstruct\n";
    }
    void delayRun();
private:
    int val;
};

void ThreadObject::delayRun() {std::this_thread::sleep_for(std::chrono::milliseconds(500));
    cout << "delayRun finished, values is " << val << '\n';
}

void TestRunAfterDeconstruct() {
    {
        ThreadObject* obj = new ThreadObject(42);
        thread([obj] { obj->delayRun(); }).detach();
        delete obj;
    }
    cout << "leave scope\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

int main() {
    // TestThreadDetachJoin();
    // TestCalculate();
    TestRunAfterDeconstruct();
    return 0;
}