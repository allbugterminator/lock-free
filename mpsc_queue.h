#ifndef __MPSC_QUEUE__
#define __MPSC_QUEUE__

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

// 标记指针定义
template<typename T>
struct TaggedPtr {
    T* ptr;
    uint64_t tag;

    bool operator==(const TaggedPtr& other) const {
        return ptr == other.ptr && tag == other.tag;
    }
};

// 节点定义
template<typename T>
struct Node {
    T data;
    std::atomic<TaggedPtr<Node>> next; // 下一个节点也是标记指针

    Node(T data) : data(std::move(data)) {
        next.store({{nullptr}, 0}, std::memory_order_relaxed);
    }
};

// 全局风险指针注册表（简化版，每个消费者线程一个风险指针）
template<typename T>
class HazardPointerRegistry {
public:
    static constexpr int MAX_THREADS = 100;
    static constexpr int HP_PER_THREAD = 1; // 每个消费者线程一个HP

    std::atomic<Node<T>*> pointers[MAX_THREADS * HP_PER_THREAD] = {};

    // 申请一个风险指针槽位
    std::atomic<Node<T>*>& acquire(int thread_id) {
        return pointers[thread_id * HP_PER_THREAD];
    }

    // 扫描并回收所有未被保护的内存
    void reclaim(Node<T>* node){
        // 简化实现：检查节点是否被任何风险指针引用
        for (int i = 0; i < MAX_THREADS * HP_PER_THREAD; ++i) {
            if (pointers[i].load(std::memory_order_acquire) == node) {
                return; // 被引用，不回收
            }
        }
        // 安全了，可以删除
        delete node;
    }
};

// 全局风险指针注册表
template<typename T>
HazardPointerRegistry<T> gp_hp_registry;

// MPSC队列主类
template<typename T>
class MPSCQueue {
private:
    // 尾指针使用标记指针
    alignas(64) std::atomic<TaggedPtr<Node<T>>> tail_;
    // 哑节点，用于简化边界条件处理
    alignas(64) Node<T>* dummy_head_;

public:
    MPSCQueue() {
        // 初始化时创建一个哑节点
        dummy_head_ = new Node<T>(T{});
        tail_.store({{dummy_head_}, 0}, std::memory_order_relaxed);
    }

    ~MPSCQueue() {
        // 简单遍历释放所有节点，生产环境中需更安全的方式
        TaggedPtr<Node<T>> curr = tail_.load(std::memory_order_relaxed);
        delete curr.ptr;
    }

    // 生产者：入队操作
    void enqueue(T data){
        Node<T>* new_node = new Node<T>(std::move(data));
        TaggedPtr<Node<T>> old_tail = tail_.load(std::memory_order_relaxed);

        while (true) {
            // 尝试将新节点链接到当前尾节点之后
            TaggedPtr<Node<T>> next = {nullptr, old_tail.tag + 1}; // 新节点next指向nullptr，标签递增
            new_node->next.store(next, std::memory_order_relaxed);

            TaggedPtr<Node<T>> new_tail = {new_node, old_tail.tag + 1}; // 新尾指针，标签递增

            // 关键CAS操作：如果当前tail的指针和标签仍与old_tail相同，则更新
            if (tail_.compare_exchange_weak(old_tail, new_tail,
                                            std::memory_order_acq_rel, // 成功内存序
                                            std::memory_order_relaxed)) { // 失败内存序
                // 入队成功！
                old_tail.ptr->next.store({new_node, next.tag}, std::memory_order_release);
                break;
            }
            // CAS失败，old_tail已被更新为最新值，循环重试
        }
    }

    // 消费者：出队操作
    bool dequeue(T& result, int consumer_thread_id){
        // 获取本线程的风险指针
        std::atomic<Node<T>*>& hp = gp_hp_registry<T>.acquire(consumer_thread_id);

        Node<T>* old_head = nullptr;
        TaggedPtr<Node<T>> old_next;

        while (true) {
            old_head = dummy_head_;
            // 将当前头节点的下一个节点存入风险指针
            hp.store(old_head->next.load(std::memory_order_acquire).ptr, std::memory_order_release);

            // 检查在设置风险指针后，头节点是否已被改变
            TaggedPtr<Node<T>> current_head_next = old_head->next.load(std::memory_order_acquire);
            if (hp.load(std::memory_order_acquire) != current_head_next.ptr) {
                // 被修改了，重试
                continue;
            }

            if (hp.load(std::memory_order_acquire) == nullptr) {
                // 队列为空
                hp.store(nullptr, std::memory_order_release);
                return false;
            }

            // 尝试移动头指针
            old_next = {hp.load(std::memory_order_acquire), current_head_next.tag};
            TaggedPtr<Node<T>> new_head = {old_next.ptr, old_head->next.load(std::memory_order_relaxed).tag + 1};

            if (dummy_head_->next.compare_exchange_strong(current_head_next, new_head,
                                                         std::memory_order_acq_rel)) {
                // 出队成功
                result = std::move(old_next.ptr->data);
                hp.store(nullptr, std::memory_order_release); // 释放风险指针

                // 将旧头节点（哑节点）放入回收列表，稍后由风险指针机制回收
                gp_hp_registry<T>.reclaim(old_head);
                // 设置新的哑节点
                dummy_head_ = old_next.ptr;
                return true;
            }
        }
        return false;
    }
};

#endif