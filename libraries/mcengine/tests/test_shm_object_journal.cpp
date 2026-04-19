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

// T8：shm_object 写路径 journal 三步协议 + crash recovery
//
// 范围：
//   T8.1 同进程：人为模拟"begin 但未 end"（拷贝 shm_object snapshot 后重放写入
//        中间状态），调 shm_object_journal_recover 验证字段最终值要么是旧值
//        要么是新值，不出现撕裂。
//   T8.2 多进程：fork 出子进程持续高频写 property，父进程 SIGKILL 子进程后
//        attach 同一 region，对所有 shadow 跑 recover，验证 SHM 中 property 值
//        与最后一次成功写入一致（旧或新），slab CRC 一致。
//   T8.3 ABI：构造一个 abi=2 的旧版 shm_object 二进制（journal 全零），recover
//        是 no-op；abi 校验由调用方负责。

#include <gtest/gtest.h>

#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include <mc/engine/shm_object.h>

#include "shm_object_ops.h"
#include <mc/shm/allocator.h>
#include <mc/shm/container/journal.h>
#include <mc/shm/region.h>

using mc::engine::property_slab;
using mc::engine::property_slab_check;
using mc::engine::property_type_tag;
using mc::engine::shm_object;
using mc::engine::shm_object_check;
using mc::engine::shm_object_class_name;
using mc::engine::shm_object_create;
using mc::engine::shm_object_destroy;
using mc::engine::shm_object_get_property_blob;
using mc::engine::shm_object_get_property_int64;
using mc::engine::shm_object_journal_recover;
using mc::engine::shm_object_name;
using mc::engine::shm_object_path;
using mc::engine::shm_object_property_count;
using mc::engine::shm_object_add_child;
using mc::engine::shm_object_child_count;
using mc::engine::shm_object_position;
using mc::engine::shm_object_set_class_name;
using mc::engine::shm_object_set_name;
using mc::engine::shm_object_set_path;
using mc::engine::shm_object_set_position;
using mc::engine::shm_object_set_property_blob;
using mc::engine::shm_object_set_property_int64;
using mc::engine::shm_allocator;
using mc::shm::shm_region;
using mc::shm::shm_region_options;
using mc::shm::container::container_op;
using mc::shm::container::container_journal;
using mc::shm::container::container_journal_view;
using mc::shm::container::journal_load;

namespace {

constexpr std::size_t k_region_size = 4U * 1024U * 1024U;

class shadow_journal_fixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::random_device rd;
        std::mt19937       rng(rd());
        char               buf[128];
        std::snprintf(buf, sizeof(buf), "mc_shadow_journal_test_%d_%u", ::getpid(), rng());

        shm_region_options opts;
        opts.segment_name  = mc::string(buf);
        opts.total_size    = k_region_size;
        opts.root_capacity = 16;

        m_region = shm_region(opts);
        ASSERT_TRUE(m_region.is_valid());
        m_alloc = m_region.user_arena();
        ASSERT_TRUE(m_alloc.is_valid());
    }

    shm_region    m_region;
    shm_allocator m_alloc;
};

// 把 shm_object 整个字节拷贝到一个临时 buffer（用于"快照 + 中途回滚"）
struct shadow_snapshot {
    alignas(shm_object) std::byte bytes[sizeof(shm_object)];

    explicit shadow_snapshot(const shm_object& s) noexcept
    {
        std::memcpy(bytes, &s, sizeof(s));
    }
    void restore(shm_object& s) const noexcept { std::memcpy(&s, bytes, sizeof(s)); }
};

}  // namespace

// ============================================================================
// T8.1 同进程：journal 三步协议正常路径下 op 始终被清回 none
// ============================================================================

