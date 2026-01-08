#include <thread>
#include <vector>
#include <iostream>
#include "stack.h"
#include "spsc_queue.h"

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

void testSpscQueue() {
    SPSCQueue<int> spscQueue(1000);
    std::vector<std::thread> threads;
    std::atomic<bool> isStop(false);
    threads.emplace_back([&spscQueue, &isStop]() {
        for (int j = 0; j < 1000; ++j) {
            while (!spscQueue.enqueue(j)) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            // std::cout << "enqueue item: " << j << std::endl;
        }

        isStop = true;
    });

    std::atomic<int> counter(0);
    threads.emplace_back([&spscQueue, &counter, &isStop]() {
        while(1) {
            int res = -1;
            while (!spscQueue.dequeue(res)) {
                if (isStop) {
                    break;
                }
            }

            std::cout << "dequeue item: " << res << std::endl;
            if (res == -1) {
                break;
            }

            counter ++;
        }
    });

    for (auto& t : threads) t.join();

    std::cout << "dqueue count: " << counter << std::endl;
}

int main() {
    // testLockFreeStack();
    testSpscQueue();
    return 0;
}