#ifndef _HEAPTIMER_H_
#define _HEAPTIMER_H_

#include <chrono>
#include <functional>
#include <vector>

using TimeoutCallBack = std::function<void()>;
using Clock = std::chrono::high_resolution_clock;
using MS = std::chrono::milliseconds;
using TimeStamp = Clock::time_point;

struct TimerNode {
    int id;
    // 超时时间
    TimeStamp expires;
    // 超时回调函数
    TimeoutCallBack cb;
    bool operator< (const TimerNode& rhs) {
        return expires < rhs.expires;
    }

    bool operator<= (const TimerNode& rhs) {
        return expires <= rhs.expires;
    }

    bool operator> (const TimerNode& rhs) {
        return expires > rhs.expires;
    }

    bool operator>= (const TimerNode& rhs) {
        return expires >= rhs.expires;
    }

    bool operator== (const TimerNode& rhs) {
        return expires == rhs.expires;
    }

    bool operator!= (const TimerNode& rhs) {
        return expires != rhs.expires;
    }
};

class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }
    
    ~HeapTimer() { 
        clear();
    }
    
    // 重新调整超时时间
    void adjust(int id, int newTimeOutMs);

    // 添加一个新的计时器
    void add(int id, int timeoutMs, const TimeoutCallBack& cb);

    // 调用回调函数并删除节点
    void doWork(int id);

    // 清空堆信息
    void clear();

    // 清除超时节点
    void tick();

    // 弹出堆顶元素
    void pop();

    // 获取下一个超时时间
    int getNextTickMs();

    size_t size() { return heap_.size(); };

    bool empty() { return size() == 0; }

private:
    void del_(size_t index);

    void shiftup_(size_t index);

    bool shiftdown_(size_t index);

    void swapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;
    // id -> heap position
    std::unordered_map<int, size_t> ref_;
};


#endif