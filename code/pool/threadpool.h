
#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <thread>
#include <assert.h>


// 线程池
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
        assert(threadCount > 0);
        for (size_t i = 0; i < threadCount; i++) {
            std::thread([pool = pool_] {
                // 判断是否有任务, 有的话就执行, 否则等待任务
                std::unique_lock<std::mutex> locker(pool->mtx);
                while (true) {
                    if (!pool->tasks.empty()) {
                        auto task = pool->tasks.front();
                        pool->tasks.pop();
                        locker.unlock();
                        task();
                        locker.lock();
                    } else if (pool->isClosed) {
                        break;
                    } else {
                        pool->cond.wait(locker);
                    }
                }
            }).detach();
        }
    }

    ~ThreadPool() {
        if (pool_) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            pool_->cond.notify_all();
        }
    }

    template <typename F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();
    }

    
private:
    struct Pool {
        std::mutex mtx;
        std::condition_variable cond;
        bool isClosed;
        std::queue<std::function<void()>> tasks;
    };
    std::shared_ptr<Pool> pool_;
};

#endif
