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
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

namespace mc::memory {

/**
 * @brief 定长对象池（固定跨度 segment/page：块内槽位等长，空闲槽位串联成链表）
 *
 * 仅管理原始存储，不调用类型的构造/析构；调用方负责 placement new 与显式析构。
 *
 * 实现机制：
 * 1. 对象池按固定步长 stride 管理槽位；每个槽位内部布局为“隐藏 header + 对外 payload”，
 *    每次扩容分配一个固定跨度的 segment/page，segment 内部切成多个等长槽位。
 * 2. 空闲槽位分成两层管理：
 *    - 线程本地空闲链：只给当前线程复用，命中时不需要加锁。
 *    - 中心空闲链：保存未被任何线程本地缓存持有的空闲槽位，慢路径下在锁保护中访问。
 * 3. allocate() 先尝试从当前线程的本地空闲链直接取槽位；若本地空闲链未命中，则进入慢路径，
 *    在锁保护下从中心空闲链取槽位；若中心空闲链也为空，则先分配新的 segment，再从新 segment 中取槽位。
 * 4. deallocate() 优先把槽位放回当前线程的本地空闲链；当本地空闲链中的空闲槽位数量超过
 *    local_cache_capacity 时，多出的槽位会被刷回中心空闲链。
 *
 * 5. trim() 不参与快路径；它只处理调用线程本地空闲链和中心空闲链：
 *    - 先把调用线程本地空闲链刷回中心空闲链；
 *    - 再检查哪些 segment 已经完全空闲；
 *    - 只释放那些“整块空闲且释放后仍能保留 keep_free_slots 个中心空闲槽位”的 segment。
 * 6. 因为活跃远端线程的本地空闲链不会被 trim() 直接改写，所以总空闲内存通常由三部分组成：
 *    - 中心空闲链中的槽位；
 *    - 各活跃线程本地空闲链中的槽位；
 *    - 仍包含存活对象、暂时无法整块归还的 partially-used segment 的结构性占用。
 *
 * 注意：
 * 1. 适用于受控线程模型下的共享对象池；同线程 allocate/deallocate 为主要快路径。
 * 2. local_cache_capacity 表示每个活跃线程本地空闲链允许保留的空闲槽位上界；超过该上限的槽位会刷回中心空闲链。
 * 3. trim() 只回收调用线程本地缓存、中心空闲链和完整空 segment，不会直接回收其它活跃线程的本地缓存。
 * 4. 因为活跃远端线程的本地缓存不会被 trim() 直接夺回，所以总空闲内存上界为：
 *    中心空闲槽位 + 每个活跃线程最多 local_cache_capacity 个本地空闲槽位 + partially-used segment 的结构性占用。
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

    struct allocation_result {
        void* ptr{nullptr};
        bool  reused{false};
    };

    struct clear_result {
        std::size_t released_slots{0};
        std::size_t remaining_slots{0};
    };

    fixed_size_pool(std::size_t object_size, std::size_t alignment);
    fixed_size_pool(std::size_t object_size, std::size_t alignment, const options& opts);
    ~fixed_size_pool();

    fixed_size_pool(const fixed_size_pool&)            = delete;
    fixed_size_pool& operator=(const fixed_size_pool&) = delete;
    fixed_size_pool(fixed_size_pool&&)                 = delete;
    fixed_size_pool& operator=(fixed_size_pool&&)      = delete;

    void*             allocate();
    allocation_result allocate_with_status();

    void deallocate(void* item) noexcept;

    void reserve(std::size_t slot_count);

    /**
     * @brief 回收当前线程本地缓存、中心空闲链中的完整空 segment，并尽量保留 keep_free_slots 个中心空闲槽位
     *
     * 注意：该接口不会直接改写其它活跃线程的本地缓存内容。
     */
    std::size_t        trim(std::size_t keep_free_slots = 0);
    clear_result       clear();
    std::vector<void*> snapshot_reused_slots() const;

