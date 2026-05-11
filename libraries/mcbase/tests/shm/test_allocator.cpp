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

#include <gtest/gtest.h>

#include <chrono>
#include <csignal>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include <mc/shm/allocator.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/region.h>

#include "shm/allocator/internal.h"
#include "shm/allocator/tlsf.h"

namespace {

struct allocator_payload {
    explicit allocator_payload(int v) : value(v)
    {}

    int value = 0;
};

std::string make_unique_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

TEST(shm_allocator_test, allocate_deallocate_and_reopen)
{
    const auto segment_name = make_unique_name("mc_shm_allocator");
    auto cleanup = [&]() { mc::shm::detail::shared_memory_backend::remove(segment_name); };

    mc::shm::shm_region_options options;
    options.segment_name = segment_name;
    options.total_size = 256 * 1024;

    mc::shm::shm_region region(options);
    ASSERT_TRUE(region.is_valid());
    auto allocator = region.user_arena();
    ASSERT_TRUE(allocator.is_valid());

    const auto before_available = allocator.available_size();
    void* first = allocator.allocate(128);
    void* second = allocator.allocate(256);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_LT(allocator.available_size(), before_available);

    allocator.deallocate(first);
    allocator.deallocate(second);
    EXPECT_EQ(allocator.allocated_size(), 0U);

    auto* payload = allocator.construct<allocator_payload>(99);
    ASSERT_NE(payload, nullptr);
    ASSERT_TRUE(region.upsert_root("payload", region.offset_of(payload), sizeof(*payload)));

    options.open_mode = mc::shm::detail::open_mode::open_existing;
    mc::shm::shm_region reopened(options);
    ASSERT_TRUE(reopened.is_valid());

    auto root = reopened.find_root("payload");
    ASSERT_TRUE(root.has_value());
    auto* reopened_payload = reopened.ptr_from_offset<allocator_payload>(root->offset);
    ASSERT_NE(reopened_payload, nullptr);
    EXPECT_EQ(reopened_payload->value, 99);

    cleanup();
}

class shm_allocator_fixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_segment_name = make_unique_name("mc_shm_allocator_fx");

        mc::shm::shm_region_options options;
        options.segment_name = m_segment_name;
        options.total_size   = 256 * 1024;

        m_region = std::make_unique<mc::shm::shm_region>(options);
        ASSERT_TRUE(m_region->is_valid());
        m_allocator = m_region->user_arena();
        ASSERT_TRUE(m_allocator.is_valid());
    }

    void TearDown() override
    {
        m_region.reset();
        mc::shm::detail::shared_memory_backend::remove(m_segment_name);
    }

    std::string                              m_segment_name;
    std::unique_ptr<mc::shm::shm_region>     m_region;
    mc::shm::shm_allocator                   m_allocator{};
};

TEST_F(shm_allocator_fixture, allocates_various_sizes_with_max_align)
{
    constexpr std::size_t sizes[] = {1, 7, 16, 63, 128, 1023, 4096};
    std::vector<void*>    blocks;
    for (auto s : sizes) {
        auto* p = m_allocator.allocate(s);
        ASSERT_NE(p, nullptr) << "size=" << s;
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % alignof(std::max_align_t), 0U) << "size=" << s;
        std::memset(p, 0xab, s);
        blocks.push_back(p);
    }
    for (auto* p : blocks) {
        m_allocator.deallocate(p);
    }
    EXPECT_EQ(m_allocator.allocated_size(), 0U);
}

TEST_F(shm_allocator_fixture, deallocate_null_and_double_free_are_noop)
{
    m_allocator.deallocate(nullptr);
    auto* p = m_allocator.allocate(64);
    ASSERT_NE(p, nullptr);
    const auto used_after_alloc = m_allocator.allocated_size();
    EXPECT_GT(used_after_alloc, 0U);
    m_allocator.deallocate(p);
    const auto used_after_free = m_allocator.allocated_size();
    EXPECT_LT(used_after_free, used_after_alloc);
    m_allocator.deallocate(p);
    EXPECT_EQ(m_allocator.allocated_size(), used_after_free);
}

