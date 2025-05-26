cmake_minimum_required(VERSION 3.14)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(logger)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

#添加库
add_library(logger ${CMAKE_CURRENT_SOURCE_DIR}/logger.cpp)
target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_TESTS)
    add_executable(loggerTest ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp)
    target_link_libraries(loggerTest PRIVATE logger gtest gtest_main)
    include(GoogleTest)
    gtest_discover_tests(loggerTest)
endif()