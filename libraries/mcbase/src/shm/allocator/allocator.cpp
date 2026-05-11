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

#include <mc/shm/allocator.h>

#include <cstring>
#include <new>

#include "internal.h"
#include "slab.h"
#include "tlsf.h"

namespace mc::shm {

struct shm_allocator::arena_header : public detail::arena_header {};

namespace {

// 抢锁 + 自动 recover 的 guard
class arena_guard {
public:
    arena_guard(ipc_mutex& m, detail::arena_header* arena, void* base) noexcept
        : m_mutex(&m), m_arena(arena), m_base(base)
    {
        auto state = m_mutex->lock_ex();
        if (state == lock_acquire_state::recovered) {
            detail::tlsf_recover(m_base, m_arena);
            detail::slab_recover(m_base, m_arena);
            detail::journal_end(m_arena->journal);
            // 递增 recover_epoch：告知快路径 in-flight 操作需要回滚
            m_arena->recover_epoch.fetch_add(1, std::memory_order_acq_rel);
        }
    }

    ~arena_guard()
    {
        if (m_mutex != nullptr) {
            m_mutex->unlock();
        }
    }

    arena_guard(const arena_guard&)            = delete;
    arena_guard& operator=(const arena_guard&) = delete;

private:
    ipc_mutex*            m_mutex;
    detail::arena_header* m_arena;
    void*                 m_base;
};

inline std::size_t aligned_arena_header_size() noexcept
{
    return detail::align_up(sizeof(detail::arena_header), detail::tlsf_align);
}

bool is_valid_alignment(std::size_t alignment) noexcept
{
    if (alignment == 0 || alignment > shm_allocator::max_alignment) {
        return false;
    }
    return (alignment & (alignment - 1)) == 0;
}

} // namespace

shm_allocator::shm_allocator() noexcept : m_base(nullptr)
{}

shm_allocator::shm_allocator(void* base, std::size_t /*size*/) noexcept : m_base(base)
{}

bool shm_allocator::initialize(void* base, std::size_t size) noexcept
{
    if (base == nullptr) {
        return false;
    }
    const std::size_t header_size = aligned_arena_header_size();
    if (size <= header_size + detail::tlsf_min_block_size) {
        return false;
    }

    auto* arena = new (base) detail::arena_header{};
    arena->magic   = detail::arena_magic;
    arena->version = detail::arena_version;
    new (&arena->arena_lock) ipc_mutex{};

    arena->arena_size  = size;
    arena->user_offset = header_size;
    arena->user_size   = size - header_size;
    arena->live_bytes.store(0, std::memory_order_relaxed);
    arena->peak_bytes.store(0, std::memory_order_relaxed);

    // journal 置零
    arena->journal.op.store(static_cast<std::uint8_t>(detail::journal_op::none), std::memory_order_relaxed);
    arena->journal.arg0 = 0;
    arena->journal.arg1 = 0;
    arena->journal.arg2 = 0;
    arena->journal.arg3 = 0;
    arena->journal.arg4 = 0;
    arena->journal.misc = 0;

    arena->recover_epoch.store(0, std::memory_order_relaxed);

    if (!detail::tlsf_init(base, arena)) {
        return false;
    }
    if (!detail::slab_init(base, arena)) {
        return false;
    }
    return true;
}

bool shm_allocator::is_valid() const noexcept
{
    const auto* arena = header();
    return arena != nullptr && arena->magic == detail::arena_magic && arena->version == detail::arena_version;
}

void* shm_allocator::allocate(std::size_t size, std::size_t alignment) noexcept
{
    if (!is_valid() || size == 0 || !is_valid_alignment(alignment)) {
        return nullptr;
    }

    auto* arena = header();

    // 快路径：slab CAS pop 不持 arena_lock；小对象无锁并发
    // 前置条件：journal 干净（dirty 意味着有进程崩溃未 recover，必须走慢路径）
    if (detail::journal_load(arena->journal) == detail::journal_op::none) {
        std::uint64_t off = detail::slab_fast_allocate(m_base, arena, size, alignment);
        if (off != 0) {
            return static_cast<std::byte*>(m_base) + off;
        }
    }

    // 慢路径：持 arena_lock 触发 recover / wholesale / TLSF
    arena_guard guard(arena->arena_lock, arena, m_base);
    std::uint64_t off = detail::slab_try_allocate(m_base, arena, size, alignment);
    if (off == 0) {
        off = detail::tlsf_allocate(m_base, arena, size, alignment);
    }
    if (off == 0) {
        return nullptr;
    }
    return static_cast<std::byte*>(m_base) + off;
}

void shm_allocator::deallocate(void* ptr) noexcept
{
    if (!is_valid() || ptr == nullptr) {
        return;
    }
    auto* arena = header();
    auto payload_offset = static_cast<std::uint64_t>(static_cast<std::byte*>(ptr) - static_cast<std::byte*>(m_base));
    if (payload_offset < arena->user_offset || payload_offset >= arena->user_offset + arena->user_size) {
        return;
    }

    if (detail::journal_load(arena->journal) == detail::journal_op::none) {
        if (detail::slab_fast_deallocate(m_base, arena, payload_offset)) {
            return;
        }
    }

    arena_guard guard(arena->arena_lock, arena, m_base);
    if (detail::slab_try_deallocate(m_base, arena, payload_offset)) {
        return;
    }
    detail::tlsf_deallocate(m_base, arena, payload_offset);
}

std::size_t shm_allocator::allocated_size() const noexcept
{
    if (!is_valid()) {
        return 0;
    }
    return static_cast<std::size_t>(header()->live_bytes.load(std::memory_order_acquire));
}

std::size_t shm_allocator::capacity() const noexcept
{
    if (!is_valid()) {
        return 0;
    }
    return static_cast<std::size_t>(header()->user_size);
}

std::size_t shm_allocator::available_size() const noexcept
{
    const auto cap  = capacity();
    const auto used = allocated_size();
    return used > cap ? 0 : cap - used;
}

std::size_t shm_allocator::peak_bytes() const noexcept
{
    if (!is_valid()) {
        return 0;
    }
    return static_cast<std::size_t>(header()->peak_bytes.load(std::memory_order_acquire));
}

integrity_status shm_allocator::check_integrity() const noexcept
{
    if (!is_valid()) {
        return integrity_status::invalid_arena;
    }
    const auto* arena = header();
    // journal 脏标志优先：一旦发现 pending，说明上次修改未 commit，
    // 调用方应先 recover() 再检查
    if (detail::journal_load(arena->journal) != detail::journal_op::none) {
        return integrity_status::journal_pending;
    }
    auto tlsf_status = detail::tlsf_check_integrity(m_base, arena);
    if (tlsf_status != integrity_status::ok) {
        return tlsf_status;
    }
    return detail::slab_check_integrity(m_base, arena);
}

void shm_allocator::recover() noexcept
{
    if (!is_valid()) {
        return;
    }
    auto* arena = header();
    arena_guard guard(arena->arena_lock, arena, m_base);
    // arena_guard 抢锁已自动 recover；这里为显式调用入口留空
}

void shm_allocator::repair() noexcept
{
    if (!is_valid()) {
        return;
    }
    auto* arena = header();
    arena_guard guard(arena->arena_lock, arena, m_base);
    // arena_guard 构造若检测到 recovered 已自动 rebuild；此处显式执行一次 repair 以覆盖
    // "journal 干净但用户主动诉求重建" 场景：两个 recover 内部会在 journal 干净时 early return，
    // 无副作用。
    detail::tlsf_recover(m_base, arena);
    detail::slab_recover(m_base, arena);
    detail::journal_end(arena->journal);
    arena->recover_epoch.fetch_add(1, std::memory_order_acq_rel);
}

shm_allocator::arena_header* shm_allocator::header() const noexcept
{
    return static_cast<arena_header*>(m_base);
}

} // namespace mc::shm