TEST_F(shm_allocator_fixture, free_blocks_coalesce_for_larger_reuse)
{
    constexpr std::size_t block_size = 4096;
    auto*                 a          = m_allocator.allocate(block_size);
    auto*                 b          = m_allocator.allocate(block_size);
    auto*                 c          = m_allocator.allocate(block_size);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);
    m_allocator.deallocate(b);
    m_allocator.deallocate(a);
    // a、b 相邻 free-block 合并后应能容纳 1.8 * block_size。
    auto* big = m_allocator.allocate(block_size + block_size / 2);
    EXPECT_NE(big, nullptr);
    if (big != nullptr) {
        m_allocator.deallocate(big);
    }
    m_allocator.deallocate(c);
}

TEST_F(shm_allocator_fixture, construct_destroy_round_trip)
{
    struct holder {
        int           id;
        double        value;
        explicit holder(int i, double v) : id(i), value(v)
        {}
    };
    auto* obj = m_allocator.construct<holder>(7, 3.14);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->id, 7);
    EXPECT_DOUBLE_EQ(obj->value, 3.14);
    m_allocator.destroy(obj);
    EXPECT_EQ(m_allocator.allocated_size(), 0U);
}

TEST_F(shm_allocator_fixture, exhausted_arena_returns_null_without_damage)
{
    std::vector<void*> blocks;
    while (auto* p = m_allocator.allocate(8192)) {
        blocks.push_back(p);
    }
    EXPECT_GT(blocks.size(), 0U);
    EXPECT_EQ(m_allocator.allocate(8192), nullptr);
    EXPECT_EQ(m_allocator.allocate(256 * 1024), nullptr);
    // 释放一个后还能再分配。
    m_allocator.deallocate(blocks.back());
    blocks.pop_back();
    auto* p = m_allocator.allocate(8192);
    EXPECT_NE(p, nullptr);
    if (p != nullptr) {
        m_allocator.deallocate(p);
    }
    for (auto* b : blocks) {
        m_allocator.deallocate(b);
    }
}

TEST_F(shm_allocator_fixture, capacity_and_available_are_consistent)
{
    const auto cap = m_allocator.capacity();
    EXPECT_GT(cap, 0U);
    EXPECT_LE(m_allocator.allocated_size() + m_allocator.available_size(), cap);
    auto* p = m_allocator.allocate(1024);
    ASSERT_NE(p, nullptr);
    EXPECT_LE(m_allocator.allocated_size() + m_allocator.available_size(), cap);
    m_allocator.deallocate(p);
}

// ================ TLSF 单元测试扩展 ================

class shm_allocator_large_fixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_segment_name = make_unique_name("mc_shm_allocator_large");

        mc::shm::shm_region_options options;
        options.segment_name = m_segment_name;
        options.total_size   = 4 * 1024 * 1024;

        m_region = std::make_unique<mc::shm::shm_region>(options);
        ASSERT_TRUE(m_region->is_valid());
        m_allocator = m_region->user_arena();
        ASSERT_TRUE(m_allocator.is_valid());
    }

    void TearDown() override
    {
        m_region.reset();
        mc::shm::detail::shared_memory_backend::remove(m_segment_name);
    }

    std::string                              m_segment_name;
    std::unique_ptr<mc::shm::shm_region>     m_region;
    mc::shm::shm_allocator                   m_allocator{};
};

TEST_F(shm_allocator_large_fixture, many_small_allocs_deterministic)
{
    std::vector<void*> ptrs;
    const std::size_t  count = 2000;
    for (std::size_t i = 0; i < count; ++i) {
        auto* p = m_allocator.allocate(32 + (i % 7) * 8);
        ASSERT_NE(p, nullptr) << "i=" << i;
        ptrs.push_back(p);
    }
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
    for (auto* p : ptrs) {
        m_allocator.deallocate(p);
    }
    EXPECT_EQ(m_allocator.allocated_size(), 0U);
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
}

TEST_F(shm_allocator_large_fixture, mixed_sizes_fuzz_round_trip)
{
    std::mt19937          rng(0xC0FFEE);
    std::vector<void*>    ptrs;
    const std::size_t     iterations = 5000;
    const std::size_t     size_max   = 2048;

    for (std::size_t i = 0; i < iterations; ++i) {
        bool should_alloc = ptrs.empty() || (rng() & 1u);
        if (should_alloc) {
            std::size_t size = 8 + (rng() % size_max);
            auto*       p    = m_allocator.allocate(size);
            if (p != nullptr) {
                std::memset(p, static_cast<int>(i & 0xFF), size);
                ptrs.push_back(p);
            }
        } else {
            std::size_t idx = rng() % ptrs.size();
            std::swap(ptrs[idx], ptrs.back());
            m_allocator.deallocate(ptrs.back());
            ptrs.pop_back();
        }
    }

    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);

    for (auto* p : ptrs) {
        m_allocator.deallocate(p);
    }
    EXPECT_EQ(m_allocator.allocated_size(), 0U);
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
}

