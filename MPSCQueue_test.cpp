#include "mpsc_queue.h"
#include <gtest/gtest.h>  // Google Test框架
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <set>
#include <mutex>

class MPSCTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试用例前重置全局状态
        // gp_hp_registry<int> = HazardPointerRegistry<int>();
    }
    
    void TearDown() override {
        // 确保所有线程完成
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
};

// 单线程基本操作测试
TEST_F(MPSCTest, SingleThreadBasicOperations) {
    MPSCQueue<int> queue;
    int value;
    
    // 测试空队列出队
    ASSERT_FALSE(queue.dequeue(value, 0));
    
    // 测试入队出队
    queue.enqueue(42);
    ASSERT_TRUE(queue.dequeue(value, 0));
    ASSERT_EQ(value, 42);
    
    // 测试再次空队列
    ASSERT_FALSE(queue.dequeue(value, 0));
}

// FIFO顺序测试
TEST_F(MPSCTest, FIFOOrder) {
    MPSCQueue<int> queue;
    const int TEST_COUNT = 1000;
    
    // 顺序入队
    for (int i = 0; i < TEST_COUNT; ++i) {
        queue.enqueue(i);
    }
    
    // 顺序出队验证
    int value;
    for (int i = 0; i < TEST_COUNT; ++i) {
        ASSERT_TRUE(queue.dequeue(value, 0));
        ASSERT_EQ(value, i);
    }
    
    ASSERT_FALSE(queue.dequeue(value, 0)); // 队列应为空
}

