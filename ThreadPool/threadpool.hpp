#pragma once

#include <vector>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <iostream>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);

    ~ThreadPool();
    ThreadPool(const ThreadPool& other) = delete;
    ThreadPool& operator=(const ThreadPool& other) = delete;
    template<class F, class... Args>
    auto enqueue(F &&f, Args &&...args)->std::future<typename std::invoke_result_t<F, Args...>>;
    void Stop();
    void Start();
private:
    void workerThread();
    std::vector<std::thread> mWorks;
    std::queue<std::function<void()>> mQueueTasks;
    std::mutex mtx;
    std::condition_variable mCv;
    std::atomic<bool> mStart;
    size_t mNumThreads;
};

template<class F, class...Args>
auto ThreadPool::enqueue(F &&f, Args &&...args)->std::future<typename std::invoke_result_t<F, Args...>>
{
    using return_type = typename std::invoke_result_t<F, Args...>;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        [f = std::forward<F>(f), ... args = std::forward<Args>(args)]() mutable -> return_type {
            return f(args...);
        }
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lk(mtx);
        if (!mStart) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        mQueueTasks.emplace([task]() { (*task)(); });
    }
    mCv.notify_one();
    return res;
}