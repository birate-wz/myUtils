#pragma once

#include <iostream>
#include <atomic>
#include <vector>
#include <memory>
#include <cstddef> 
#include <array>

// 无锁栈实现
template <typename T>
class LockFreeStack {
public:
    LockFreeStack():head(nullptr) {}
    ~LockFreeStack() {
        T dummy;
        while(pop(dummy)) {}
    }

    void push(T item) {
        auto* newNode = new Node(std::move(item));
        Node* oldNode = head.load(std::memory_order_relaxed);
        do{
            newNode->next.store(oldNode, std::memory_order_relaxed);
        } while(!head.compare_exchange_weak(oldNode, newNode, std::memory_order_release, std::memory_order_relaxed));
    }
    bool pop(T& result) {
        Node* oldNode = head.load(std::memory_order_relaxed);
        if (oldNode) {
            if (head.compare_exchange_weak(oldNode, oldNode->next.load(std::memory_order_relaxed), std::memory_order_release, std::memory_order_relaxed)) {
                result = std::move(oldNode->data);
                delete oldNode;
                return true;
            }
        }
        return false;
    }
    bool empty() const {
        return head.load(std::memory_order_relaxed) == nullptr;
    }
private:
    struct Node {
        T data;
        std::atomic<Node*> next;

        template<typename... Args>
        Node(Args&&... args): data(std::forward<Args>(args)...), next(nullptr) {}
    };

    std::atomic<Node*> head;
};

// 无锁固定大小内存池
template <typename T, size_t N = 4096>
class LockFreeFixedSizePool {
private:
    struct alignas(T) Block {
        std::byte data[sizeof(T)]; // 每个block的大小为T
        std::atomic<Block*> next{nullptr};
        T* as_object() { return reinterpret_cast<T*>(data);}
    };

    struct Chunk { // 每个chunk包含N个block
        std::unique_ptr<Block[]> blocks;
        size_t count;
        Chunk(size_t block_count)
            :blocks(std::make_unique<Block[]>(block_count)), count(block_count) {}
    };

    struct ThreadCache {
        Block* blocks[32];
        int count = 0;
        static constexpr size_t BATCH_SIZE = 16; // 批量获取块的数量

        LockFreeFixedSizePool<T, N>* pool_instance = nullptr;
        ~ThreadCache() {
            if (count > 0 && pool_instance) {
                return_thread_cache();
            }
        }
        // 将线程本地缓存归还全局链表
        void return_thread_cache() {
            if (count == 0 || !pool_instance) return;

            //将本地缓存链接成链表
            for (int i = 0; i < count - 1; ++i) {
                blocks[i]->next.store(blocks[i+1], std::memory_order_relaxed);
            }

            Block* old_head = pool_instance->free_list.load(std::memory_order_relaxed);
            Block* new_head = blocks[0];
            Block* new_tail = blocks[count - 1];
            do {
                new_tail->next.store(old_head, std::memory_order_relaxed);
            } while (!pool_instance->free_list.compare_exchange_weak(old_head, new_head, std::memory_order_release, std::memory_order_relaxed));

            count = 0;
        }
    };
    static inline ThreadCache local_cache;

    static inline constexpr size_t BLOCK_PER_CHUNK = N / sizeof(Block);

    std::atomic<Block*> free_list{nullptr};
    LockFreeStack<std::unique_ptr<Chunk>> chunks; 
    std::atomic<size_t> allocate_count{0};
    std::atomic<size_t> deallocated_count{0};

    void allocate_new_chunk() {
        auto chunk = std::make_unique<Chunk>(BLOCK_PER_CHUNK);
        Block* blocks = chunk->blocks.get();

        // 准备链表，只在最后一步进行一次原子操作
        auto* first_block = &blocks[0];
        for (size_t i = 0; i < chunk->count - 1; ++i) {
            blocks[i].next.store(&blocks[i+1], std::memory_order_relaxed);
        }

        Block* old_head = free_list.load(std::memory_order_relaxed);
        do {
            blocks[chunk->count-1].next.store(old_head, std::memory_order_relaxed);
        } while(!free_list.compare_exchange_weak(old_head, first_block, std::memory_order_release, std::memory_order_relaxed));
        //将所有块链接到空闲列表
        chunks.push(std::move(chunk));
    }

