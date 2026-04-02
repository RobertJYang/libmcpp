/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_MEMORY_FIXED_SIZE_POOL_H
#define MC_MEMORY_FIXED_SIZE_POOL_H

#include <mc/common.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>

namespace mc::memory {

/**
 * @brief 定长对象池（slab：按块分配，块内槽位等长，空闲槽位串联成链表）
 *
 * 仅管理原始存储，不调用类型的构造/析构；调用方负责 placement new 与显式析构。
 *
 * 实现机制：
 * 1. 对象池按固定步长 stride 管理槽位；每次扩容分配一个 slab，slab 是一块连续内存，内部切成多个等长槽位。
 * 2. 空闲槽位分成两层管理：
 *    - 线程本地空闲链：只给当前线程复用，命中时不需要加锁。
 *    - 中心空闲链：保存未被任何线程本地缓存持有的空闲槽位，慢路径下在锁保护中访问。
 * 3. allocate() 先尝试从当前线程的本地空闲链直接取槽位；若本地空闲链未命中，则进入慢路径，
 *    在锁保护下从中心空闲链取槽位；若中心空闲链也为空，则先分配新的 slab，再从新 slab 中取槽位。
 * 4. deallocate() 优先把槽位放回当前线程的本地空闲链；当本地空闲链中的空闲槽位数量超过
 *    local_cache_capacity 时，多出的槽位会被刷回中心空闲链。
 *
 * 5. trim() 不参与快路径；它只处理调用线程本地空闲链和中心空闲链：
 *    - 先把调用线程本地空闲链刷回中心空闲链；
 *    - 再检查哪些 slab 已经完全空闲；
 *    - 只释放那些“整块空闲且释放后仍能保留 keep_free_slots 个中心空闲槽位”的 slab。
 * 6. 因为活跃远端线程的本地空闲链不会被 trim() 直接改写，所以总空闲内存通常由三部分组成：
 *    - 中心空闲链中的槽位；
 *    - 各活跃线程本地空闲链中的槽位；
 *    - 仍包含存活对象、暂时无法整块归还的 partially-used slab 的结构性占用。
 *
 * 注意：
 * 1. 适用于受控线程模型下的共享对象池；同线程 allocate/deallocate 为主要快路径。
 * 2. local_cache_capacity 表示每个活跃线程本地空闲链允许保留的空闲槽位上界；超过该上限的槽位会刷回中心空闲链。
 * 3. trim() 只回收调用线程本地缓存、中心空闲链和完整空 slab，不会直接回收其它活跃线程的本地缓存。
 * 4. 因为活跃远端线程的本地缓存不会被 trim() 直接夺回，所以总空闲内存上界为：
 *    中心空闲槽位 + 每个活跃线程最多 local_cache_capacity 个本地空闲槽位 + partially-used slab 的结构性占用。
 * 5. 若需要最严格的内存占用控制，建议限制线程数，并结合较小的 local_cache_capacity、显式 trim()、
 *    以及稳定的对象生命周期管理共同使用。
 */
class MC_API fixed_size_pool {
public:
    struct options {
        std::size_t initial_slab_slots{64};
        std::size_t max_slab_slots{1024};
        std::size_t max_total_slots{0};
        std::size_t local_cache_capacity{32};
    };

    fixed_size_pool(std::size_t object_size, std::size_t alignment);
    fixed_size_pool(std::size_t object_size, std::size_t alignment, const options& opts);
    ~fixed_size_pool();

    fixed_size_pool(const fixed_size_pool&)            = delete;
    fixed_size_pool& operator=(const fixed_size_pool&) = delete;
    fixed_size_pool(fixed_size_pool&&)                 = delete;
    fixed_size_pool& operator=(fixed_size_pool&&)      = delete;

    void* allocate();

    void deallocate(void* item) noexcept;

    void reserve(std::size_t slot_count);

    /**
     * @brief 回收当前线程本地缓存、中心空闲链中的完整空 slab，并尽量保留 keep_free_slots 个中心空闲槽位
     *
     * 注意：该接口不会直接改写其它活跃线程的本地缓存内容。
     */
    std::size_t trim(std::size_t keep_free_slots = 0);
    std::size_t object_size() const noexcept
    {
        return m_object_size;
    }

    std::size_t slot_stride() const noexcept
    {
        return m_stride;
    }

private:
    struct slab;

    struct slot_header {
        slot_header* next{nullptr};
    };

    struct local_cache {
        fixed_size_pool* owner{nullptr};
        slot_header*     fast_free_list{nullptr};
        std::size_t      fast_free_count{0};
        local_cache*     owner_next{nullptr};
        local_cache*     thread_next{nullptr};
        bool             registered{false};
    };

    struct tls_cache_registry {
        local_cache* head{nullptr};
        ~tls_cache_registry();
    };

    struct cached_local_cache_ref {
        fixed_size_pool* owner{nullptr};
        local_cache*     cache{nullptr};
    };

    mutable std::mutex    m_mutex;
    options               m_options;
    std::size_t           m_object_size{0};
    std::size_t           m_alignment{1};
    std::size_t           m_payload_offset{0};
    std::size_t           m_stride{0};
    std::size_t           m_next_slab_slots{0};
    slot_header*          m_central_free_list{nullptr};
    local_cache*          m_registered_caches{nullptr};
    std::unique_ptr<slab> m_slabs;
    std::size_t           m_total_slots{0};
    std::size_t           m_central_free_slots{0};
    std::size_t           m_local_hard_limit{0};
    bool                  m_has_local_cache{false};

    static tls_cache_registry& tls_registry() noexcept
    {
        static thread_local tls_cache_registry registry;
        return registry;
    }

    static cached_local_cache_ref& tls_last_cache() noexcept
    {
        static thread_local cached_local_cache_ref cache_ref;
        return cache_ref;
    }

    slot_header* slot_from_payload(void* payload) const noexcept
    {
        return static_cast<slot_header*>(payload);
    }

    void* payload_from_slot(slot_header* slot) const noexcept
    {
        return static_cast<void*>(slot);
    }

    std::size_t local_hard_limit() const noexcept
    {
        return m_local_hard_limit;
    }

    local_cache& create_and_register_local_cache(tls_cache_registry& registry);

    local_cache& acquire_local_cache();

    void* allocate_fast(local_cache& cache);

    void deallocate_fast(void* item, local_cache& cache) noexcept;

    void         register_local_cache_locked(local_cache& cache) noexcept;
    void         unregister_local_cache_locked(local_cache& cache) noexcept;
    std::size_t  next_slab_capacity(std::size_t required_slots) const noexcept;
    void         allocate_slab_locked(std::size_t capacity);
    void         reserve_locked(std::size_t free_slots);
    slab*        find_slab_by_slot_locked(const slot_header* slot) noexcept;
    bool         slot_belongs_to_slab(const slot_header* slot, const slab* owner) const noexcept;
    slot_header* pop_central_locked();
    void         push_central_locked(slot_header* slot) noexcept;
    void         rebuild_central_counts_locked() noexcept;
    std::size_t  drain_local_cache_locked(local_cache& cache, std::size_t keep_count) noexcept;
    void         refill_local_cache(local_cache& cache);
    void         flush_local_cache(local_cache& cache, std::size_t keep_count);
    void*        allocate_slow(local_cache& cache);
    void         deallocate_slow(slot_header* slot, local_cache& cache) noexcept;
    void         release_local_cache_on_thread_exit(local_cache& cache) noexcept;
    void         shutdown_registered_caches() noexcept;
};

} // namespace mc::memory

#endif // MC_MEMORY_FIXED_SIZE_POOL_H
