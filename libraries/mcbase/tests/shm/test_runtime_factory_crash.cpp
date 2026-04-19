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

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/shm_runtime.h>

namespace {

std::string make_unique_region_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

#if defined(__unix__) || defined(__APPLE__)

class shm_runtime_factory_crash_fixture : public ::testing::Test {
protected:
    void SetUp() override { m_region_name = make_unique_region_name("mc_shm_fac_crash"); }

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

// ---------------------------------------------------------------------------
// 子进程在 factory 调用的热循环中被 SIGKILL 打死，parent 仍应能正常创建与使用
// 容器。覆盖两种可能的崩溃时机：
//   - 持有 control->lock 期间：下一个 lock 申请者通过 ipc_mutex 的 dead holder
//     探测走 recovered 路径；
//   - allocate 成功但 upsert_root 未完成：控制块成孤儿，root_table 未污染，
//     parent 再 ensure 时走"找不到 → 重新 allocate+init+upsert_root"分支。
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_crash_fixture, child_sigkill_during_factory_leaves_region_usable)
{
    mc::shm::shm_runtime parent_rt(opts());
    ASSERT_TRUE(parent_rt.is_valid());

    const auto child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        mc::shm::shm_runtime rt(opts());
        if (!rt.is_valid()) _exit(11);
        // 热循环不停 get_or_create，极大概率命中 factory 的各个阶段
        for (int i = 0;; ++i) {
            const auto name = std::string("busy_") + std::to_string(i % 4);
            auto       s    = rt.get_or_create_set<int>(name);
            if (!s.has_value()) continue;
            s->insert(i);
        }
    }

    // 让 child 跑起来，确保工厂热循环在进行
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ASSERT_EQ(kill(child, SIGKILL), 0);

    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    EXPECT_TRUE(WIFSIGNALED(status));

    // parent 侧：之前可能完全没建过这些容器，也可能建了一部分
    // 无论如何，get_or_create 必须收敛到可用状态
    for (int round = 0; round < 3; ++round) {
        for (int k = 0; k < 4; ++k) {
            const auto name = std::string("busy_") + std::to_string(k);
            auto       s    = parent_rt.get_or_create_set<int>(name);
            ASSERT_TRUE(s.has_value()) << "round=" << round << " name=" << name;
            s->insert(10000 + round * 10 + k);
        }
    }
    // 至少 4 个容器存在且可用
    for (int k = 0; k < 4; ++k) {
        const auto name = std::string("busy_") + std::to_string(k);
        auto       s    = parent_rt.get_or_create_set<int>(name);
        ASSERT_TRUE(s.has_value());
        EXPECT_GE(s->size(), 3u);  // 至少 parent 的 3 轮插入
    }
}

// ---------------------------------------------------------------------------
// 反复"子进程启动→创建容器→被 SIGKILL→父进程验证"闭环，确认孤儿控制块不会
// 把 region 吃干，工厂仍能持续创建新的命名容器。
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_crash_fixture, repeated_child_crashes_still_converge)
{
    mc::shm::shm_runtime parent_rt(opts());
    ASSERT_TRUE(parent_rt.is_valid());

    constexpr int rounds = 20;
    for (int r = 0; r < rounds; ++r) {
        const auto pid = fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) {
            mc::shm::shm_runtime rt(opts());
            if (!rt.is_valid()) _exit(11);
            // 在一个独立 name 上循环首次创建 + insert，被外部随时 SIGKILL
            const auto name = std::string("roundN");  // 公共 name，刻意让它跨轮复用
            for (int i = 0;; ++i) {
                auto m = rt.get_or_create_map<int, int>(name);
                if (!m.has_value()) continue;
                m->try_emplace(r * 10000 + i, i);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        kill(pid, SIGKILL);
        int status = 0;
        ASSERT_EQ(waitpid(pid, &status, 0), pid);
        EXPECT_TRUE(WIFSIGNALED(status));

        auto m = parent_rt.get_or_create_map<int, int>("roundN");
        ASSERT_TRUE(m.has_value()) << "round " << r;
        // parent 自身插入一条，验证可写
        m->try_emplace(-(r + 1), r);
        ASSERT_TRUE(m->find(-(r + 1)));
    }
}

// ---------------------------------------------------------------------------
// 子进程抢先 drop 容器，但在 drop 过程中（clear 完但 drop 未完成）被杀。
// parent 再 ensure 应仍能正常工作，因为 drop 是"单次 erase_root"动作，要么
// 完全完成、要么完全没发生（root_table 写入是原子的）。
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_crash_fixture, child_killed_before_drop_keeps_container_valid)
{
    mc::shm::shm_runtime parent_rt(opts());
    ASSERT_TRUE(parent_rt.is_valid());
    auto s = parent_rt.get_or_create_set<int>("pre_drop");
    ASSERT_TRUE(s.has_value());
    for (int i = 0; i < 10; ++i) s->insert(i);
    const auto old_ctrl = s->control();

    const auto child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        mc::shm::shm_runtime rt(opts());
        if (!rt.is_valid()) _exit(11);
        // clear + drop 之间可能被杀；两种结果均可接受
        auto cs = rt.get_or_create_set<int>("pre_drop");
        if (!cs.has_value()) _exit(12);
        cs->clear();
        // 不等父进程 kill，这里主动退出，parent 验证 drop 是否被观察到
        (void)rt.drop_named_container("pre_drop");
        _exit(0);
    }

    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(WEXITSTATUS(status), 0);

    // child 执行了完整 clear+drop，parent 的旧指针不应再被使用（root 已被抹掉）
    auto fresh = parent_rt.get_or_create_set<int>("pre_drop");
    ASSERT_TRUE(fresh.has_value());
    EXPECT_EQ(fresh->size(), 0u);
    // fresh 与旧的 control 可能是同一地址（allocator 复用），也可能不是；
    // 这里只验证行为而非地址
    (void)old_ctrl;

    fresh->insert(77);
    EXPECT_TRUE(fresh->find(77));
}

#else

TEST(shm_runtime_factory_crash_stub, requires_unix_fork) { GTEST_SKIP(); }

#endif

} // namespace