TEST_F(shm_allocator_large_fixture, large_object_allocation)
{
    auto* big1 = m_allocator.allocate(256 * 1024);
    ASSERT_NE(big1, nullptr);
    std::memset(big1, 0x5A, 256 * 1024);

    auto* big2 = m_allocator.allocate(512 * 1024);
    ASSERT_NE(big2, nullptr);
    std::memset(big2, 0xA5, 512 * 1024);

    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);

    m_allocator.deallocate(big1);
    m_allocator.deallocate(big2);
    EXPECT_EQ(m_allocator.allocated_size(), 0U);
}

TEST_F(shm_allocator_large_fixture, peak_bytes_tracks_maximum)
{
    EXPECT_EQ(m_allocator.peak_bytes(), 0U);
    std::vector<void*> ptrs;
    for (int i = 0; i < 8; ++i) {
        auto* p = m_allocator.allocate(64 * 1024);
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }
    const auto peak_after_8 = m_allocator.peak_bytes();
    EXPECT_GE(peak_after_8, 8U * 64U * 1024U);

    for (auto* p : ptrs) {
        m_allocator.deallocate(p);
    }
    // peak 不应该随 free 下降
    EXPECT_EQ(m_allocator.peak_bytes(), peak_after_8);
}

TEST_F(shm_allocator_large_fixture, integrity_check_reports_ok_on_clean_arena)
{
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
    auto* p = m_allocator.allocate(1024);
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
    m_allocator.deallocate(p);
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
}

TEST_F(shm_allocator_large_fixture, invalid_alignment_rejected)
{
    EXPECT_EQ(m_allocator.allocate(64, 0), nullptr);
    EXPECT_EQ(m_allocator.allocate(64, 3), nullptr);
    EXPECT_EQ(m_allocator.allocate(64, 8192), nullptr);
}

TEST_F(shm_allocator_large_fixture, zero_size_rejected)
{
    EXPECT_EQ(m_allocator.allocate(0), nullptr);
}

// Phase 4：验证大对齐路径（64B / 256B / 4KB），依赖 tlsf_allocate 的 prefix-split
// 注意：alignment 是 arena 内 offset 对齐（不是绝对地址对齐），跨进程 mmap 到
// 不同虚拟地址时对齐语义保持一致
TEST_F(shm_allocator_large_fixture, large_alignment_allocations)
{
    constexpr std::size_t aligns[] = {64, 256, 1024, 4096};
    constexpr std::size_t sizes[]  = {32, 512, 4096, 64 * 1024};

    auto* user_base = static_cast<std::byte*>(m_region->user_base());

    std::vector<void*> ptrs;
    for (auto a : aligns) {
        for (auto s : sizes) {
            void* p = m_allocator.allocate(s, a);
            ASSERT_NE(p, nullptr) << "align=" << a << " size=" << s;
            auto offset = static_cast<std::uintptr_t>(static_cast<std::byte*>(p) - user_base);
            EXPECT_EQ(offset % a, 0U)
                << "align=" << a << " size=" << s;
            std::memset(p, 0x5A, s);
            ptrs.push_back(p);
        }
    }
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
    for (auto* p : ptrs) {
        m_allocator.deallocate(p);
    }
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
    EXPECT_EQ(m_allocator.allocated_size(), 0U);
}

TEST_F(shm_allocator_large_fixture, page_alignment_round_trip_with_repair)
{
    // 4KB 对齐 + repair 组合：确保大对齐路径在脏 journal / repair 后仍可正常工作
    // 对齐语义为 arena 内 offset 对齐（跨进程稳定）
    auto* user_base = static_cast<std::byte*>(m_region->user_base());
    auto  off_of    = [&](void* p) {
        return static_cast<std::uintptr_t>(static_cast<std::byte*>(p) - user_base);
    };

    void* p1 = m_allocator.allocate(4096, 4096);
    void* p2 = m_allocator.allocate(8192, 4096);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(off_of(p1) % 4096, 0U);
    EXPECT_EQ(off_of(p2) % 4096, 0U);

    m_allocator.repair();
    void* p3 = m_allocator.allocate(16384, 4096);
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(off_of(p3) % 4096, 0U);

    m_allocator.deallocate(p1);
    m_allocator.deallocate(p2);
    m_allocator.deallocate(p3);
    EXPECT_EQ(m_allocator.allocated_size(), 0U);
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
}

