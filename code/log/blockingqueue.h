#ifndef _BLOCKINGQUEUE_H_
#define _BLOCKINGQUEUE_H_

#include <mutex>
#include <queue>
#include <condition_variable>
#include <assert.h>
#include <chrono>

template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(size_t maxCapacity = 1000);

    ~BlockingQueue();

    void clear();

    bool empty();

    bool full();

    void close();

    bool closed() {
        std::lock_guard<std::mutex> locker(mtx_);
        return isClosed;
    }

    size_t size();
    
    size_t capacity();

    T front();

    T back();

    void push(const T& item);

    bool pop(T& item);

    bool pop(T& item, int timeoutMs);

    void flush();


private:
    std::queue<T> que_;

    size_t capacity_;

    std::mutex mtx_;

    bool isClosed;

    std::condition_variable condConsumer_;

    std::condition_variable condProducer_;

};

template <typename T>
BlockingQueue<T>::BlockingQueue(size_t maxCapacity): capacity_(maxCapacity) {
    assert(maxCapacity > 0);
    isClosed = false;
}

template <typename T>
BlockingQueue<T>::~BlockingQueue() {
    close();
}

template <typename T>
void BlockingQueue<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    while(!que_.empty()) que_.pop();
}

template <typename T>
bool BlockingQueue<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return que_.empty();
}

template <typename T>
bool BlockingQueue<T>::full() {
    std::lock_guard<std::mutex> locker(mtx_);
    return que_.size() >= capacity_;
}

template <typename T>
void BlockingQueue<T>::close() {
    {
        std::lock_guard<std::mutex> locker(mtx_);
        isClosed = true;
        while(!que_.empty()) que_.pop();
    }
    condConsumer_.notify_all();
    condProducer_.notify_all();
}

template <typename T>
size_t BlockingQueue<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return que_.size();
}

template <typename T>
size_t BlockingQueue<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

template <typename T>
T BlockingQueue<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return que_.front();
}

template <typename T>
T BlockingQueue<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return que_.back();
}

template <typename T>
void BlockingQueue<T>::push(const T& item) {
    std::unique_lock<std::mutex> locker(mtx_);
    condProducer_.wait(locker, [this] {
        return que_.size() < capacity_ || isClosed;
    });
    if (isClosed) return;
    que_.push(item);
    condConsumer_.notify_one();
}

template <typename T>
bool BlockingQueue<T>::pop(T& item) {
    std::unique_lock<std::mutex> locker(mtx_);
    condConsumer_.wait(locker, [this] {
        return !que_.empty() || isClosed;
    });
    if (isClosed) return false;
    item = que_.front();
    que_.pop();
    condProducer_.notify_one();
    return true;
}

template <typename T>
bool BlockingQueue<T>::pop(T& item, int timeoutMs) {
    std::unique_lock<std::mutex> locker(mtx_);
    std::cv_status status = condConsumer_.wait_for(locker, std::chrono::milliseconds(timeoutMs), [this] {
        return !que_.empty() || isClosed;
    });
    if (status == std::cv_status::timeout || isClosed) return false;
    item = que_.front();
    que_.pop();
    condProducer_.notify_one();
    return true;
}

// TODO: 不知道为什么需要这个
template <typename T>
void BlockingQueue<T>::flush() {
    condConsumer_.notify_one();
}

#endif