
#include "MemoryPool.hpp"
#include "logger.hpp"

constexpr size_t align_of(size_t size, size_t allignment) {
    return (size + allignment -1) & ~(allignment - 1);
}

LockFreeMultiSizePool::LockFreeMultiSizePool() {
    for (size_t i = 0; i < SIZE_CALSSES.size(); ++i) {
        new (&chunk_classes[i]) ChunkClass(SIZE_CALSSES[i]);
    }
    thread_cache.pool_ptr = this;
}

bool LockFreeMultiSizePool::fill_class_cache(size_t index) {
    if (index >= SIZE_CALSSES.size()) return false;

    ChunkClass& chunk_class = chunk_classes[index];
    auto& class_cache = thread_cache.caches[index];

    // 已经有缓存，不需要填充
    if (class_cache.count > 0) return true;

    constexpr int BATCH_SIZE = 8;

    // 尝试从全局链表获取多个块
    FreeBlock* old_head = chunk_class.free_list.load(std::memory_order_acquire);
    if (!old_head) {
        allocate_chunk_for_size_class(index);
        old_head = chunk_class.free_list.load(std::memory_order_acquire);
        if (!old_head) return false;
    }

    //获取一串块
    FreeBlock* current = old_head;
    FreeBlock* new_head = nullptr;

    int count = 0;

    for (int i = 0; i < BATCH_SIZE - 1 && current->next.load(std::memory_order_relaxed); ++i) {
        current = current->next.load(std::memory_order_relaxed);
        ++count;
    }

    new_head = current->next.load(std::memory_order_relaxed);

    // 更新链表头
    if(!chunk_class.free_list.compare_exchange_strong(old_head, new_head, std::memory_order_release, std::memory_order_relaxed)) {
        return false;
    }

    //将获取的块放入本地缓存
    current = old_head;
    while (current != new_head) {
        class_cache.blocks[class_cache.count++] = current;
        current = current->next.load(std::memory_order_relaxed);
    }

    return true;
}

void* LockFreeMultiSizePool::allocate(size_t size) {
    if (size == 0) return nullptr;

    thread_cache.pool_ptr = this; // 设置当前线程的内存池实例
    size_t aligned_size = align_of(size, ALIGNMENT);
    size_t index = get_size_class_index(aligned_size);
    if (index >= SIZE_CALSSES.size()) {  // 分配大对象
        void* ptr = std::aligned_alloc(ALIGNMENT, aligned_size);
        return ptr;
    }

    ChunkClass& chunk_class = chunk_classes[index];
    FreeBlock* old_block = chunk_class.free_list.load(std::memory_order_acquire);
    FreeBlock* block = nullptr;

    // 尝试从本地缓存分配
    auto& class_cache = thread_cache.caches[index];
    if (class_cache.count > 0) {
        FreeBlock* block = class_cache.blocks[--class_cache.count];
        chunk_class.allocated_count.fetch_add(1, std::memory_order_relaxed);
        return block->data(); // 返回数据指针
    }

    // 尝试批量获取块到本地缓存
    if (fill_class_cache(index)) {
        FreeBlock* block = class_cache.blocks[--class_cache.count];
        chunk_class.allocated_count.fetch_add(1, std::memory_order_relaxed);
        return block->data(); // 返回数据指针
    }
    // 尝试从空闲列表获取
    while(old_block) {
        if(chunk_class.free_list.compare_exchange_weak(old_block, old_block->next.load(std::memory_order_relaxed))) {
            block = old_block;
            break;
        }
    }
    if (!block) {
        allocate_chunk_for_size_class(index);

        old_block = chunk_class.free_list.load(std::memory_order_acquire);
        // 再次尝试从空闲列表获取
        while(old_block) {
            if(chunk_class.free_list.compare_exchange_weak(old_block, old_block->next.load(std::memory_order_relaxed))) {
                block = old_block;
                break;
            }
        }
    }

    if(block) {
        chunk_class.allocated_count.fetch_add(1, std::memory_order_relaxed);
        return block->data(); // 返回数据指针
    }
    return nullptr;
}