// Phase 2 sanity：slab 层骨架存在但默认未激活，所有 size 均通过 TLSF 分配
// （Phase 4 启用 slab 后此测试应继续通过，路由对用户透明）
TEST_F(shm_allocator_large_fixture, small_objects_routed_through_allocator)
{
    constexpr std::size_t small_sizes[] = {1, 8, 16, 24, 32, 48, 64, 96, 128};
    std::vector<void*>    ptrs;
    ptrs.reserve(std::size(small_sizes));
    for (auto s : small_sizes) {
        void* p = m_allocator.allocate(s);
        ASSERT_NE(p, nullptr) << "size=" << s;
        std::memset(p, 0xA5, s);
        ptrs.push_back(p);
    }
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);

    for (auto* p : ptrs) {
        m_allocator.deallocate(p);
    }
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
    EXPECT_EQ(m_allocator.allocated_size(), 0U);
}

// ================ Phase 3 journal/recover 直连测试 ================
//
// 使用独立 buffer 初始化 shm_allocator，直接访问 detail::arena_header 以注入
// 脏 journal，验证 repair() 能够完整重建 free-list、bitmap、live_bytes。

class allocator_journal_fixture : public ::testing::Test {
protected:
    // 4MB：需要容纳 64KB chunk + 64KB alignment padding，以及重复 allocate 用于 rollback 测试
    static constexpr std::size_t buffer_size = 4 * 1024 * 1024;

    void SetUp() override
    {
        m_buffer.resize(buffer_size + 64);
        // 把 buffer 起点对齐到 64B（arena_header 的 alignas）
        auto raw = reinterpret_cast<std::uintptr_t>(m_buffer.data());
        auto aligned = (raw + 63) & ~std::uintptr_t{63};
        m_base   = reinterpret_cast<void*>(aligned);
        m_alloc  = mc::shm::shm_allocator{m_base, buffer_size};
        ASSERT_TRUE(m_alloc.initialize(m_base, buffer_size));
        ASSERT_TRUE(m_alloc.is_valid());
    }

    mc::shm::detail::arena_header* arena_ptr() noexcept
    {
        return static_cast<mc::shm::detail::arena_header*>(m_base);
    }

    std::vector<std::byte>  m_buffer;
    void*                   m_base = nullptr;
    mc::shm::shm_allocator  m_alloc;
};

TEST_F(allocator_journal_fixture, check_integrity_reports_journal_pending_when_dirty)
{
    // 分配/释放若干，确保有正常状态
    auto* p = m_alloc.allocate(128);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::ok);

    // 注入脏 journal
    arena_ptr()->journal.op.store(
        static_cast<std::uint8_t>(mc::shm::detail::journal_op::tlsf_alloc),
        std::memory_order_release);

    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::journal_pending);

    // 关闭 journal，恢复正常
    mc::shm::detail::journal_end(arena_ptr()->journal);
    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::ok);

    m_alloc.deallocate(p);
}

TEST_F(allocator_journal_fixture, repair_rebuilds_free_list_and_clears_journal)
{
    // 分配混合大小，产生多个 free/allocated block 交错
    std::vector<void*> ptrs;
    for (int i = 0; i < 20; ++i) {
        auto* p = m_alloc.allocate(64 + (i % 5) * 16);
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }
    // 释放一半制造碎片
    for (std::size_t i = 0; i < ptrs.size(); i += 2) {
        m_alloc.deallocate(ptrs[i]);
        ptrs[i] = nullptr;
    }

    auto before_live = m_alloc.allocated_size();
    ASSERT_GT(before_live, 0U);

    // 破坏 free-list bitmap + journal dirty，模拟崩溃中间态
    auto* arena = arena_ptr();
    arena->tlsf.fl_bitmap = 0;
    for (std::uint32_t i = 0; i < mc::shm::detail::tlsf_fl_count; ++i) {
        arena->tlsf.sl_bitmap[i] = 0;
        for (std::uint32_t j = 0; j < mc::shm::detail::tlsf_sl_count; ++j) {
            arena->tlsf.free_list[i][j] = 0;
        }
    }
    arena->live_bytes.store(0, std::memory_order_relaxed);
    arena->journal.op.store(
        static_cast<std::uint8_t>(mc::shm::detail::journal_op::tlsf_free),
        std::memory_order_release);

    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::journal_pending);

    m_alloc.repair();

    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::ok);
    EXPECT_EQ(m_alloc.allocated_size(), before_live);

    // 继续正常使用
    auto* after = m_alloc.allocate(256);
    ASSERT_NE(after, nullptr);
    m_alloc.deallocate(after);

    for (auto* p : ptrs) {
        if (p != nullptr) {
            m_alloc.deallocate(p);
        }
    }
    EXPECT_EQ(m_alloc.allocated_size(), 0U);
    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::ok);
}