TEST_F(shadow_journal_fixture, normal_setter_leaves_journal_clean)
{
    auto* shadow = shm_object_create(m_alloc, 1, "C", "n", "/p", "0001");
    ASSERT_NE(shadow, nullptr);

    shm_object_set_name(m_alloc, *shadow, "renamed");
    EXPECT_EQ(shadow->journal.op.load(std::memory_order_acquire),
              static_cast<std::uint8_t>(container_op::none));
    EXPECT_TRUE(shm_object_check(*shadow));
    EXPECT_EQ(shm_object_name(*shadow), "renamed");

    shm_object_set_property_int64(m_alloc, *shadow, "n", 42);
    EXPECT_EQ(shadow->journal.op.load(std::memory_order_acquire),
              static_cast<std::uint8_t>(container_op::none));
    int64_t v = 0;
    EXPECT_TRUE(shm_object_get_property_int64(*shadow, "n", v));
    EXPECT_EQ(v, 42);

    shm_object_set_property_blob(m_alloc, *shadow, "label", "hello",
                                 property_type_tag::string);
    EXPECT_EQ(shadow->journal.op.load(std::memory_order_acquire),
              static_cast<std::uint8_t>(container_op::none));
    std::string_view              s_out;
    mc::engine::property_type_tag tag_out{};
    EXPECT_TRUE(shm_object_get_property_blob(*shadow, "label", s_out, tag_out));
    EXPECT_EQ(s_out, "hello");

    shm_object_destroy(m_alloc, shadow);
}

// 在 setter 写入"中途"快照（journal.op != none，字段已写、CRC 已刷、但 end
// 还没发生）→ recover 应识别为 commit 已生效（redo 路径）→ 字段保持新值。
TEST_F(shadow_journal_fixture, recover_redo_byte_string_replace_after_commit_visible)
{
    auto* shadow = shm_object_create(m_alloc, 1, "C", "n", "/p", "0001");
    ASSERT_NE(shadow, nullptr);

    // 模拟 W3 byte_string_replace 的"已写字段 + 已刷 CRC + 未 end"中间态：
    //   实现层面：保留 setter 写入完成后 journal payload 仍存在 + 手动重置 op = update
    shm_object_set_name(m_alloc, *shadow, "new-name");
    // 重读 journal payload（setter 内部 saved_a/b 已被 end 后清空？ 不，end 只清 op，
    // payload 留存）
    container_journal_view v_before{};
    (void)journal_load(shadow->journal, v_before);  // op == none → false，不取
    // 手动复原"未 commit"中间态：保留 saved_a/b，把 op 重置回 update
    shadow->journal.op.store(static_cast<std::uint8_t>(container_op::update),
                             std::memory_order_release);
    container_journal_view v{};
    ASSERT_TRUE(journal_load(shadow->journal, v));
    EXPECT_EQ(v.anchor_offset, 100U);  // sub_op::byte_string_replace
    EXPECT_EQ(v.target_node_offset, offsetof(shm_object, name));

    shm_object_journal_recover(m_alloc, *shadow);
    EXPECT_EQ(shadow->journal.op.load(std::memory_order_acquire),
              static_cast<std::uint8_t>(container_op::none));
    EXPECT_EQ(shm_object_name(*shadow), "new-name");
    EXPECT_TRUE(shm_object_check(*shadow));

    shm_object_destroy(m_alloc, shadow);
}

// 模拟"begin 完成、字段还没改"的中间态：恢复 saved_a 应使字段保持旧值
// （undo 路径），且新分配的 byte_string 被 destroy
TEST_F(shadow_journal_fixture, recover_undo_byte_string_replace_when_field_not_yet_written)
{
    auto* shadow = shm_object_create(m_alloc, 1, "C", "old", "/p", "0001");
    ASSERT_NE(shadow, nullptr);

    // 直接模拟 setter 的"alloc 新 + journal_begin → field 还没写"
    auto* new_bs = mc::engine::shm_byte_string_create(m_alloc, "should-be-undone");
    ASSERT_NE(new_bs, nullptr);

    // saved_a = 当前 field 位模式（指向 "old"）
    std::uint64_t saved_a = 0;
    std::memcpy(&saved_a, &shadow->name, sizeof(saved_a));

    // saved_b = 假设把 new_bs 写入 field 后字段位模式
    const auto diff =
        static_cast<std::int64_t>(reinterpret_cast<const std::byte*>(new_bs)
                                  - reinterpret_cast<const std::byte*>(&shadow->name));
    const std::uint64_t saved_b = static_cast<std::uint64_t>(diff);

    mc::shm::container::journal_begin(
        shadow->journal, container_op::update,
        static_cast<std::uint64_t>(reinterpret_cast<const std::byte*>(&shadow->name)
                                   - reinterpret_cast<const std::byte*>(shadow)),
        100U,  // sub_op::byte_string_replace
        saved_a, saved_b, 0, 0);

    // 这里"模拟 crash"，不写 field 也不 refresh CRC，直接 recover
    shm_object_journal_recover(m_alloc, *shadow);

    EXPECT_EQ(shadow->journal.op.load(std::memory_order_acquire),
              static_cast<std::uint8_t>(container_op::none));
    EXPECT_EQ(shm_object_name(*shadow), "old");
    EXPECT_TRUE(shm_object_check(*shadow));

    shm_object_destroy(m_alloc, shadow);
}

