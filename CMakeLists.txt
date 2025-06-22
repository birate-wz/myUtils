cmake_minimum_required(VERSION 3.14)
project(myUtils VERSION 1.0.0 LANGUAGES CXX)

#设置c++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

#设置构建选项
option(BUILD_THREADPOOL "Enable thread pool library" ON)
option(BUILD_LOGGER "Enable logger library " ON)
option(BUILD_MEMORYPOOL "Enable memory pool library" ON)
option(BUILD_COROUTINE "Enable coroutine library" OFF)

#设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

#添加子目录
if(BUILD_THREADPOOL)
    add_subdirectory(ThreadPool)
endif()

if(BUILD_LOGGER)
    add_subdirectory(Logger)
endif()

if(BUILD_MEMORYPOOL)
    add_subdirectory(MemoryPool)
endif()

if(BUILD_COROUTINE)
    add_subdirectory(Coroutine)
endif()

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)
    enable_testing()

    # 获取 Google Test 源码和构建目录
    FetchContent_GetProperties(googletest
        SOURCE_DIR GTEST_SOURCE_DIR
        BINARY_DIR GTEST_BINARY_DIR
    )
    # 输出路径信息（用于调试）
    message(STATUS "Google Test source directory: ${GTEST_SOURCE_DIR}")
    message(STATUS "Google Test binary directory: ${GTEST_BINARY_DIR}")
    message(STATUS "Google Test include directory: ${GTEST_SOURCE_DIR}/googletest/include")
    include_directories(${GTEST_SOURCE_DIR}/googletest/include)
endif()