TEST_F(allocator_journal_fixture, recover_skips_when_journal_clean)
{
    // 干净 journal 下调用 repair 应无副作用
    auto* p = m_alloc.allocate(64);
    ASSERT_NE(p, nullptr);
    auto live_before = m_alloc.allocated_size();

    m_alloc.repair();
    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::ok);
    EXPECT_EQ(m_alloc.allocated_size(), live_before);

    m_alloc.deallocate(p);
}

// 模拟"slab wholesale 已完成 tlsf_allocate、写入 chunk_magic 但 chunks_head 未更新"中间态
// repair 应该把半成品 chunk 回滚给 TLSF，避免 64KB 永久泄漏
TEST_F(allocator_journal_fixture, slab_wholesale_rollback_frees_half_built_chunk)
{
    namespace d = mc::shm::detail;

    auto* arena = arena_ptr();

    // 1) 直接调 detail::tlsf_allocate 拿一个 64KB 对齐块
    //    （公开 API 限制 alignment <= 4096，但 wholesale 路径实际用的就是 detail::tlsf_allocate）
    constexpr std::size_t chunk_sz  = 64 * 1024;
    std::uint64_t         chunk_off = d::tlsf_allocate(m_base, arena, chunk_sz, chunk_sz);
    ASSERT_NE(chunk_off, 0U);

    auto blk_off = d::tlsf_block_offset_from_payload(chunk_off);
    auto bsize   = d::tlsf_block_size(d::ptr_at<d::tlsf_block_header>(m_base, blk_off));

    // 2) 模拟 wholesale 在 "fetch_sub(bsize) 之后 commit 之前" 崩溃：
    //    live_bytes 已扣 bsize、chunk_magic 已写、chunks_head 未更新
    arena->live_bytes.fetch_sub(bsize, std::memory_order_acq_rel);

    auto* chunk              = d::ptr_at<d::slab_chunk_header>(m_base, chunk_off);
    chunk->chunk_magic       = d::slab_chunk_magic;
    chunk->class_id          = 0;
    chunk->slot_size         = arena->slab.classes[0].slot_size;
    chunk->slot_count        = 1;
    chunk->free_count.store(1, std::memory_order_relaxed);
    chunk->slots_base_offset = chunk_off + 1024;
    chunk->next_chunk_offset = 0;

    // 3) 注入 journal = slab_wholesale, chunk_off 不挂链
    ASSERT_EQ(arena->slab.classes[0].chunks_head_offset, 0U)
        << "slab class 0 不应该有其它 chunk 挂在链上";
    arena->journal.arg0 = chunk_off;
    arena->journal.arg1 = bsize;
    arena->journal.arg2 = 0;
    arena->journal.arg3 = 0;
    arena->journal.arg4 = 0;
    arena->journal.misc = 0;
    arena->journal.op.store(static_cast<std::uint8_t>(d::journal_op::slab_wholesale),
                            std::memory_order_release);
    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::journal_pending);

    // 4) repair：slab_recover 应检测到 slab_wholesale 未挂链 → tlsf_deallocate 回滚
    m_alloc.repair();
    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::ok);
    EXPECT_EQ(m_alloc.allocated_size(), 0U);

    // 5) 再分配同样大小应该立即成功，证明 chunk 已归还 TLSF free-list
    std::uint64_t reclaim = d::tlsf_allocate(m_base, arena, chunk_sz, chunk_sz);
    ASSERT_NE(reclaim, 0U);
    d::tlsf_deallocate(m_base, arena, reclaim);
    EXPECT_EQ(m_alloc.allocated_size(), 0U);
    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::ok);
}

