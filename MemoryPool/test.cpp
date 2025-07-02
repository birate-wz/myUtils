#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <atomic>
#include <random>
#include <cstring> 

#include "MemoryPool.hpp"
#include "logger.hpp"
#include "threadpool.hpp"

#include <gtest/gtest.h>

class TestObject {
private:
    int data;
    std::string name;
public:
    TestObject(int value, std::string n): data(value), name(n) {
        LOG_INFO("constructor, value:{}, name:{}", data, name);
    }
    ~TestObject() {
        LOG_INFO("desConstructor, value:{}, name:{}", data, name);
    }

    int getData() const {
        return data;
    }
    std::string getName() const {
        return name;
    }
    void update(int val) {
        data = val;
    }
};

TEST(MemoryPoolTest, basic_test) {
    PoolAllocator<TestObject> pool;

    {
        auto ptr1 = pool.make(100, "test1");
        auto ptr2 = pool.make(200, "test2");
        auto ptr3 = pool.make(300, "test3");

        LOG_INFO("ptr1 value:{}, name:{}", ptr1->getData(), ptr1->getName());
        LOG_INFO("ptr2 value:{}, name:{}", ptr2->getData(), ptr2->getName());
        LOG_INFO("ptr3 value:{}, name:{}", ptr3->getData(), ptr3->getName());

        ptr1->update(1000);
        LOG_INFO("Update ptr1 value:{}, name:{}", ptr1->getData(), ptr1->getName());

        auto ptr4 = std::move(ptr1);
        LOG_INFO("ptr4 value:{}, name:{}", ptr4->getData(), ptr4->getName());  
        LOG_INFO("Pool allocated:{}, deallocated:{}", pool.get_pool().get_allocated_count(), pool.get_pool().get_deallocated_count());
    }
    LOG_INFO("Pool stats - Active objects:{}", pool.get_pool().get_active_objects());
}

TEST(MemoryPoolTest, multiThread) {
    constexpr int operations = 100000;
    int counter = 0;
    ThreadPool pool(std::thread::hardware_concurrency());

    PoolAllocator<int> allocate;
    pool.Start();
    std::vector<std::future<void>> res;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 1000);
    auto thread_id = std::this_thread::get_id();
    std::vector<PoolPtr<int>> ptrs;
    ptrs.reserve(100);
    for (int i = 0; i < operations; ++i) {
        res.emplace_back(pool.enqueue([&]{
            if (ptrs.empty() || dis(gen) > 500) {
                auto ptr = allocate.make(dis(gen));
                ptrs.push_back(std::move(ptr));
            } else {
                size_t index = dis(gen) % ptrs.size();  // 随机释放对象
                ptrs.erase(ptrs.begin() + index);
            }
            counter++;
            std::stringstream ss;
            ss << thread_id;
            std::string id_str = ss.str();
        }));
    }

    for (auto& r : res) {
        r.get();
    }
    pool.Stop();
}

TEST(MemoryPoolTest, multisizePool) {
    MemoryPoolAllocater allocator;

    std::vector<std::pair<void*, size_t>> allocate;
    for (size_t size: {8, 63, 64, 526, 3000, 5000}) {
        void* ptr = allocator.allocate(size);
        if (ptr) {
            std::memset(ptr, static_cast<int>(size), size);
            allocate.emplace_back(ptr, size);   
           // LOG_INFO("allocate memory of size:{} at{}", size, ptr);   
        } else {
            //LOG_ERROR("Failed to allocate memory of size:{}", size);
        }

    }

    auto* ptr1 = allocator.create<TestObject>(1578, "ptr1");
    auto* ptr2 = allocator.create<std::string>("ptr2");

   // LOG_INFO("ptr1 value:{}, name:{}", ptr1->getData(), ptr1->getName());
    //LOG_INFO("ptr2 value:{}", ptr2->c_str());

   // allocator.print_stats();
    for (auto& [ptr, size] : allocate) {
        allocator.deallocate(ptr, size);
    }
    allocator.destroy(ptr1);
    allocator.destroy(ptr2);

    //allocator.print_stats();
}

TEST(MemoryPoolTest, Performance) {
    constexpr int NUM_ALLOCATIONS = 100000;
    
    // 测试标准分配器
    auto test_standard = [&]() {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<int*> ptrs;
        ptrs.reserve(NUM_ALLOCATIONS);
        
        for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
            ptrs.push_back(new int(i));
        }
        
        for (auto* ptr : ptrs) {
            delete ptr;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    };

    // 测试固定大小内存池
    auto test_fix_pool = [&]() {
        auto start = std::chrono::high_resolution_clock::now();
        
        PoolAllocator<int> allocator;
        std::vector<PoolPtr<int>> ptrs;
        ptrs.reserve(NUM_ALLOCATIONS);
        
        for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
            ptrs.push_back(allocator.make(i));
        }
        
        ptrs.clear(); // 自动释放
        
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    };
     // 测试非固定大小内存池
    auto test__multi_pool = [&]() {
        auto start = std::chrono::high_resolution_clock::now();
        MemoryPoolAllocater allocator;
        allocator.reset_global_state();
        std::vector<void*> ptrs;
        ptrs.reserve(NUM_ALLOCATIONS);
        
        for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
            void* ptr = allocator.allocate(i);
            ptrs.push_back(ptr);
        }
        
        for (int i = 0; i < NUM_ALLOCATIONS; ++i)  {
            allocator.deallocate(ptrs[i], i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    };

    LOG_INFO("Standard allocator time: {} microseconds", test_standard().count());
    LOG_INFO("Fixed size pool time: {} microseconds", test_fix_pool().count());
    LOG_INFO("Multi size pool time: {} microseconds", test__multi_pool().count());
}

int main() {
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}