    // 批量获取块到本地缓存
    void fill_local_cache(ThreadCache& cache) {
        cache.pool_instance = this;
        // 如果本地缓存还有剩余，则直接返回
        if (cache.count > 0) return;

        // 从全局空闲列表中获取块
        int batch_size = ThreadCache::BATCH_SIZE;
        Block* head = nullptr;
        Block* tail = nullptr;
        Block* new_head = nullptr;

        Block* old_head = free_list.load(std::memory_order_acquire);
        do {
            // 如果全局链表为空，分配新的块
            if (!old_head) {
                allocate_new_chunk();
                old_head = free_list.load(std::memory_order_acquire);
                if (!old_head)  return;  // 如果还是仍然为空，返回
            }

            head = old_head;
            Block* current = old_head;
            int count = 0;
            while (count < batch_size - 1 && current->next.load(std::memory_order_relaxed)) {
                current = current->next.load(std::memory_order_relaxed);
                ++count;
            }
            tail = current;
            new_head = tail->next.load(std::memory_order_relaxed);
        } while(!free_list.compare_exchange_weak(old_head, new_head, std::memory_order_release, std::memory_order_relaxed));

        // 将获取到的块存储到本地缓存
        Block* current = head;
        int i = 0;
        while (current != new_head) {
            cache.blocks[i++] = current;
            current = current->next.load(std::memory_order_relaxed);
        }
        cache.count = i;
    }
public:
    LockFreeFixedSizePool() {
        allocate_new_chunk();
    }

    //禁止拷贝和移动
    LockFreeFixedSizePool(const LockFreeFixedSizePool&) = delete;
    LockFreeFixedSizePool& operator=(const LockFreeFixedSizePool&) = delete;
    LockFreeFixedSizePool(const LockFreeFixedSizePool&&) = delete;
    LockFreeFixedSizePool& operator=(const LockFreeFixedSizePool&&) = delete;

    template<typename... Args>
    T* allocate(Args&&... args) {

        if (local_cache.count == 0) {
            fill_local_cache(local_cache);
        }
        
        Block* block = nullptr;
        // 尝试从本地缓存获取块
        if (local_cache.count > 0) {
            block = local_cache.blocks[--local_cache.count];
        } else {
            // 本地缓存填充失败，直接从全局获取单个块
            Block* old_block = free_list.load(std::memory_order_acquire);
            // 先尝试从空闲列表中获取一个块
            while(old_block) {
                if (free_list.compare_exchange_weak(old_block, old_block->next.load())) {
                    block = old_block;
                    break;
                }
            }

            // 如果空闲列表为空，则分配一个新的块
            if (!block) {
                allocate_new_chunk();
                old_block = free_list.load(std::memory_order_acquire);
                while(old_block) {
                    if (free_list.compare_exchange_weak(old_block, old_block->next.load())) {
                        block = old_block;
                        break;
                    }
                }
            }
        }
       allocate_count.fetch_add(1);
        if (block) {
            // 构造新对象
            return new (block->as_object()) T(std::forward<Args>(args)...);
        }
        return nullptr;
    }