    /**
     * @brief 在持有池内部互斥下，遍历当前所有「reused」空闲槽位（仅缓存队列），对每个 payload 调用回调。
     *
     * 用于上层在槽位仍挂在 reused 链上时调用析构/清理逻辑；回调不得从该池再分配/释放。
     */
    void for_each_reused_payload_locked(const std::function<void(void*)>& fn);
    std::size_t        object_size() const noexcept
    {
        return m_object_size;
    }

    std::size_t slot_stride() const noexcept
    {
        return m_stride;
    }

private:
    struct segment {
        void*       allocation_base{nullptr};
        std::size_t capacity{0};
        std::size_t central_count{0};
    };

    /**
     * 空闲链节点：next 仅在槽位空闲时使用；segment_index 在槽位生命周期内不变（用于 O(1) 定位所属 segment）。
     */
    struct slot_header {
        slot_header*  next{nullptr};
        std::uint32_t segment_index{0};
    };

    struct local_cache {
        fixed_size_pool* owner{nullptr};
        slot_header*     fast_reused_list{nullptr};
        std::size_t      fast_reused_count{0};
        slot_header*     fast_fresh_list{nullptr};
        std::size_t      fast_fresh_count{0};
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

    mutable std::mutex   m_mutex;
    options              m_options;
    std::size_t          m_object_size{0};
    std::size_t          m_alignment{1};
    std::size_t          m_payload_offset{0};
    std::size_t          m_slot_bytes{0};
    std::size_t          m_stride{0};
    std::size_t          m_segment_slots{0};
    std::size_t          m_next_segment_slots{0};
    slot_header*         m_central_reused_free_list{nullptr};
    slot_header*         m_central_fresh_free_list{nullptr};
    local_cache*         m_registered_caches{nullptr};
    std::vector<segment>     m_segment_table;
    // trim 释放 segment 后回收表项下标，allocate 时 O(1) 复用，避免线性扫描空洞
    std::vector<std::size_t> m_segment_free_indices;
    std::size_t              m_total_slots{0};
    std::size_t          m_central_reused_slots{0};
    std::size_t          m_central_fresh_slots{0};
    std::size_t          m_local_hard_limit{0};
    bool                 m_has_local_cache{false};

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
        return reinterpret_cast<slot_header*>(static_cast<unsigned char*>(payload) - m_payload_offset);
    }

    void* payload_from_slot(slot_header* slot) const noexcept
    {
        return static_cast<void*>(reinterpret_cast<unsigned char*>(slot) + m_payload_offset);
    }

    std::size_t local_hard_limit() const noexcept
    {
        return m_local_hard_limit;
    }

    static std::size_t local_cached_slots(const local_cache& cache) noexcept
    {
        return cache.fast_reused_count + cache.fast_fresh_count;
    }

    std::size_t central_free_slots() const noexcept
    {
        return m_central_reused_slots + m_central_fresh_slots;
    }

    std::size_t acquire_segment_index_locked();

    local_cache& create_and_register_local_cache(tls_cache_registry& registry);

    local_cache& acquire_local_cache();

    void         register_local_cache_locked(local_cache& cache) noexcept;
    void         unregister_local_cache_locked(local_cache& cache) noexcept;
    void         allocate_segment_locked(std::size_t capacity);
    void         reserve_locked(std::size_t free_slots);
    slot_header* pop_central_reused_locked() noexcept;
    slot_header* pop_central_fresh_locked() noexcept;
    void         push_central_reused_locked(slot_header* slot) noexcept;
    void         push_central_fresh_locked(slot_header* slot) noexcept;
    void         rebuild_central_counts_locked() noexcept;
    std::size_t  drain_local_cache_locked(local_cache& cache, std::size_t keep_count) noexcept;
    void         refill_local_cache(local_cache& cache);
    void         flush_local_cache(local_cache& cache, std::size_t keep_count);
    slot_header* acquire_slot_slow(local_cache& cache, bool* reused = nullptr);
    void         deallocate_slow(slot_header* slot, local_cache& cache) noexcept;
    clear_result clear_locked(bool keep_one_empty_slab, std::size_t keep_free_slots);
    void         release_local_cache_on_thread_exit(local_cache& cache) noexcept;
    void         shutdown_registered_caches() noexcept;
};

} // namespace mc::memory

#endif // MC_MEMORY_FIXED_SIZE_POOL_H