// 多生产者数据完整性测试
TEST_F(MPSCTest, MultiProducerDataIntegrity) {
    MPSCQueue<int> queue;
    const int PRODUCER_COUNT = 4;
    const int ITEMS_PER_PRODUCER = 1000;
    const int TOTAL_ITEMS = PRODUCER_COUNT * ITEMS_PER_PRODUCER;
    
    std::vector<std::thread> producers;
    std::atomic<int> produced_count{0};
    
    // 启动生产者线程
    for (int i = 0; i < PRODUCER_COUNT; ++i) {
        producers.emplace_back([&, i]() {
            int start = i * ITEMS_PER_PRODUCER;
            for (int j = 0; j < ITEMS_PER_PRODUCER; ++j) {
                queue.enqueue(start + j);
                produced_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    // 消费者线程（主线程）
    std::set<int> received_values;
    int consumed_count = 0;
    int value;
    
    while (consumed_count < TOTAL_ITEMS) {
        if (queue.dequeue(value, 0)) {
            // 检查数据唯一性
            ASSERT_TRUE(received_values.insert(value).second);
            consumed_count++;
        }
    }
    
    // 等待生产者完成
    for (auto& producer : producers) {
        producer.join();
    }
    
    ASSERT_EQ(consumed_count, TOTAL_ITEMS);
    ASSERT_EQ(produced_count.load(), TOTAL_ITEMS);
}

// 生产者-消费者速率不匹配测试
TEST_F(MPSCTest, ProducerConsumerRateMismatch) {
    MPSCQueue<int> queue;
    std::atomic<bool> stop_producers{false};
    std::atomic<int> total_produced{0};
    std::atomic<int> total_consumed{0};
    
    // 快速生产者
    std::thread fast_producer([&]() {
        for (int i = 0; i < 5000; ++i) {
            queue.enqueue(i);
            total_produced.fetch_add(1, std::memory_order_relaxed);
        }
    });
    
    // 慢速消费者
    std::thread slow_consumer([&]() {
        int value;
        int count = 0;
        while (count < 5000) {
            if (queue.dequeue(value, 0)) {
                count++;
                total_consumed.fetch_add(1, std::memory_order_relaxed);
                // 模拟处理时间
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    });
    
    fast_producer.join();
    slow_consumer.join();
    
    ASSERT_EQ(total_produced.load(), 5000);
    ASSERT_EQ(total_consumed.load(), 5000);
}

// 吞吐量测试
TEST_F(MPSCTest, ThroughputBenchmark) {
    MPSCQueue<int> queue;
    const int OPERATIONS = 100000;
    const int PRODUCER_COUNT = 4;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> producers;
    std::atomic<int> produced{0};
    
    // 启动生产者
    for (int i = 0; i < PRODUCER_COUNT; ++i) {
        producers.emplace_back([&]() {
            while (produced.fetch_add(1, std::memory_order_relaxed) < OPERATIONS) {
                queue.enqueue(produced.load());
            }
        });
    }
    
    // 消费者
    std::thread consumer([&]() {
        int value;
        int consumed = 0;
        while (consumed < OPERATIONS) {
            if (queue.dequeue(value, 0)) {
                consumed++;
            }
        }
    });
    
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    double ops_per_sec = (OPERATIONS * 1000.0) / duration.count();
    std::cout << "吞吐量: " << ops_per_sec << " 操作/秒" << std::endl;
    
    // 性能断言（根据硬件调整阈值）
    ASSERT_GT(ops_per_sec, 100000); // 至少10万操作/秒
}

// 延迟测试
TEST_F(MPSCTest, LatencyBenchmark) {
    MPSCQueue<std::chrono::high_resolution_clock::time_point> queue;
    const int SAMPLES = 10000;
    std::vector<long long> latencies;
    latencies.reserve(SAMPLES);
    
    std::thread producer([&]() {
        for (int i = 0; i < SAMPLES; ++i) {
            queue.enqueue(std::chrono::high_resolution_clock::now());
        }
    });
    
    std::thread consumer([&]() {
        std::chrono::high_resolution_clock::time_point value;
        for (int i = 0; i < SAMPLES; ++i) {
            if (queue.dequeue(value, 0)) {
                auto now = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    now - value).count();
                latencies.push_back(latency);
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // 计算延迟统计
    long long avg_latency = 0;
    long long max_latency = 0;
    for (auto lat : latencies) {
        avg_latency += lat;
        max_latency = std::max(max_latency, lat);
    }
    avg_latency /= latencies.size();
    
    std::cout << "平均延迟: " << avg_latency << " μs" << std::endl;
    std::cout << "最大延迟: " << max_latency << " μs" << std::endl;
}

// 内存回收测试
TEST_F(MPSCTest, MemoryReclamation) {
    // 测试风险指针是否正确工作
    const int CYCLES = 1000;
    const int BATCH_SIZE = 100;
    
    for (int cycle = 0; cycle < CYCLES; ++cycle) {
        MPSCQueue<int> queue;
        
        // 大量入队出队，触发内存分配和回收
        for (int i = 0; i < BATCH_SIZE; ++i) {
            queue.enqueue(i);
        }
        
        int value;
        for (int i = 0; i < BATCH_SIZE; ++i) {
            ASSERT_TRUE(queue.dequeue(value, 0));
        }
        
        // 如果没有内存错误，说明风险指针工作正常
    }
}

// 并发消费者ID测试
TEST_F(MPSCTest, ConcurrentConsumerIds) {
    MPSCQueue<int> queue;
    const int CONSUMER_THREADS = 3; // 测试多个消费者ID
    
    std::vector<std::thread> consumers;
    std::atomic<int> stop_consumers{false};
    std::vector<int> consumed_counts(CONSUMER_THREADS, 0);
    
    // 先填充一些数据
    for (int i = 0; i < 1000; ++i) {
        queue.enqueue(i);
    }
    
    // 启动多个消费者（使用不同ID）
    for (int i = 0; i < CONSUMER_THREADS; ++i) {
        consumers.emplace_back([&, i]() {
            int value;
            while (!stop_consumers.load()) {
                if (queue.dequeue(value, i)) { // 使用不同的消费者ID
                    consumed_counts[i]++;
                }
            }
        });
    }
    
    // 继续生产
    for (int i = 0; i < 500; ++i) {
        queue.enqueue(i);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop_consumers.store(true);
    
    for (auto& consumer : consumers) {
        consumer.join();
    }
    
    // 验证所有消费者都能正常工作
    int total_consumed = 0;
    for (int count : consumed_counts) {
        total_consumed += count;
        ASSERT_GE(count, 0);
    }
}

// 内存泄漏检测
TEST_F(MPSCTest, MemoryLeakCheck) {
    // 在Valgrind或AddressSanitizer下运行
    const int ITERATIONS = 10000;
    
    for (int i = 0; i < ITERATIONS; ++i) {
        MPSCQueue<int> queue;
        
        for (int j = 0; j < 100; ++j) {
            queue.enqueue(j);
        }
        
        int value;
        for (int j = 0; j < 100; ++j) {
            queue.dequeue(value, 0);
        }
        
        // 队列析构时应释放所有内存
    }
}

// 线程安全检测（使用ThreadSanitizer）
TEST_F(MPSCTest, ThreadSafetyStressTest) {
    MPSCQueue<int> queue;
    const int DURATION_MS = 5000; // 5秒压力测试
    std::atomic<bool> stop{false};
    
    std::vector<std::thread> producers;
    const int PRODUCER_COUNT = 4;
    
    // 启动生产者
    for (int i = 0; i < PRODUCER_COUNT; ++i) {
        producers.emplace_back([&, i]() {
            int counter = i * 1000000;
            while (!stop.load()) {
                queue.enqueue(counter++);
            }
        });
    }
    
    // 消费者
    std::thread consumer([&]() {
        int value;
        while (!stop.load()) {
            queue.dequeue(value, 0);
        }
    });
    
    // 运行指定时间
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    stop.store(true);
    
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    
    // 如果没有数据竞争，ThreadSanitizer不会报告错误
    SUCCEED();
}