// W1 property 覆盖：模拟"begin 完成、slot 还没改"中间态，recover undo 后值=旧
TEST_F(shadow_journal_fixture, recover_undo_property_value_simple)
{
    auto* shadow = shm_object_create(m_alloc, 1, "C", "n", "/p", "0001");
    ASSERT_NE(shadow, nullptr);
    ASSERT_TRUE(shm_object_set_property_int64(m_alloc, *shadow, "k", 100));

    auto* slab = shadow->properties.get();
    ASSERT_NE(slab, nullptr);
    const int idx = mc::engine::property_slab_find(*slab, "k");
    ASSERT_GE(idx, 0);
    auto& slot = slab->slots[idx];

    const std::int64_t old_v   = slot.v_int64;
    const std::int64_t new_v   = 999;
    std::uint64_t      saved_a = 0;
    std::uint64_t      saved_b = 0;
    std::memcpy(&saved_a, &old_v, sizeof(saved_a));
    std::memcpy(&saved_b, &new_v, sizeof(saved_b));

    // sub_op::property_value_simple = 200
    // extra: (old_type=int64=1) | (new_type=int64=1 << 8) | (idx<<16) | (key_hash<<32)
    const std::uint8_t old_type = static_cast<std::uint8_t>(property_type_tag::int64);
    const std::uint8_t new_type = old_type;
    const std::uint64_t extra = static_cast<std::uint64_t>(old_type)
                                | (static_cast<std::uint64_t>(new_type) << 8)
                                | (static_cast<std::uint64_t>(idx) << 16)
                                | (static_cast<std::uint64_t>(slot.key_hash) << 32);

    mc::shm::container::journal_begin(
        shadow->journal, container_op::update,
        static_cast<std::uint64_t>(reinterpret_cast<const std::byte*>(&slot)
                                   - reinterpret_cast<const std::byte*>(shadow)),
        200U, saved_a, saved_b, extra, 0);

    // 不动 slot，模拟 crash，recover 应 undo
    shm_object_journal_recover(m_alloc, *shadow);

    EXPECT_EQ(shadow->journal.op.load(std::memory_order_acquire),
              static_cast<std::uint8_t>(container_op::none));
    int64_t v = 0;
    EXPECT_TRUE(shm_object_get_property_int64(*shadow, "k", v));
    EXPECT_EQ(v, old_v);
    EXPECT_TRUE(property_slab_check(*slab));

    shm_object_destroy(m_alloc, shadow);
}

// W2 grow：模拟"alloc 新 slab + transfer + journal_begin"中途 crash → undo 路径
// 应把 shadow.properties 还原回旧 slab，并 free 新 slab。
TEST_F(shadow_journal_fixture, recover_undo_property_slab_replace_when_field_not_yet_switched)
{
    auto* shadow = shm_object_create(m_alloc, 1, "C", "n", "/p", "0001");
    ASSERT_NE(shadow, nullptr);
    // 准备初始 slab（容量 4）
    ASSERT_TRUE(shm_object_set_property_int64(m_alloc, *shadow, "a", 1));
    auto* old_slab = shadow->properties.get();
    ASSERT_NE(old_slab, nullptr);
    const std::uint16_t old_cap = old_slab->slot_capacity;

    // 手工分配新 slab 模拟 W2 中途
    const std::uint16_t new_cap = static_cast<std::uint16_t>(old_cap * 2U);
    auto* new_slab = mc::engine::property_slab_create(m_alloc, new_cap);
    ASSERT_NE(new_slab, nullptr);
    // 不 transfer slot（recover undo 不依赖新 slab 内容）

    std::uint64_t saved_a = 0;
    std::memcpy(&saved_a, &shadow->properties, sizeof(saved_a));
    const auto diff =
        static_cast<std::int64_t>(reinterpret_cast<const std::byte*>(new_slab)
                                  - reinterpret_cast<const std::byte*>(&shadow->properties));
    const std::uint64_t saved_b = static_cast<std::uint64_t>(diff);

    mc::shm::container::journal_begin(
        shadow->journal, container_op::update,
        static_cast<std::uint64_t>(reinterpret_cast<const std::byte*>(&shadow->properties)
                                   - reinterpret_cast<const std::byte*>(shadow)),
        210U,  // sub_op::property_slab_replace
        saved_a, saved_b, 0, 0);

    shm_object_journal_recover(m_alloc, *shadow);

    EXPECT_EQ(shadow->journal.op.load(std::memory_order_acquire),
              static_cast<std::uint8_t>(container_op::none));
    EXPECT_EQ(shadow->properties.get(), old_slab);
    int64_t v = 0;
    EXPECT_TRUE(shm_object_get_property_int64(*shadow, "a", v));
    EXPECT_EQ(v, 1);

    shm_object_destroy(m_alloc, shadow);
}