// wholesale 在 chunk_magic 尚未写入就崩溃的场景：tlsf_recover 把 chunk 视为普通 allocated block
// slab_recover 从 journal 找到 chunk_off，chunk_magic 不匹配 → 直接 tlsf_deallocate 回滚
TEST_F(allocator_journal_fixture, slab_wholesale_rollback_before_magic_written)
{
    namespace d = mc::shm::detail;

    auto* arena = arena_ptr();

    constexpr std::size_t chunk_sz  = 64 * 1024;
    std::uint64_t         chunk_off = d::tlsf_allocate(m_base, arena, chunk_sz, chunk_sz);
    ASSERT_NE(chunk_off, 0U);

    auto blk_off = d::tlsf_block_offset_from_payload(chunk_off);
    auto bsize   = d::tlsf_block_size(d::ptr_at<d::tlsf_block_header>(m_base, blk_off));

    // 模拟 wholesale 崩溃点：journal 已写，chunk header 未写 magic
    auto* chunk        = d::ptr_at<d::slab_chunk_header>(m_base, chunk_off);
    chunk->chunk_magic = 0;

    arena->journal.arg0 = chunk_off;
    arena->journal.arg1 = bsize;
    arena->journal.misc = 0;
    arena->journal.op.store(static_cast<std::uint8_t>(d::journal_op::slab_wholesale),
                            std::memory_order_release);

    m_alloc.repair();
    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::ok);
    EXPECT_EQ(m_alloc.allocated_size(), 0U);

    std::uint64_t reclaim = d::tlsf_allocate(m_base, arena, chunk_sz, chunk_sz);
    ASSERT_NE(reclaim, 0U);
    d::tlsf_deallocate(m_base, arena, reclaim);
    EXPECT_EQ(m_alloc.allocated_size(), 0U);
}

// wholesale 已挂入 chunks_head 之后崩溃：slab_recover 走正常 bitmap 重建路径，chunk 保留
TEST_F(allocator_journal_fixture, slab_wholesale_dirty_journal_after_commit_is_noop)
{
    namespace d = mc::shm::detail;
    auto* arena = arena_ptr();

    // 正常触发一次 slab 分配 → chunk 已挂链
    void* slot = m_alloc.allocate(16);
    ASSERT_NE(slot, nullptr);
    ASSERT_NE(arena->slab.classes[0].chunks_head_offset, 0U);

    auto live_before = m_alloc.allocated_size();
    auto chunk_off   = arena->slab.classes[0].chunks_head_offset;
    auto blk_off     = d::tlsf_block_offset_from_payload(chunk_off);
    auto bsize       = d::tlsf_block_size(d::ptr_at<d::tlsf_block_header>(m_base, blk_off));

    // 人为把 journal 置脏为 "slab_wholesale"（模拟 commit 后尚未 journal_end 就崩溃）
    arena->journal.arg0 = chunk_off;
    arena->journal.arg1 = bsize;
    arena->journal.misc = 0;
    arena->journal.op.store(static_cast<std::uint8_t>(d::journal_op::slab_wholesale),
                            std::memory_order_release);

    m_alloc.repair();
    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::ok);
    EXPECT_EQ(m_alloc.allocated_size(), live_before);

    m_alloc.deallocate(slot);
    EXPECT_EQ(m_alloc.allocated_size(), 0U);
}

// ================ Phase 5 崩溃注入测试 ================
//
// 使用 shm_region 创建跨进程共享段，fork 子进程模拟"持锁时崩溃 + journal 脏"，
// 验证父进程通过 arena_guard 自动触发 ipc_mutex::recovered → tlsf_recover 修复。

class shm_allocator_crash_fixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_segment_name = make_unique_name("mc_shm_allocator_crash");
        mc::shm::shm_region_options options;
        options.segment_name = m_segment_name;
        options.total_size   = 2 * 1024 * 1024;
        m_region             = std::make_unique<mc::shm::shm_region>(options);
        ASSERT_TRUE(m_region->is_valid());
    }

    void TearDown() override
    {
        m_region.reset();
        mc::shm::detail::shared_memory_backend::remove(m_segment_name);
    }

    mc::shm::detail::arena_header* arena_ptr() noexcept
    {
        return static_cast<mc::shm::detail::arena_header*>(m_region->user_base());
    }

    std::string                          m_segment_name;
    std::unique_ptr<mc::shm::shm_region> m_region;
};

