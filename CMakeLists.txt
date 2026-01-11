cmake_minimum_required(VERSION 3.14)
project(MyUnitTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(
    -g           # 包含调试信息
    -O2          # 优化级别为O2
    -Wall        # 启用大部分警告
    -Wextra      # 启用额外警告
)

# 1. 查找 GoogleTest 包
find_package(GTest REQUIRED)

# 2. 创建你的测试可执行文件
add_executable(mpsc_test MPSCQueue_test.cpp)

# 3. 链接 GoogleTest 库
target_link_libraries(mpsc_test PRIVATE GTest::gtest GTest::gtest_main atomic Threads::Threads)

# 4. 全局链接选项：启用AddressSanitizer
# 注意：链接选项也需要设置-fsanitize=address
add_link_options(-fsanitize=address)