#include <thread>
#include <vector>
#include <iostream>
#include "stack.h"

void testLockFreeStack() {
    LockFreeStack<int> stack;
    std::vector<std::thread> threads;

    // 两个线程并发push
    for (int i = 0; i < 2; ++i) {
        threads.emplace_back([&stack, i]() {
            for (int j = 0; j < 1000; ++j) {
                stack.push(i * 1000 + j);
            }
        });
    }

    // ... 等待线程结束，再创建pop线程 ...

    for (auto& t : threads) t.join();

    threads.clear();
    std::atomic<int> stackCount{0};
    for (int i = 0; i < 2; ++i) {
        threads.emplace_back([&stack, i, &stackCount]() {
            int result;
            while (stack.pop(result)) {
                std::cout << "====" << result << std::endl;
                stackCount++;
            }
        });
    }

    for (auto& t : threads) t.join();

    std::cout << "stack count: " << stackCount << std::endl;
}

int main() {
    testLockFreeStack();
    return 0;
}