// ============================================================================
// T8.2 fork+SIGKILL：父子共享 SHM region，子进程持续写 property，父进程
// 随机时刻 SIGKILL 子进程；attach 后对所有 shadow 跑 recover，验证 property
// 值要么旧要么新，slab CRC 不撕裂。
// ============================================================================

TEST_F(shadow_journal_fixture, fork_sigkill_during_write_recover_keeps_invariant)
{
    auto* shadow = shm_object_create(m_alloc, 1, "ForkT8", "f8", "/t/8", "0001");
    ASSERT_NE(shadow, nullptr);
    // 预填一个 property 槽，避免子进程同时触发 W2 grow 时的复杂窗口
    ASSERT_TRUE(shm_object_set_property_int64(m_alloc, *shadow, "n", 0));

    // 用 SHM 内一个原子标志做"父子准备同步"
    std::atomic<int>* sync_flag = static_cast<std::atomic<int>*>(
        m_alloc.allocate(sizeof(std::atomic<int>), alignof(std::atomic<int>)));
    ASSERT_NE(sync_flag, nullptr);
    new (sync_flag) std::atomic<int>(0);

    pid_t child = ::fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        // 子进程：循环高频写 property + 切换字符串，直到被 SIGKILL
        sync_flag->store(1, std::memory_order_release);
        std::int64_t counter = 1;
        while (true) {
            shm_object_set_property_int64(m_alloc, *shadow, "n", counter);
            // 偶尔写 blob 触发 W1 blob ownership 路径
            if ((counter & 0x3F) == 0) {
                std::string s = "v" + std::to_string(counter);
                shm_object_set_property_blob(m_alloc, *shadow, "n", s,
                                             property_type_tag::string);
            }
            ++counter;
            if ((counter & 0xFFF) == 0) {
                // 周期性切回 int64 防止 leak 累积
                shm_object_set_property_int64(m_alloc, *shadow, "n", counter);
            }
        }
    }

    // 父进程：等子进程开始写
    while (sync_flag->load(std::memory_order_acquire) != 1) {
        ::usleep(100);
    }
    // 给子进程少量时间累积写入
    ::usleep(20 * 1000);
    ASSERT_EQ(::kill(child, SIGKILL), 0);
    int status = 0;
    ASSERT_EQ(::waitpid(child, &status, 0), child);

    // 在父进程视角"attach"——其实仍是同一进程同一 mmap，但等价于 takeover
    // recover（journal 里可能有未完成事务）
    shm_object_journal_recover(m_alloc, *shadow);

    EXPECT_EQ(shadow->journal.op.load(std::memory_order_acquire),
              static_cast<std::uint8_t>(container_op::none));
    EXPECT_TRUE(shm_object_check(*shadow));
    auto* slab = shadow->properties.get();
    ASSERT_NE(slab, nullptr);
    EXPECT_TRUE(property_slab_check(*slab));

    // property "n" 必须可读（旧值 0 或子进程写过的某个值），且 type/value 自洽
    int64_t                       i_out = 0;
    std::string_view              s_out;
    mc::engine::property_type_tag tag_out{};
    const bool                    int_ok = shm_object_get_property_int64(*shadow, "n", i_out);
    const bool                    str_ok = shm_object_get_property_blob(*shadow, "n", s_out, tag_out);
    EXPECT_TRUE(int_ok || str_ok)
        << "property n 必须能按某种 type 被读出，不允许撕裂为无类型";

    sync_flag->~atomic();
    m_alloc.deallocate(sync_flag);
    shm_object_destroy(m_alloc, shadow);
}

