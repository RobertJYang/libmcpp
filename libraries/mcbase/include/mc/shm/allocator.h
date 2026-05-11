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

#ifndef MC_SHM_ALLOCATOR_H
#define MC_SHM_ALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

#include <mc/common.h>

namespace mc::shm {

// arena 一致性状态，用于崩溃后排障与测试
enum class integrity_status : std::uint32_t {
    ok                = 0,
    invalid_arena     = 1,
    journal_pending   = 2,
    tlsf_inconsistent = 3,
    slab_inconsistent = 4,
    block_corrupted   = 5,
};

class MC_API shm_allocator {
public:
    // 支持的最大对齐（放宽到 page size）
    static constexpr std::size_t max_alignment = 4096;

    shm_allocator() noexcept;
    shm_allocator(void* base, std::size_t size) noexcept;

    static bool initialize(void* base, std::size_t size) noexcept;

    bool        is_valid() const noexcept;
    void*       allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) noexcept;
    void        deallocate(void* ptr) noexcept;

    std::size_t allocated_size() const noexcept;
    std::size_t capacity() const noexcept;
    std::size_t available_size() const noexcept;
    std::size_t peak_bytes() const noexcept;

    // 崩溃恢复相关
    integrity_status check_integrity() const noexcept;
    void             recover() noexcept;
    void             repair() noexcept;

    template <typename T, typename... Args>
    T* construct(Args&&... args) noexcept
    {
        static_assert(alignof(T) <= max_alignment, "shm_allocator supports alignments up to 4096 bytes");

        void* memory = allocate(sizeof(T), alignof(T));
        if (memory == nullptr) {
            return nullptr;
        }

        return new (memory) T(std::forward<Args>(args)...);
    }

    template <typename T>
    void destroy(T* ptr) noexcept
    {
        if (ptr == nullptr) {
            return;
        }

        ptr->~T();
        deallocate(ptr);
    }

private:
    struct arena_header;

    arena_header* header() const noexcept;

    void* m_base;
};

} // namespace mc::shm

#endif // MC_SHM_ALLOCATOR_H
