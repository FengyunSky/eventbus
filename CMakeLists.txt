cmake_minimum_required(VERSION 3.14)
project(EventBus CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用线程支持（必须）
find_package(Threads REQUIRED)

# 查找 Google Test
find_package(GTest REQUIRED)

# 主库
add_library(EventBus STATIC 
    src/EventBus.cpp
)

# 单元测试可执行文件
add_executable(EventBusTests
    tests/EventBusTests.cpp
)

target_link_libraries(EventBusTests
    PRIVATE EventBus
    PRIVATE GTest::GTest
    PRIVATE GTest::Main
    PRIVATE Threads::Threads
)

# 启用测试
enable_testing()
add_test(NAME EventBusTests COMMAND EventBusTests)

# 可选的示例程序
add_executable(EventBusExample
    examples/Main.cpp
)
target_link_libraries(EventBusExample
    PRIVATE EventBus
    PRIVATE Threads::Threads
)