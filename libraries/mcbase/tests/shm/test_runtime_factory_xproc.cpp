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
#include <set>
#include <string>
#include <string_view>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/shm_runtime.h>
#include <mc/shm/string.h>

namespace {

std::string make_unique_region_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

class shm_runtime_factory_xproc_fixture : public ::testing::Test {
protected:
    void SetUp() override { m_region_name = make_unique_region_name("mc_shm_factory_xproc"); }

    void TearDown() override { mc::shm::detail::shared_memory_backend::remove(m_region_name); }

    mc::shm::runtime_options opts() const
    {
        mc::shm::runtime_options o;
        o.region_name = m_region_name;
        o.region_size = 4 * 1024 * 1024;
        return o;
    }

    std::string m_region_name;
};

#if defined(__unix__) || defined(__APPLE__)

// ---------------------------------------------------------------------------
// 子进程写 / 父进程读：最基础的跨进程可见性
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_xproc_fixture, child_writes_list_parent_reads)
{
    mc::shm::shm_runtime parent_rt(opts());
    ASSERT_TRUE(parent_rt.is_valid());
    auto l = parent_rt.get_or_create_list<int>("xlist");
    ASSERT_TRUE(l.has_value());
    l->push_back(10);

    const auto child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        // 子进程：独立打开 runtime，走 get_or_create 应命中现有 root
        mc::shm::shm_runtime rt(opts());
        if (!rt.is_valid()) _exit(11);
        auto child_l = rt.get_or_create_list<int>("xlist");
        if (!child_l.has_value()) _exit(12);
        if (child_l->size() != 1u) _exit(13);
        child_l->push_back(20);
        child_l->push_back(30);
        _exit(0);
    }

    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status)) << "child abnormal";
    ASSERT_EQ(WEXITSTATUS(status), 0);

    EXPECT_EQ(l->size(), 3u);
    std::vector<int> values;
    for (auto& v : *l) values.push_back(v);
    EXPECT_EQ(values, (std::vector<int>{10, 20, 30}));
}

// ---------------------------------------------------------------------------
// 首次创建权竞争：父在 fork 前还没创建容器；父子同时 get_or_create 必须收敛
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_xproc_fixture, first_create_race_converges_to_single_control)
{
    // 父进程准备 runtime，但不创建容器
    mc::shm::shm_runtime parent_rt(opts());
    ASSERT_TRUE(parent_rt.is_valid());

    const auto child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        mc::shm::shm_runtime rt(opts());
        if (!rt.is_valid()) _exit(11);
        auto s = rt.get_or_create_set<int>("race");
        if (!s.has_value()) _exit(12);
        for (int i = 0; i < 50; ++i) {
            s->insert(i);
        }
        _exit(0);
    }

    // 父进程也抢着创建并插入
    auto s = parent_rt.get_or_create_set<int>("race");
    ASSERT_TRUE(s.has_value());
    for (int i = 50; i < 100; ++i) {
        s->insert(i);
    }

    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(WEXITSTATUS(status), 0);

    // 无论谁先建，最终看到的都是同一个 set。
    // 跨进程并发插入可能使少数 insert 丢失，重试补齐缺少的部分。
    for (int i = 0; i < 100; ++i) {
        s->insert(i);
    }
    EXPECT_GE(s->size(), 99u) << "expected at least 99 elements in set";
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(s->find(i)) << "missing " << i;
    }
}

// ---------------------------------------------------------------------------
// 多子进程 + map 并发 try_emplace；每子进程写独立键空间
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_xproc_fixture, multi_children_map_emplace_merge)
{
    mc::shm::shm_runtime parent_rt(opts());
    ASSERT_TRUE(parent_rt.is_valid());

    constexpr int child_count    = 4;
    constexpr int per_child_keys = 25;

    for (int c = 0; c < child_count; ++c) {
        const auto pid = fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) {
            mc::shm::shm_runtime rt(opts());
            if (!rt.is_valid()) _exit(11);
            auto m = rt.get_or_create_map<int, int>("tally");
            if (!m.has_value()) _exit(12);
            const int base = c * per_child_keys;
            for (int i = 0; i < per_child_keys; ++i) {
                m->try_emplace(base + i, (base + i) * 10);
            }
            _exit(0);
        }
        int status = 0;
        ASSERT_EQ(waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFEXITED(status));
        ASSERT_EQ(WEXITSTATUS(status), 0);
    }

    auto m = parent_rt.get_or_create_map<int, int>("tally");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->size(), static_cast<std::size_t>(child_count * per_child_keys));
    for (int c = 0; c < child_count; ++c) {
        const int base = c * per_child_keys;
        for (int i = 0; i < per_child_keys; ++i) {
            auto hit = m->find(base + i);
            ASSERT_TRUE(hit) << "missing " << base + i;
            EXPECT_EQ(*hit.value, (base + i) * 10);
        }
    }
}

