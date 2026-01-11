#!/bin/bash
# run_mpsc_tests.sh

echo "=== MPSC队列完整测试套件 ==="

# 1. 编译测试程序
echo "编译测试程序..."
g++ MPSCQueue_test.cpp -o mpsc_test -std=c++17 -g -O2 -pthread -lgtest -lgtest_main -fsanitize=address

# g++ -std=c++17 -g -O2 -pthread -lgtest -lgtest_main \
#     -fsanitize=thread \
#     MPSCQueue_test.cpp -o mpsc_test

# 2. 运行功能测试
echo "运行功能测试..."
./mpsc_test --gtest_filter="MPSCTest.*"

# 3. 运行性能测试  
echo "运行性能基准测试..."
./mpsc_test --gtest_filter="MPSCTest.ThroughputBenchmark" --gtest_repeat=3

# 4. 运行竞争检测测试（长时间）
echo "运行线程安全压力测试..."
./mpsc_test --gtest_filter="MPSCTest.ThreadSafetyStressTest"

# 5. 生成测试报告
echo "生成测试报告..."
./mpsc_test --gtest_output=xml:test_report.xml

echo "=== 测试完成 ==="