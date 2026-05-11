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

// shm_allocator 多进程并发 benchmark
//
// 目的：量化当前单 arena_lock 架构下，10 级进程并发 alloc/dealloc 的真实开销，
// 为后续是否引入"slab per-class 独立锁"或"lock-free slab 快路径"提供数据依据。
//
// 场景：
//   1. 单进程串行 baseline
//   2. N 子进程并发 churn（alloc+dealloc 循环）
//   3. 覆盖 slab 路径（16B/64B）与 TLSF 路径（256B/4KB）
//
// 测试默认 DISABLED_ 前缀，仅手工开启：
//   ./mcbase_test --gtest_also_run_disabled_tests \
//                 --gtest_filter='allocator_concurrency_benchmark.*'

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include <mc/shm/allocator.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/region.h>

namespace {

std::string make_unique_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

class allocator_concurrency_benchmark : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_segment_name = make_unique_name("mc_shm_alloc_bench");
        mc::shm::shm_region_options opts;
        opts.segment_name = m_segment_name;
        opts.total_size   = 64 * 1024 * 1024; // 64 MB 防止碎片下 OOM
        m_region          = std::make_unique<mc::shm::shm_region>(opts);
        ASSERT_TRUE(m_region->is_valid());
    }

    void TearDown() override
    {
        m_region.reset();
        mc::shm::detail::shared_memory_backend::remove(m_segment_name);
    }

    // 单个 worker 的工作：churn loop，每次 alloc 立即 dealloc
    static void worker_churn(mc::shm::shm_allocator& a, std::size_t size,
                             std::size_t ops) noexcept
    {
        for (std::size_t i = 0; i < ops; ++i) {
            void* p = a.allocate(size);
            if (p != nullptr) {
                a.deallocate(p);
            }
        }
    }

    // 运行 n 个并发子进程 × ops 次 churn；返回 wallclock 毫秒
    double run_concurrent(int n, std::size_t size, std::size_t ops)
    {
        std::vector<pid_t> pids;
        pids.reserve(n);
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < n; ++i) {
            pid_t pid = ::fork();
            if (pid == -1) {
                ADD_FAILURE() << "fork failed";
                return -1;
            }
            if (pid == 0) {
                auto alloc = m_region->user_arena();
                worker_churn(alloc, size, ops);
                ::_exit(0);
            }
            pids.push_back(pid);
        }
        for (auto p : pids) {
            int status = 0;
            ::waitpid(p, &status, 0);
        }
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    // 单进程 baseline：直接在父进程串行
    double run_serial(std::size_t size, std::size_t ops)
    {
        auto alloc = m_region->user_arena();
        auto t0    = std::chrono::steady_clock::now();
        worker_churn(alloc, size, ops);
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    std::string                          m_segment_name;
    std::unique_ptr<mc::shm::shm_region> m_region;
};

struct workload {
    const char* label;
    std::size_t size; // 分配尺寸
    const char* path; // slab / tlsf，仅用于打印
};

} // namespace

TEST_F(allocator_concurrency_benchmark, DISABLED_slab_vs_tlsf_single_vs_multi_proc)
{
    // 每个 worker 的 churn 次数；总操作数 = ops_per_worker × n_procs
    constexpr std::size_t ops_per_worker = 100'000;

    const workload workloads[] = {
        {"slab-16B",  16,   "slab"},
        {"slab-32B",  32,   "slab"},
        {"slab-64B",  64,   "slab"},
        {"tlsf-256B", 256,  "tlsf"},
        {"tlsf-4KB",  4096, "tlsf"},
    };

    const int concurrency_levels[] = {1, 4, 8, 10};

    std::printf("\n"
                "============================================================\n"
                " shm_allocator concurrency benchmark (ops/worker = %zu)\n"
                "============================================================\n"
                "%-12s %-6s %5s  %10s  %12s  %9s\n",
                ops_per_worker, "label", "path", "nproc", "wall(ms)", "ops/sec(total)",
                "rel-1p");

    for (const auto& w : workloads) {
        // 单进程 baseline（父进程串行）——纯锁开销基准
        double baseline_ms = run_serial(w.size, ops_per_worker);
        std::printf("%-12s %-6s %5d  %10.2f  %12.0f  %9.2fx\n",
                    w.label, w.path, 1, baseline_ms,
                    static_cast<double>(ops_per_worker) / (baseline_ms / 1000.0),
                    1.0);
        ASSERT_GT(baseline_ms, 0.0);

        // 并发场景：n 子进程并行
        for (int n : concurrency_levels) {
            if (n <= 1) {
                continue;
            }
            double ms = run_concurrent(n, w.size, ops_per_worker);
            ASSERT_GT(ms, 0.0);
            double total_ops = static_cast<double>(ops_per_worker) * n;
            std::printf("%-12s %-6s %5d  %10.2f  %12.0f  %9.2fx\n",
                        w.label, w.path, n, ms,
                        total_ops / (ms / 1000.0),
                        ms / baseline_ms);
        }
    }

    std::printf("============================================================\n"
                " 解读：\n"
                "  rel-1p < n ：说明并行有效，锁竞争可接受\n"
                "  rel-1p ≈ n ：串行化严重（单 arena_lock 是瓶颈）\n"
                "  ops/sec(total) 下降 ：总吞吐被锁拖累\n"
                "============================================================\n");
}
