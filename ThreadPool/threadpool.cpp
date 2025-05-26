#include "threadpool.hpp"

ThreadPool::ThreadPool(size_t numThreads): mStart(false), mNumThreads(numThreads)
{
    if (numThreads <= 0) {
        throw std::invalid_argument("numThreads must be positive");
    }
}

ThreadPool::~ThreadPool()
{
    Stop();
}

void ThreadPool::Start()
{
    mStart = true;
    for (size_t i = 0; i < mNumThreads; ++i) {
        mWorks.emplace_back([this] {
            workerThread();
        });
    }
}

void ThreadPool::Stop()
{
    if (mStart) {
        std::unique_lock<std::mutex> lock(mtx);
        mStart = false;
    }
    mCv.notify_all();
    for (auto &work:mWorks) {
        if (work.joinable()) work.join();
    }
}
void ThreadPool::workerThread()
{
    while(mStart) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(mtx);
            if (mQueueTasks.empty() && mStart) {
                mCv.wait(lk, [this] {
                    return !mStart || !mQueueTasks.empty();
                });
            }
            if (!mStart && mQueueTasks.empty()) {
                return; 
            }
            if (mQueueTasks.empty()) {
                continue;
            }
            task = std::move(mQueueTasks.front());
            mQueueTasks.pop();
            task();
        }
        
    }
}