    void deallocate(T* ptr) {
        if (!ptr) return;

        ptr->~T();

        /*
            Block 结构的第一个成员就是 data 数组
            as_object() 方法返回的是 data 数组的起始地址
            通过 alignas(T) 确保 Block 的对齐要求满足 T 的需求
            因此，指向 T 对象的指针实际上就是指向 Block 结构体中 data 数组的指针，而 data 数组又在 Block 的起始位置，所以可以安全地将 T 指针转回 Block 指针。
            内存布局:

            Block 对象:
            +----------------------+
            | data[sizeof(T)]      | <-- 用户获得的 T* 指针指向这里
            +----------------------+
            | next                 |
            +----------------------+
        */
        Block* block = reinterpret_cast<Block*>(ptr);

        local_cache.pool_instance = this;
        // 确定本地缓存容量
        const int cache_capacity = static_cast<int>(sizeof(local_cache.blocks) / sizeof(local_cache.blocks[0]));
        
        // 如果本地缓存已近容量的80%，预先归还一半
        if (local_cache.count >= cache_capacity * 0.8) {
            int half_count = local_cache.count / 2;
            
            // 构建链表
            for (int i = 0; i < half_count - 1; ++i) {
                local_cache.blocks[i]->next.store(local_cache.blocks[i + 1], std::memory_order_relaxed);
            }
            
            // 归还到全局链表
            Block* old_head = free_list.load(std::memory_order_relaxed);
            Block* cache_head = local_cache.blocks[0];
            Block* cache_tail = local_cache.blocks[half_count - 1];
            
            do {
                cache_tail->next.store(old_head, std::memory_order_relaxed);
            } while (!free_list.compare_exchange_weak(old_head, cache_head, 
                                                    std::memory_order_release, 
                                                    std::memory_order_relaxed));
            
            // 压缩剩余缓存
            for (int i = 0; i < local_cache.count - half_count; ++i) {
                local_cache.blocks[i] = local_cache.blocks[i + half_count];
            }
            local_cache.count -= half_count;
        }
        
        // 现在将新块添加到本地缓存
        local_cache.blocks[local_cache.count++] = block;
        deallocated_count.fetch_add(1, std::memory_order_relaxed);
    }

    size_t get_allocated_count() const {
        return allocate_count.load();
    }

    size_t get_deallocated_count() const {
        return deallocated_count.load();
    }

    size_t get_active_objects() const {
        return get_allocated_count() - get_deallocated_count();
    }
};

// RAII 智能指针包装器
template<typename T>
class PoolPtr {
private:
    T* ptr_;
    LockFreeFixedSizePool<T>* pool_;
    
public:
    PoolPtr(T* p, LockFreeFixedSizePool<T>* pool) : ptr_(p), pool_(pool) {}
    
    PoolPtr(const PoolPtr&) = delete;
    PoolPtr& operator=(const PoolPtr&) = delete;
    
    PoolPtr(PoolPtr&& other) noexcept : ptr_(other.ptr_), pool_(other.pool_) {
        other.ptr_ = nullptr;
        other.pool_ = nullptr;
    }
    
    PoolPtr& operator=(PoolPtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            pool_ = other.pool_;
            other.ptr_ = nullptr;
            other.pool_ = nullptr;
        }
        return *this;
    }
    
    ~PoolPtr() {
        reset();
    }
    
    T* get() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    void reset() {
        if (ptr_ && pool_) {
            pool_->deallocate(ptr_);
            ptr_ = nullptr;
        }
    }
    
    T* release() noexcept {
        T* temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }
};

// 便利的创建函数
template<typename T>
class PoolAllocator {
private:
    LockFreeFixedSizePool<T> pool_;
    
public:
    template<typename... Args>
    PoolPtr<T> make(Args&&... args) {
        T* ptr = pool_.allocate(std::forward<Args>(args)...);
        return PoolPtr<T>(ptr, &pool_);
    }
    
    LockFreeFixedSizePool<T>& get_pool() { return pool_; }
};

class LockFreeMultiSizePool {
private:
    static constexpr std::array<size_t, 16> SIZE_CALSSES = {  //max 2k
        8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048
    };

    // 内存结构： FreeBlock + data
    struct FreeBlock {
        size_t size;
        std::atomic<FreeBlock*> next;
        FreeBlock(size_t s):size(s), next(nullptr) {}

