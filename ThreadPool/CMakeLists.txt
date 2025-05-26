cmake_minimum_required(VERSION 3.14)

# 单独构建
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(ThreadPool)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

#添加库
add_library(threadPool ${CMAKE_CURRENT_SOURCE_DIR}/threadpool.cpp)
target_include_directories(threadPool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_TESTS)
    add_executable(threadPoolTest ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp)
    target_link_libraries(threadPoolTest PRIVATE threadPool gtest gtest_main)
    include(GoogleTest)
    gtest_discover_tests(threadPoolTest)
endif()