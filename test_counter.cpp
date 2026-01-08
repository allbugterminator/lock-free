#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

void test_performance() {
    const int NUM_THREADS = 4;
    const int ITERATIONS = 1000000;
    
    // 测试1: 使用 fetch_add
    {
        std::atomic<int> counter1(0);
        std::vector<std::thread> threads;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&counter1, ITERATIONS]() {
                for (int j = 0; j < ITERATIONS; ++j) {
                    counter1.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        
        for (auto& t : threads) t.join();
        auto end = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double> duration = end - start;
        std::cout << "fetch_add 耗时: " << duration.count() << "秒, "
                  << "最终值: " << counter1.load() << std::endl;
    }
    
    // 测试2: 使用CAS循环
    {
        std::atomic<int> counter2(0);
        std::vector<std::thread> threads;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&counter2, ITERATIONS]() {
                for (int j = 0; j < ITERATIONS; ++j) {
                    int expected = counter2.load(std::memory_order_relaxed);
                    while (!counter2.compare_exchange_weak(
                        expected, 
                        expected + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed
                    )) {
                        // 循环直到成功
                    }
                }
            });
        }
        
        for (auto& t : threads) t.join();
        auto end = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double> duration = end - start;
        std::cout << "CAS循环 耗时: " << duration.count() << "秒, "
                  << "最终值: " << counter2.load() << std::endl;
    }
}

int main() {
    std::cout << "=== 原子操作性能对比 ===" << std::endl;
    test_performance();
    return 0;
}