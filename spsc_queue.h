#ifndef __SPSC_QUEUE__
#define __SPSC_QUEUE__

#include <atomic>
#include <vector>

template<typename T>
class SPSCQueue {
private:
    // 使用vector作为底层固定大小的循环数组
    std::vector<T> buffer_;
    // 消费者线程只修改head_
    alignas(64) std::atomic<size_t> head_ {0};
    // 生产者线程只修改tail_
    alignas(64) std::atomic<size_t> tail_ {0};

    size_t next_(size_t current) const {
        return (current + 1) % buffer_.size();
    }

public:
    explicit SPSCQueue(size_t capacity)
        : buffer_(capacity + 1) // 多分配一个位置，用于区分队满和队空
    {
        // 初始状态 head_ 和 tail_ 均为0
    }

    // 生产者调用：尝试入队
    bool enqueue(const T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = next_(current_tail);

        // 判断队列是否已满
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // 队列满，入队失败
        }

        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    // 消费者调用：尝试出队
    bool dequeue(T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);

        // 判断队列是否为空
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // 队列空，出队失败
        }

        item = buffer_[current_head];
        const size_t next_head = next_(current_head);
        head_.store(next_head, std::memory_order_release);
        return true;
    }
};

#endif