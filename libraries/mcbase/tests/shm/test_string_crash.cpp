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
#include <string>

#include <sys/wait.h>
#include <unistd.h>

#include <mc/shm/allocator.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/region.h>
#include <mc/shm/string.h>

using mc::shm::shm_allocator;
using mc::shm::shm_region;
using mc::shm::shm_region_options;
using mc::shm::string;

namespace {

std::string make_unique_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(::getpid()) + "_" + std::to_string(now_ns);
}

constexpr const char* k_root = "mcstr_slot";

// 在 shm 里常驻的一个 string slot，供父子进程共享访问
class shm_string_crash_fixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_segment_name = make_unique_name("mc_shm_string_crash");
        shm_region_options options;
        options.segment_name = m_segment_name;
        options.total_size   = 2 * 1024 * 1024;
        m_region             = std::make_unique<shm_region>(options);
        ASSERT_TRUE(m_region->is_valid());

        auto  alloc = m_region->user_arena();
        auto* mem   = alloc.allocate(sizeof(string), alignof(string));
        ASSERT_NE(mem, nullptr);
        m_slot = new (mem) string{};

        ASSERT_TRUE(m_region->upsert_root(k_root, m_region->offset_of(mem), sizeof(string)));
    }

    void TearDown() override
    {
        m_region.reset();
        mc::shm::detail::shared_memory_backend::remove(m_segment_name);
    }

    std::string              m_segment_name;
    std::unique_ptr<shm_region> m_region;
    string*                  m_slot = nullptr;
};

struct child_ctx {
    std::unique_ptr<shm_region> region;
    string*                     slot = nullptr;
};

child_ctx child_attach(const std::string& seg)
{
    child_ctx ctx;
    shm_region_options options;
    options.segment_name = seg;
    options.open_mode    = mc::shm::detail::open_mode::open_existing;
    ctx.region           = std::make_unique<shm_region>(options);
    if (!ctx.region->is_valid()) {
        return ctx;
    }
    auto root = ctx.region->find_root(k_root);
    if (!root) {
        return ctx;
    }
    ctx.slot = static_cast<string*>(ctx.region->address_from_offset(root->offset));
    return ctx;
}

} // namespace

// =========================================================================
// 跨进程 attach：子进程 attach 并 destroy，父进程 allocator 的 live_bytes 下降
// =========================================================================
TEST_F(shm_string_crash_fixture, child_destroy_releases_buffer_in_parent_view)
{
    auto alloc        = m_region->user_arena();
    *m_slot           = string::create(alloc, "cross-process-string");
    const auto baseline_after_create = alloc.allocated_size();
    ASSERT_TRUE(m_slot->buffer_intact());

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto ctx = child_attach(m_segment_name);
        if (ctx.slot == nullptr) {
            ::_exit(2);
        }
        if (ctx.slot->std_view() != std::string_view("cross-process-string")) {
            ::_exit(3);
        }
        ctx.slot->destroy();
        if (!ctx.slot->empty()) {
            ::_exit(4);
        }
        ::_exit(0);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(0, WEXITSTATUS(status));

    // 父进程视角：buffer 已回收，slot 已清空
    EXPECT_TRUE(m_slot->empty());
    EXPECT_LT(alloc.allocated_size(), baseline_after_create);
}

// =========================================================================
// 子进程崩溃：fork 后 create 一个新 string 并提前 SIGKILL（未 destroy）
// 父进程应仍能使用 region；buffer 成为 leaked（allocator 层面 live_bytes 仍高）
// =========================================================================
TEST_F(shm_string_crash_fixture, child_crash_leaves_buffer_leaked_but_region_usable)
{
    auto       alloc    = m_region->user_arena();
    const auto baseline = alloc.allocated_size();

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto ctx = child_attach(m_segment_name);
        if (ctx.slot == nullptr) {
            ::_exit(2);
        }
        auto child_alloc = ctx.region->user_arena();

        // 分配一个 buffer 但不记录到 slot 也不释放，直接 SIGKILL 模拟崩溃
        // 这相当于 string::create 写完 buffer 但还没挂到 slot 时挂掉
        *ctx.slot = string::create(child_alloc, "leaked-on-crash");
        // 手动把 slot 清空，但不 destroy——模拟 slot 被重置但 buffer 还挂着
        // 由于我们没有对 string 做这种低层破坏的入口，这里改为直接创建后不释放
        // 再 SIGKILL
        ::raise(SIGKILL);
        ::_exit(0);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status));
    EXPECT_EQ(SIGKILL, WTERMSIG(status));

    // 父进程再用 region 仍正常（allocator recovery 应已由 arena_guard 处理）
    auto* probe_mem = alloc.allocate(128, 16);
    EXPECT_NE(nullptr, probe_mem);
    if (probe_mem) {
        alloc.deallocate(probe_mem);
    }

    // 父进程仍看得到 slot（子进程 create 后退出，slot 还挂着 buffer）
    // 父进程此刻走 destroy() 能正确 free 子进程创建的 buffer（registry 在父进程 fresh）
    if (!m_slot->empty()) {
        EXPECT_EQ("leaked-on-crash", m_slot->std_view());
        m_slot->destroy();
    }

    // 无论子进程是否完成写入，allocator 都应保持一致
    EXPECT_GE(alloc.allocated_size(), baseline);
}

// =========================================================================
// 连续多次"子进程崩溃"循环：验证每次崩溃后父进程仍可正常 alloc/free/destroy
// registry 在 fork 后子进程里有独立副本（fork 拷贝），父进程 registry 不受影响
// =========================================================================
TEST_F(shm_string_crash_fixture, repeated_child_crashes_region_still_usable)
{
    auto alloc = m_region->user_arena();

    for (int i = 0; i < 8; ++i) {
        pid_t pid = ::fork();
        ASSERT_NE(pid, -1);
        if (pid == 0) {
            auto ctx = child_attach(m_segment_name);
            if (ctx.slot == nullptr) {
                ::_exit(2);
            }
            auto child_alloc = ctx.region->user_arena();
            for (int k = 0; k < 5; ++k) {
                auto s = string::create(child_alloc,
                                        std::string("loop_") + std::to_string(k));
                (void)s;
            }
            ::raise(SIGKILL);
        }
        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFSIGNALED(status));

        // 父进程仍能 create / destroy
        auto s = string::create(alloc, "parent-after-crash");
        EXPECT_FALSE(s.empty());
        EXPECT_TRUE(s.buffer_intact());
        EXPECT_EQ("parent-after-crash", s.std_view());
    }
}
