#ifndef __STACK_H__
#define __STACK_H__

#include <atomic>

template<typename T>
struct Node {
    T data;
    Node* next;
    Node(const T& data) : data(data), next(nullptr){}
};

template<typename T>
class LockFreeStack {
private:
    std::atomic<Node<T>*> head{nullptr};

public:
    void push(const T& data) {
        Node<T> *new_data = new Node<T>(data);
        new_data->next = head.load();// 1. 设置新节点的next指向当前头节点
        // 2. 使用CAS将头指针原子地替换为新节点
        while (!head.compare_exchange_weak(new_data->next, new_data)) {
            // CAS失败：说明在步骤1之后，head被其他线程修改了
            // new_node->next 已被CAS函数更新为新的head，循环会重试
        }
    }

    bool pop(T& result) {
        Node<T> *old_data = head.load();
        while (old_data != nullptr && !head.compare_exchange_weak(old_data, old_data->next)) {
            // CAS失败：head已被其他线程修改，用新的head重试
        }

        if (old_data == nullptr) {
            return false; // 栈为空
        }

        result = old_data->data;
        delete old_data; // 注意：在无锁结构中安全释放内存是个复杂问题
        return true;
    }
};

#endif