        void* data() { // 指向data 数据
            return reinterpret_cast<std::byte*>(this) + sizeof(FreeBlock);
        }

        static FreeBlock* from_data(void* ptr) { // 从data 数据获取FreeBlock指针
            return reinterpret_cast<FreeBlock*>(reinterpret_cast<std::byte*>(ptr) - sizeof(FreeBlock));
        }
    };

    struct ChunkClass {
        std::atomic<FreeBlock*> free_list;
        size_t block_size;  // 每个block size
        std::atomic<size_t> allocated_count;
        std::atomic<size_t> deallocated_count;

        ChunkClass() = default;
        ChunkClass(size_t s):free_list(nullptr), block_size(s) {}
    };

    struct MultiSizeThreadCache {
        struct ClassCache {
            FreeBlock* blocks[16]; // 每个大小类缓存块
            int count;  // 当前缓存的数量
        };
        ClassCache caches[SIZE_CALSSES.size()];
        LockFreeMultiSizePool* pool_ptr; // 指向全局内存池

        ~MultiSizeThreadCache() {

            if (!pool_ptr) return; 
            // 遍历所有大小类
            for (size_t index = 0; index < SIZE_CALSSES.size(); ++index) {
                auto& class_cache = caches[index];
                if (class_cache.count == 0) continue;
                
                // 将块连接成链表
                for (int i = 0; i < class_cache.count - 1; ++i) {
                    class_cache.blocks[i]->next.store(class_cache.blocks[i + 1], std::memory_order_relaxed);
                }
                
                auto& chunk_class = pool_ptr->chunk_classes[index];
                FreeBlock* old_head = chunk_class.free_list.load(std::memory_order_relaxed);
                FreeBlock* cache_head = class_cache.blocks[0];
                FreeBlock* cache_tail = class_cache.blocks[class_cache.count - 1];
                
                // 归还到全局链表
                do {
                    cache_tail->next.store(old_head, std::memory_order_relaxed);
                } while (!chunk_class.free_list.compare_exchange_weak(old_head, cache_head, 
                                                                    std::memory_order_release, 
                                                                    std::memory_order_relaxed));
                
                class_cache.count = 0;
            }
        }

    };

    std::array<ChunkClass, SIZE_CALSSES.size()> chunk_classes;
    LockFreeStack<std::unique_ptr<std::byte[]>> allocated_chunks;

    static constexpr size_t ALIGNMENT = alignof(std::max_align_t);
    static constexpr size_t CHUNK_SIZE = 64 * 1024; // 64k

    static inline thread_local MultiSizeThreadCache thread_cache;
    size_t get_size_class_index(size_t size);
    void allocate_chunk_for_size_class(size_t index);
    bool fill_class_cache(size_t index);

public:
    LockFreeMultiSizePool();
    LockFreeMultiSizePool(const LockFreeMultiSizePool&) = delete;
    LockFreeMultiSizePool& operator=(const LockFreeMultiSizePool&) = delete;
    LockFreeMultiSizePool(const LockFreeMultiSizePool&&) = delete;
    LockFreeMultiSizePool& operator=(const LockFreeMultiSizePool&&) = delete;

    void* allocate(size_t size);

    void deallocate(void* ptr, size_t size);

    void print_stats() const;
};

class MemoryPoolAllocater {
private:
    LockFreeMultiSizePool pool;

public:
    void* allocate(size_t size) {
        return pool.allocate(size);
    }

    void deallocate(void* ptr, size_t size) {
        pool.deallocate(ptr, size);
    }

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* ptr = allocate(sizeof(T));
        if (ptr) {
            return new (ptr) T(std::forward<Args>(args)...);
        }
        return nullptr;
    }

    template<typename T>
    void destroy(T* ptr) {
        if (ptr) {
            ptr->~T();
            deallocate(ptr, sizeof(T));
        }
    }

    void print_stats() const {
        pool.print_stats();
    }
};