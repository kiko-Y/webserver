#include "heaptimer.h"
#include <assert.h>

using namespace std;

void HeapTimer::shiftup_(size_t i) {
    assert(i >= 0 && i < size());
    while (i > 0) {
        size_t j = (i - 1) >> 1;
        if (heap_[j] <= heap_[i]) {
            break;
        }
        swapNode_(i, j);
        i = j;
    }
}

void HeapTimer::swapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < size());
    assert(j >= 0 && j < size());
    if (i == j) return;
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
}

bool HeapTimer::shiftdown_(size_t index) {
    assert(index >= 0 && index < size());
    size_t i = index;
    size_t j = i * 2 + 1;
    size_t n = size();
    while(j < n) {
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if(heap_[i] <= heap_[j]) break;
        swapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

void HeapTimer::adjust(int id, int timeoutMs) {
    assert(ref_.count(id) > 0);
    size_t index = ref_[id];
    heap_[index].expires = Clock::now() + MS(timeoutMs);
    if (!shiftdown_(index)) {
        shiftup_(index);
    }
}

void HeapTimer::add(int id, int timeoutMs, const TimeoutCallBack& cb) {
    assert (id >= 0);
    if (ref_.count(id) == 0) {
        // 新节点
        size_t index = size();
        ref_[id] = index;
        heap_.push_back({id, Clock::now() + MS(timeoutMs), cb});
        shiftup_(index);
    } else {
        // 已有节点, 重新调整
        size_t index = ref_[id];
        heap_[index].cb = cb;
        adjust(id, timeoutMs);
    }
}


void HeapTimer::del_(size_t index) {
    assert (index >= 0 && index < size());
    swapNode_(index, heap_.size() - 1);
    int id = heap_.back().id;
    ref_.erase(id);
    heap_.pop_back();
    if (index < heap_.size()) {
        shiftdown_(index);
    }
}

void HeapTimer::doWork(int id) {
    assert(id >= 0);
    if (ref_.count(id) == 0) {
        return;
    }
    size_t index = ref_[id];
    heap_[index].cb();
    del_(index);
}

void HeapTimer::pop() {
    assert(!empty());
    del_(0);
}

void HeapTimer::clear() {
    heap_.clear();
    ref_.clear();
}

void HeapTimer::tick() {
    if (empty()) return;
    while (!empty()) {
        TimerNode node = heap_.front();
        // 还没有超时
        if (chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        node.cb();
        pop();
    }
}

int HeapTimer::getNextTickMs() {
    tick();
    int nextTickMs = -1;
    if (!empty()) {
        nextTickMs = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if (nextTickMs < 0) nextTickMs = 0;
    }
    return nextTickMs;
}