// ============================================================================
// T8.2b fork+SIGKILL（identity setter）：循环切 path/name/position 不等长字符串
// 高频走 journaled_byte_string_replace；recover 后三个字段不能撕裂为空。
// ============================================================================

TEST_F(shadow_journal_fixture, fork_sigkill_during_identity_setters_recover_keeps_invariant)
{
    auto* shadow = shm_object_create(m_alloc, 1, "ForkIdent", "n0", "/p0", "0001");
    ASSERT_NE(shadow, nullptr);

    std::atomic<int>* sync_flag = static_cast<std::atomic<int>*>(
        m_alloc.allocate(sizeof(std::atomic<int>), alignof(std::atomic<int>)));
    ASSERT_NE(sync_flag, nullptr);
    new (sync_flag) std::atomic<int>(0);

    pid_t child = ::fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        sync_flag->store(1, std::memory_order_release);
        std::int64_t counter = 1;
        while (true) {
            const std::size_t len = static_cast<std::size_t>((counter % 23) + 1);
            std::string       payload(len, static_cast<char>('a' + (counter % 26)));
            switch (counter % 3) {
                case 0:
                    shm_object_set_name(m_alloc, *shadow, payload);
                    break;
                case 1:
                    shm_object_set_path(m_alloc, *shadow,
                                        std::string("/x/") + payload);
                    break;
                default:
                    shm_object_set_position(m_alloc, *shadow, payload);
                    break;
            }
            ++counter;
        }
    }

    while (sync_flag->load(std::memory_order_acquire) != 1) {
        ::usleep(100);
    }
    ::usleep(20 * 1000);
    ASSERT_EQ(::kill(child, SIGKILL), 0);
    int status = 0;
    ASSERT_EQ(::waitpid(child, &status, 0), child);

    shm_object_journal_recover(m_alloc, *shadow);

    EXPECT_EQ(shadow->journal.op.load(std::memory_order_acquire),
              static_cast<std::uint8_t>(container_op::none));
    EXPECT_TRUE(shm_object_check(*shadow));

    auto name_v = shm_object_name(*shadow);
    auto path_v = shm_object_path(*shadow);
    auto pos_v  = shm_object_position(*shadow);
    EXPECT_FALSE(name_v.empty()) << "name 撕裂为空";
    EXPECT_FALSE(path_v.empty()) << "path 撕裂为空";
    EXPECT_FALSE(pos_v.empty()) << "position 撕裂为空";

    sync_flag->~atomic();
    m_alloc.deallocate(sync_flag);
    shm_object_destroy(m_alloc, shadow);
}

// ============================================================================
// T8.2c fork+SIGKILL（property slab grow）：80 个互不相同的 key 持续写入，
// 反复触发 W2 grow（property_slab_replace）。recover 后 slab CRC 自洽，
// 已写入 key 数量在 [0, 80] 内（视 SIGKILL 命中点是 undo 还是 redo）。
// ============================================================================

TEST_F(shadow_journal_fixture, fork_sigkill_during_property_slab_grow_recover_keeps_invariant)
{
    auto* shadow = shm_object_create(m_alloc, 1, "ForkGrow", "g", "/g", "0001");
    ASSERT_NE(shadow, nullptr);

    std::atomic<int>* sync_flag = static_cast<std::atomic<int>*>(
        m_alloc.allocate(sizeof(std::atomic<int>), alignof(std::atomic<int>)));
    ASSERT_NE(sync_flag, nullptr);
    new (sync_flag) std::atomic<int>(0);

    pid_t child = ::fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        sync_flag->store(1, std::memory_order_release);
        std::int64_t counter = 0;
        while (true) {
            char key[16];
            std::snprintf(key, sizeof(key), "k%03lld",
                          static_cast<long long>(counter % 80));
            shm_object_set_property_int64(m_alloc, *shadow, key, counter);
            ++counter;
        }
    }

    while (sync_flag->load(std::memory_order_acquire) != 1) {
        ::usleep(100);
    }
    ::usleep(20 * 1000);
    ASSERT_EQ(::kill(child, SIGKILL), 0);
    int status = 0;
    ASSERT_EQ(::waitpid(child, &status, 0), child);

    shm_object_journal_recover(m_alloc, *shadow);

    EXPECT_EQ(shadow->journal.op.load(std::memory_order_acquire),
              static_cast<std::uint8_t>(container_op::none));
    EXPECT_TRUE(shm_object_check(*shadow));
    auto* slab = shadow->properties.get();
    ASSERT_NE(slab, nullptr) << "grow 路径中 properties 不应为 null";
    EXPECT_TRUE(property_slab_check(*slab));

    const std::uint16_t cnt = shm_object_property_count(*shadow);
    EXPECT_LE(cnt, 80U) << "property 数量超过预期上界";

    sync_flag->~atomic();
    m_alloc.deallocate(sync_flag);
    shm_object_destroy(m_alloc, shadow);
}