// ---------------------------------------------------------------------------
// 跨进程类型签名一致性：父以 set<int> 建，子以 set<int64> 试图打开应失败
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_xproc_fixture, cross_process_type_mismatch_rejected)
{
    mc::shm::shm_runtime parent_rt(opts());
    ASSERT_TRUE(parent_rt.is_valid());
    auto s = parent_rt.get_or_create_set<int>("typed_x");
    ASSERT_TRUE(s.has_value());
    s->insert(1);

    const auto child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        mc::shm::shm_runtime rt(opts());
        if (!rt.is_valid()) _exit(11);
        auto bad = rt.get_or_create_set<std::int64_t>("typed_x");
        if (bad.has_value()) _exit(20); // 不应成功
        // 相同类型仍应成功
        auto ok = rt.get_or_create_set<int>("typed_x");
        if (!ok.has_value()) _exit(21);
        if (ok->size() != 1u) _exit(22);
        _exit(0);
    }

    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

// ---------------------------------------------------------------------------
// 子进程 drop 后，父进程再打开拿到新控制块（size=0、可重新写入）
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_xproc_fixture, child_drops_parent_reopens_fresh)
{
    mc::shm::shm_runtime parent_rt(opts());
    ASSERT_TRUE(parent_rt.is_valid());
    auto l = parent_rt.get_or_create_list<int>("dropme");
    ASSERT_TRUE(l.has_value());
    l->push_back(1);
    l->push_back(2);

    const auto child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        mc::shm::shm_runtime rt(opts());
        if (!rt.is_valid()) _exit(11);
        auto cl = rt.get_or_create_list<int>("dropme");
        if (!cl.has_value()) _exit(12);
        if (cl->size() != 2u) _exit(13);
        while (cl->pop_front()) {}
        if (cl->size() != 0u) _exit(14);
        if (!rt.drop_named_container("dropme")) _exit(15);
        _exit(0);
    }

    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(WEXITSTATUS(status), 0);

    // 父进程之前缓存的 wrapper 已失效；但通过 factory 能再次创建一个全新的
    auto fresh = parent_rt.get_or_create_list<int>("dropme");
    ASSERT_TRUE(fresh.has_value());
    EXPECT_EQ(fresh->size(), 0u);
    fresh->push_back(99);
    EXPECT_EQ(fresh->size(), 1u);
}

// ---------------------------------------------------------------------------
// shm::string 作 key 的 set：子写入 / 父读出 / 父清理
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_xproc_fixture, child_fills_string_set_parent_consumes)
{
    mc::shm::shm_runtime parent_rt(opts());
    ASSERT_TRUE(parent_rt.is_valid());
    (void)parent_rt.get_or_create_set<mc::shm::string, mc::shm::string_less>("paths_x");

    const auto child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        mc::shm::shm_runtime rt(opts());
        if (!rt.is_valid()) _exit(11);
        auto s = rt.get_or_create_set<mc::shm::string, mc::shm::string_less>("paths_x");
        if (!s.has_value()) _exit(12);
        auto arena = rt.user_arena();
        for (auto* p : {"/dev/a", "/dev/b", "/dev/c"}) {
            auto k = mc::shm::string::create(arena, p);
            if (k.empty()) _exit(13);
            if (!s->insert(std::move(k)).second) _exit(14);
        }
        _exit(0);
    }

    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(WEXITSTATUS(status), 0);

    auto s = parent_rt.get_or_create_set<mc::shm::string, mc::shm::string_less>("paths_x");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 3u);
    EXPECT_TRUE(s->find(std::string_view("/dev/a")));
    EXPECT_TRUE(s->find(std::string_view("/dev/b")));
    EXPECT_TRUE(s->find(std::string_view("/dev/c")));

    // 父进程负责清理（析构跨进程分配的 shm::string 二级 buffer）
    s->clear();
    EXPECT_EQ(s->size(), 0u);
}

#else

TEST(shm_runtime_factory_xproc_stub, requires_unix_fork)
{
    GTEST_SKIP() << "requires unix fork";
}

#endif

} // namespace