TEST_F(shm_allocator_crash_fixture, child_crash_with_dirty_journal_recovers_on_parent_access)
{
    // Pre-populate：父进程先做一些正常分配
    auto allocator = m_region->user_arena();
    std::vector<void*> survivors;
    for (int i = 0; i < 10; ++i) {
        auto* p = allocator.allocate(128 + (i % 4) * 32);
        ASSERT_NE(p, nullptr);
        survivors.push_back(p);
    }
    // 释放一半制造碎片
    for (std::size_t i = 0; i < survivors.size(); i += 2) {
        allocator.deallocate(survivors[i]);
        survivors[i] = nullptr;
    }
    EXPECT_EQ(allocator.check_integrity(), mc::shm::integrity_status::ok);
    const auto live_before_crash = allocator.allocated_size();
    ASSERT_GT(live_before_crash, 0U);

    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        // 子进程：持有 arena_lock，注入脏 journal + 破坏 free-list/bitmap，然后 SIGKILL 自己
        auto* arena = arena_ptr();
        arena->arena_lock.lock_ex();
        arena->journal.op.store(
            static_cast<std::uint8_t>(mc::shm::detail::journal_op::tlsf_alloc),
            std::memory_order_release);
        // 破坏 bitmap / live_bytes 模拟崩溃中间态
        arena->tlsf.fl_bitmap = 0;
        for (std::uint32_t i = 0; i < mc::shm::detail::tlsf_fl_count; ++i) {
            arena->tlsf.sl_bitmap[i] = 0;
        }
        arena->live_bytes.store(0, std::memory_order_relaxed);
        // 不 unlock，直接自杀（模拟 SIGKILL）
        ::raise(SIGKILL);
        ::_exit(0);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status));
    EXPECT_EQ(WTERMSIG(status), SIGKILL);

    // 父进程：继续调 allocator.allocate()，arena_guard 会发现 lock 已 recovered，
    // 触发 tlsf_recover 完整 repair，整个过程对用户透明
    auto* p_after = allocator.allocate(64);
    ASSERT_NE(p_after, nullptr);
    EXPECT_EQ(allocator.check_integrity(), mc::shm::integrity_status::ok);

    // repair 后 live_bytes 应恢复为 "crash 前的 live_bytes + 本次 allocate 的 block"
    EXPECT_GT(allocator.allocated_size(), live_before_crash);

    allocator.deallocate(p_after);
    for (auto* p : survivors) {
        if (p != nullptr) {
            allocator.deallocate(p);
        }
    }
    EXPECT_EQ(allocator.allocated_size(), 0U);
    EXPECT_EQ(allocator.check_integrity(), mc::shm::integrity_status::ok);
}

TEST_F(shm_allocator_crash_fixture, repeated_child_crashes_still_converge)
{
    // 连续 10 轮 child crash，验证 arena 始终可修复且可继续分配
    auto allocator = m_region->user_arena();
    for (int round = 0; round < 10; ++round) {
        auto* p = allocator.allocate(256);
        ASSERT_NE(p, nullptr) << "round=" << round;

        pid_t pid = fork();
        ASSERT_NE(pid, -1);
        if (pid == 0) {
            auto* arena = arena_ptr();
            arena->arena_lock.lock_ex();
            arena->journal.op.store(
                static_cast<std::uint8_t>(mc::shm::detail::journal_op::tlsf_free),
                std::memory_order_release);
            ::raise(SIGKILL);
            ::_exit(0);
        }
        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);

        auto* q = allocator.allocate(128);
        ASSERT_NE(q, nullptr) << "round=" << round;
        EXPECT_EQ(allocator.check_integrity(), mc::shm::integrity_status::ok)
            << "round=" << round;

        allocator.deallocate(p);
        allocator.deallocate(q);
    }
    EXPECT_EQ(allocator.allocated_size(), 0U);
}

// Phase 5 integrity 检测：手动破坏 block header → check_integrity 应返回 block_corrupted
// ================ slab 激活后专项测试 ================

// slab 路径激活后，小对象应该走 slab chunk（不经过 TLSF header），整体 live_bytes 统计准确
TEST_F(shm_allocator_large_fixture, slab_small_object_live_bytes_accounting)
{
    constexpr std::size_t class_size = 64;
    constexpr std::size_t count      = 200;
    std::vector<void*>    ptrs;
    ptrs.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto* p = m_allocator.allocate(class_size);
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }
    // slab 路径下 live_bytes 精确等于 class_size * count（不包含 chunk overhead）
    EXPECT_EQ(m_allocator.allocated_size(), class_size * count);
    for (auto* p : ptrs) {
        m_allocator.deallocate(p);
    }
    EXPECT_EQ(m_allocator.allocated_size(), 0U);
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
}