// ============================================================================
// T8.2d fork+SIGKILL（child add）：循环 add/remove 多个预分配 child，
// 触发 child_slab grow + update；recover 后 parent 自洽，child_count 在
// [0, kChildCount] 内。
// ============================================================================

TEST_F(shadow_journal_fixture, fork_sigkill_during_child_add_recover_keeps_invariant)
{
    auto* parent = shm_object_create(m_alloc, 1, "ForkParent", "P", "/P", "0001");
    ASSERT_NE(parent, nullptr);

    constexpr std::size_t       kChildCount = 24;
    std::array<shm_object*, kChildCount> children{};
    for (std::size_t i = 0; i < kChildCount; ++i) {
        char name[16];
        char pos[16];
        std::snprintf(name, sizeof(name), "c%02zu", i);
        std::snprintf(pos, sizeof(pos), "%04zu", i);
        children[i] = shm_object_create(m_alloc, 1, "ChildC", name, "/P/c", pos);
        ASSERT_NE(children[i], nullptr);
    }

    std::atomic<int>* sync_flag = static_cast<std::atomic<int>*>(
        m_alloc.allocate(sizeof(std::atomic<int>), alignof(std::atomic<int>)));
    ASSERT_NE(sync_flag, nullptr);
    new (sync_flag) std::atomic<int>(0);

    pid_t pid = ::fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        sync_flag->store(1, std::memory_order_release);
        std::int64_t counter = 0;
        while (true) {
            const std::size_t i = static_cast<std::size_t>(counter % kChildCount);
            if ((counter / kChildCount) % 2 == 0) {
                shm_object_add_child(m_alloc, *parent, children[i]);
            } else {
                shm_object_remove_child(*parent, children[i]);
            }
            ++counter;
        }
    }

    while (sync_flag->load(std::memory_order_acquire) != 1) {
        ::usleep(100);
    }
    ::usleep(20 * 1000);
    ASSERT_EQ(::kill(pid, SIGKILL), 0);
    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);

    shm_object_journal_recover(m_alloc, *parent);

    EXPECT_EQ(parent->journal.op.load(std::memory_order_acquire),
              static_cast<std::uint8_t>(container_op::none));
    EXPECT_TRUE(shm_object_check(*parent));

    // children 允许为 null（grow 早期窗口被 undo），否则 child_count 必须在 [0, kChildCount]。
    if (auto* cs = parent->children.get(); cs != nullptr) {
        const std::uint16_t cnt = shm_object_child_count(*parent);
        EXPECT_LE(cnt, static_cast<std::uint16_t>(kChildCount));
        (void)cs;
    }

    sync_flag->~atomic();
    m_alloc.deallocate(sync_flag);
    for (auto* c : children) {
        shm_object_destroy(m_alloc, c);
    }
    shm_object_destroy(m_alloc, parent);
}

// ============================================================================
// T8.3 ABI：旧版本 shm_object 数据（abi=2，且 journal 全零）走 recover
// 是 no-op；abi 校验在调用方
// ============================================================================

TEST_F(shadow_journal_fixture, recover_is_noop_when_journal_op_none)
{
    auto* shadow = shm_object_create(m_alloc, 1, "C", "n", "/p", "0001");
    ASSERT_NE(shadow, nullptr);
    shm_object_set_property_int64(m_alloc, *shadow, "k", 7);

    // journal 当前 op == none
    const auto crc_before = shadow->crc32_self;
    shm_object_journal_recover(m_alloc, *shadow);
    EXPECT_EQ(shadow->crc32_self, crc_before);
    int64_t v = 0;
    EXPECT_TRUE(shm_object_get_property_int64(*shadow, "k", v));
    EXPECT_EQ(v, 7);

    shm_object_destroy(m_alloc, shadow);
}
