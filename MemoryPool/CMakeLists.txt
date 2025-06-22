cmake_minimum_required(VERSION 3.14)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(memoryPool)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -march=native -mtune=native -flto -ffast-math -funroll-loops")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

#添加库
add_library(memoryPool ${CMAKE_CURRENT_SOURCE_DIR}/MemoryPool.cpp)
target_include_directories(memoryPool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../Logger ${CMAKE_CURRENT_SOURCE_DIR}/../THreadPool)

if(BUILD_TESTS)
    add_executable(memoryPoolTest ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp)
    target_link_libraries(memoryPoolTest PRIVATE memoryPool logger threadPool gtest gtest_main)
    include(GoogleTest)
    gtest_discover_tests(memoryPoolTest)
endif()