// slab 路径 chunk 在 arena 内 offset 必须对齐到 slab_chunk_size（O(1) 反查的前提）
// 跨进程 mmap 的绝对地址不同，但 arena-offset 对齐是稳定不变量
TEST_F(shm_allocator_large_fixture, slab_chunks_aligned_to_chunk_size)
{
    std::vector<void*> ptrs;
    for (int i = 0; i < 100; ++i) {
        auto* p = m_allocator.allocate(32);
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }
    auto* base = static_cast<std::byte*>(m_region->user_base());
    auto* arena = reinterpret_cast<mc::shm::detail::arena_header*>(base);
    for (std::uint32_t i = 0; i < arena->slab.class_count; ++i) {
        auto off = arena->slab.classes[i].chunks_head_offset;
        while (off != 0) {
            EXPECT_EQ(off % mc::shm::detail::slab_chunk_size, 0U)
                << "class=" << i << " chunk_off=" << off;
            auto* chunk = reinterpret_cast<mc::shm::detail::slab_chunk_header*>(base + off);
            off = chunk->next_chunk_offset;
        }
    }
    for (auto* p : ptrs) {
        m_allocator.deallocate(p);
    }
}

// slab slot 的 double-free 应被 bitmap 检测并 noop
TEST_F(shm_allocator_large_fixture, slab_double_free_is_detected_and_noop)
{
    auto* p = m_allocator.allocate(64);
    ASSERT_NE(p, nullptr);
    const auto live_after_alloc = m_allocator.allocated_size();
    EXPECT_GT(live_after_alloc, 0U);

    m_allocator.deallocate(p);
    const auto live_after_free = m_allocator.allocated_size();
    EXPECT_LT(live_after_free, live_after_alloc);

    // 重复 free 不应再次扣减 live_bytes，也不应破坏 free-list
    m_allocator.deallocate(p);
    m_allocator.deallocate(p);
    EXPECT_EQ(m_allocator.allocated_size(), live_after_free);

    // double-free 后仍能正常分配/释放
    auto* q = m_allocator.allocate(64);
    ASSERT_NE(q, nullptr);
    m_allocator.deallocate(q);
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
}

// 大量小对象的生命周期压力测试（slab 核心价值场景）
TEST_F(shm_allocator_large_fixture, slab_heavy_small_object_lifecycle)
{
    constexpr std::size_t N = 4000;
    std::vector<void*>    ptrs(N, nullptr);
    std::mt19937          rng(20260417);
    std::uniform_int_distribution<int> size_dist(1, 128);

    for (std::size_t i = 0; i < N; ++i) {
        ptrs[i] = m_allocator.allocate(static_cast<std::size_t>(size_dist(rng)));
        ASSERT_NE(ptrs[i], nullptr) << "i=" << i;
    }
    // 乱序释放一半
    std::vector<std::size_t> order(N);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);
    for (std::size_t k = 0; k < N / 2; ++k) {
        m_allocator.deallocate(ptrs[order[k]]);
        ptrs[order[k]] = nullptr;
    }
    // 再分配同样数量
    for (std::size_t k = 0; k < N / 2; ++k) {
        auto* q = m_allocator.allocate(static_cast<std::size_t>(size_dist(rng)));
        ASSERT_NE(q, nullptr);
        ptrs[order[k]] = q;
    }
    // 全部释放
    for (auto* p : ptrs) {
        if (p != nullptr) {
            m_allocator.deallocate(p);
        }
    }
    EXPECT_EQ(m_allocator.allocated_size(), 0U);
    EXPECT_EQ(m_allocator.check_integrity(), mc::shm::integrity_status::ok);
}

TEST_F(allocator_journal_fixture, integrity_detects_corrupted_block_header)
{
    auto* p = m_alloc.allocate(128);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::ok);

    // 破坏首个 block 的 size_and_flags 为非法小值
    auto* arena = arena_ptr();
    auto* first_blk = reinterpret_cast<mc::shm::detail::tlsf_block_header*>(
        static_cast<std::byte*>(m_base) + arena->user_offset);
    const auto saved = first_blk->size_and_flags;
    first_blk->size_and_flags = 8;

    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::block_corrupted);

    first_blk->size_and_flags = saved;
    EXPECT_EQ(m_alloc.check_integrity(), mc::shm::integrity_status::ok);
    m_alloc.deallocate(p);
}

} // namespace
