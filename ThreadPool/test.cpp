#include "threadpool.hpp"
#include <iostream>
#include  <gtest/gtest.h>

TEST(ThreadPoolTest, BasicFunctionality) {
    ThreadPool pool(4);

    pool.Start();
    std::atomic<int> counter(0);
    for (int i = 0; i < 100; i++) {
        pool.enqueue([&counter]() {
            counter++;
        });
    }
    pool.Stop();
    EXPECT_EQ(counter, 100);
}

TEST(ThreadPoolTest, Functionality) {
    ThreadPool pool(4);

    pool.Start();
    auto task1 = pool.enqueue([]() {
        return 42;
    });
    auto task2 = pool.enqueue([](int x) {
        return x * 2;
    }, 21);

    EXPECT_EQ(task1.get(), 42);
    EXPECT_EQ(task2.get(), 42);
    pool.Stop();
}

TEST(ThreadPoolTest, ExceptionTest) {
    ThreadPool pool(2);

    pool.Start();
    auto exceptionTask = pool.enqueue([]() {
        throw std::runtime_error("This is a test exception");
        return 0;
    });

    EXPECT_THROW(exceptionTask.get(), std::runtime_error);

    auto normalTask = pool.enqueue([]() {
        return 100;
    });
    EXPECT_EQ(normalTask.get(), 100);
    pool.Stop();
}

TEST(ThreadPoolTest, StressTest) {
    ThreadPool pool(8);
    constexpr int numTasks = 10000;
    pool.Start();
    std::vector<std::future<int>> res;
    for (int i = 0; i < numTasks; i++) {
        res.emplace_back(pool.enqueue([i] {
            return i;
        } ));
    }

    for (int i = 0; i < numTasks; i++) {
        EXPECT_EQ(res[i].get(), i);
    }
    pool.Stop();
}

TEST(ThreadPoolTest, PerformanceTest) {
    constexpr int numTasks = 1000000;

    // single thread
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numTasks; i++) {
        [[maybe_unused]]volatile int x = i * i;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto single_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // thread pool
    ThreadPool pool(std::thread::hardware_concurrency());
    pool.Start();
    auto start1 = std::chrono::high_resolution_clock::now();
    std::vector<std::future<void>> res;
    for (int i = 0; i < numTasks; i++) {
        res.emplace_back(pool.enqueue([i] {
            [[maybe_unused]]volatile int x = i * i;
            }
        ));
    }
    for (int i = 0; i < numTasks; i++) {
        res[i].get();
    }
    pool.Stop();
    auto end1 = std::chrono::high_resolution_clock::now();
    auto pool_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1).count();

    std::cout << "Single thread duration: " << (float)single_duration/1000 << "ms" << std::endl;
    std::cout << "Thread pool duration: " << pool_duration << "ms" << std::endl;
}

TEST(ThreadPoolTest, EdgeTest) {

    // Test with 0 threads
    EXPECT_THROW(ThreadPool(0), std::invalid_argument);

    // The test was submitted before it started. 
    ThreadPool pool(2);
    EXPECT_THROW(pool.enqueue([]() { return 1; }), std::runtime_error);

    // Stop and then restart after testing begins.
    pool.Start();
    auto task = pool.enqueue([]() { return 42; });
    EXPECT_EQ(task.get(), 42);

    pool.Stop();
    pool.Start();  // restart
    
    auto task2 = pool.enqueue([]() { return 100; });
    EXPECT_EQ(task2.get(), 100);
    
    pool.Stop();
}
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}