void LockFreeMultiSizePool::deallocate(void* ptr, size_t size) {
    if (ptr == nullptr) return;

    size_t align_size = align_of(size, ALIGNMENT);
    size_t index = get_size_class_index(align_size);
    if (index >= SIZE_CALSSES.size()) {
        std::free(ptr);  // 大对象直接释放
        return;
    }

    ChunkClass& chunk_class = chunk_classes[index];
    FreeBlock* block_ptr = FreeBlock::from_data(ptr);  // 获取block
    
    // 尝试先放入本地缓存
    auto& class_cache = thread_cache.caches[index];
    if (class_cache.count < static_cast<int>(sizeof(class_cache.blocks) / sizeof(class_cache.blocks[0]))) {
        class_cache.blocks[class_cache.count++] = block_ptr;
    } else {
        // 本地缓存已满，将一半缓存归还全局链表
        int half_count = class_cache.count / 2;

        // 构建链表
        for (int i = 0; i < half_count - 1; ++i) {
            class_cache.blocks[i]->next.store(class_cache.blocks[i + 1], std::memory_order_relaxed);
        }

        // 将新块添加到链表尾部
        class_cache.blocks[half_count - 1]->next.store(block_ptr, std::memory_order_relaxed);

        // 归还给全局链表
        FreeBlock* old_block = chunk_class.free_list.load(std::memory_order_relaxed);
        do {
            block_ptr->next.store(old_block, std::memory_order_relaxed);
        } while(!chunk_class.free_list.compare_exchange_weak(old_block, class_cache.blocks[0], std::memory_order_release, std::memory_order_relaxed));

        // 压缩剩余缓存 更新class_cache
        for (int i = 0; i < class_cache.count - half_count; ++i) {
            class_cache.blocks[i] = class_cache.blocks[i + half_count];
        }
        class_cache.count -= half_count;
    }
    chunk_class.deallocated_count.fetch_add(1, std::memory_order_relaxed);
}

void LockFreeMultiSizePool::print_stats() const {
    LOG_INFO("=== Memory Pool Statistics ===");
    for (size_t i = 0; i < SIZE_CALSSES.size(); ++i) {
        const ChunkClass& chunk_class = chunk_classes[i];
        LOG_INFO("Size class {}: allocated: {}, deallocated: {}", 
            SIZE_CALSSES[i], chunk_class.allocated_count.load(std::memory_order_relaxed), chunk_class.deallocated_count.load(std::memory_order_relaxed));
    }
}

size_t LockFreeMultiSizePool::get_size_class_index(size_t size) {
    // 替换线性搜索为二分查找
    size_t left = 0;
    size_t right = SIZE_CALSSES.size() - 1;
    
    while (left <= right) {
        size_t mid = left + (right - left) / 2;
        if (SIZE_CALSSES[mid] < size)
            left = mid + 1;
        else if (SIZE_CALSSES[mid] > size && mid > 0)
            right = mid - 1;
        else
            return mid;
    }
    
    return (left < SIZE_CALSSES.size()) ? left : SIZE_CALSSES.size();
}

void LockFreeMultiSizePool::allocate_chunk_for_size_class(size_t index) {
    if (index >= SIZE_CALSSES.size()) return; // 分配大对象
    size_t block_size = SIZE_CALSSES[index];
    size_t total_block_size = align_of(sizeof(FreeBlock) + block_size, ALIGNMENT);
    size_t block_count = CHUNK_SIZE / total_block_size;

    if (block_count == 0) block_count = 1;

    size_t actual_chunk_size = total_block_size * block_count;
    auto chunk = std::make_unique<std::byte[]>(actual_chunk_size); // 分配一个chunk
    std::byte* ptr = chunk.get();

    // 将chunk 添加到对应大小的free_list 里面
    ChunkClass& chunk_class = chunk_classes[index];
    for (size_t i = 0; i < block_count; ++i) {
        // 获取每个block
        FreeBlock* block = new(ptr) FreeBlock(block_size);
        ptr += total_block_size;

        // 将block 添加到free_list
        FreeBlock* old_block = chunk_class.free_list.load(std::memory_order_relaxed);
        do {
            block->next.store(old_block, std::memory_order_relaxed);
        } while(!chunk_class.free_list.compare_exchange_weak(old_block, block));
    }
    allocated_chunks.push(std::move(chunk)); // 将chunk 添加到allocated_chunks 管理
}