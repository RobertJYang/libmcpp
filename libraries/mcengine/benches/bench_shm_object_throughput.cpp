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

// bench_shm_object_throughput
//
// 单线程吞吐微基准：直接打 shm_object_ops，不走 service / engine 上层。
// 输出：每个场景的 ops/s + ns/op，markdown 行格式。
// 不引入 google benchmark，避免新依赖；自己计时 + 多轮取最稳值。

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <unistd.h>
#include <vector>

#include <mc/shm/allocator.h>
#include <mc/shm/region.h>

#include "shm_object_ops.h"

namespace {

using mc::engine::shm_object;
using mc::engine::shm_object_add_child;
using mc::engine::shm_object_create;
using mc::engine::shm_object_destroy;
using mc::engine::shm_object_set_property_int64;
using mc::engine::shm_allocator;
using mc::shm::shm_region;
using mc::shm::shm_region_options;

constexpr std::size_t REGION_SIZE_BYTES   = 256ULL * 1024ULL * 1024ULL;  // 256MB
constexpr std::size_t SHM_ROOT_CAPACITY   = 16U;
constexpr int         WARMUP_ITERS        = 1024;
constexpr int         REPEATS             = 5;

struct bench_result {
    std::string   name;
    std::size_t   ops;
    double        best_ns_per_op;
    double        avg_ns_per_op;
    double        best_mops_per_sec;
};

// 禁止编译器把 op() 的返回值/副作用优化掉
template <typename T>
inline void do_not_optimize(T const& v) noexcept
{
    asm volatile("" : : "r,m"(v) : "memory");
}

shm_region make_region()
{
    std::random_device rd;
    std::mt19937       rng(rd());
    char               buf[128];
    std::snprintf(buf, sizeof(buf), "mc_bench_throughput_%d_%u", ::getpid(), rng());

    shm_region_options opts;
    opts.segment_name  = mc::string(buf);
    opts.total_size    = REGION_SIZE_BYTES;
    opts.root_capacity = SHM_ROOT_CAPACITY;
    return shm_region(opts);
}

template <typename Setup, typename Op>
bench_result run_bench(const std::string& name, std::size_t ops, Setup&& setup, Op&& op)
{
    bench_result r;
    r.name           = name;
    r.ops            = ops;
    r.best_ns_per_op = std::numeric_limits<double>::max();
    double total_avg = 0.0;

    for (int rep = 0; rep < REPEATS; ++rep) {
        setup();
        for (int i = 0; i < WARMUP_ITERS; ++i) {
            op(i);
        }
        const auto t0 = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < ops; ++i) {
            op(static_cast<int>(i));
        }
        const auto t1     = std::chrono::steady_clock::now();
        const auto elapsed_ns =
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        const double ns_per_op = elapsed_ns / static_cast<double>(ops);
        r.best_ns_per_op = std::min(r.best_ns_per_op, ns_per_op);
        total_avg += ns_per_op;
    }
    r.avg_ns_per_op       = total_avg / static_cast<double>(REPEATS);
    r.best_mops_per_sec   = 1000.0 / r.best_ns_per_op;  // ns/op -> Mops/s
    return r;
}

void print_header()
{
    std::printf("\n## bench_shm_object_throughput\n\n");
    std::printf("| scenario | ops | best ns/op | avg ns/op | best Mops/s |\n");
    std::printf("|---|---:|---:|---:|---:|\n");
}

void print_row(const bench_result& r)
{
    std::printf("| %-46s | %8zu | %10.1f | %10.1f | %11.3f |\n",
                r.name.c_str(), r.ops, r.best_ns_per_op, r.avg_ns_per_op,
                r.best_mops_per_sec);
    std::fflush(stdout);
}

// ---------------- 场景 1：set_property_int64 同 key 覆盖（无 grow） ----------------

bench_result bench_set_property_same_key(shm_allocator& alloc, std::size_t ops)
{
    shm_object* obj = nullptr;
    auto setup = [&]() {
        if (obj != nullptr) {
            shm_object_destroy(alloc, obj);
        }
        obj = shm_object_create(alloc, 1, "Cls", "name", "/path", "pos");
    };
    auto op = [&](int /*i*/) {
        bool ok = shm_object_set_property_int64(alloc, *obj, "k0", 42);
        do_not_optimize(ok);
    };
    auto r = run_bench("set_property_int64 same key (no grow)", ops, setup, op);
    if (obj != nullptr) {
        shm_object_destroy(alloc, obj);
    }
    return r;
}

// ---------------- 场景 2：set_property_int64 不同 key（走 grow） ----------------

bench_result bench_set_property_growing_keys(shm_allocator& alloc, std::size_t ops)
{
    shm_object* obj = nullptr;
    std::vector<std::string> keys;
    keys.reserve(ops);
    for (std::size_t i = 0; i < ops; ++i) {
        char k[16];
        std::snprintf(k, sizeof(k), "k%05zu", i);
        keys.emplace_back(k);
    }
    auto setup = [&]() {
        if (obj != nullptr) {
            shm_object_destroy(alloc, obj);
        }
        obj = shm_object_create(alloc, 2, "Cls", "name", "/path", "pos");
    };
    auto op = [&](int i) {
        bool ok = shm_object_set_property_int64(alloc, *obj,
                                                keys[static_cast<std::size_t>(i)], i);
        do_not_optimize(ok);
    };
    auto r = run_bench("set_property_int64 unique keys (with grow)", ops, setup, op);
    if (obj != nullptr) {
        shm_object_destroy(alloc, obj);
    }
    return r;
}

// ---------------- 场景 3：add_child 重复添加同一 child（no-op 路径） ----------------

bench_result bench_add_child_idempotent(shm_allocator& alloc, std::size_t ops)
{
    shm_object* parent = nullptr;
    shm_object* child  = nullptr;
    auto setup = [&]() {
        if (parent != nullptr) {
            shm_object_destroy(alloc, parent);
        }
        if (child != nullptr) {
            shm_object_destroy(alloc, child);
        }
        parent = shm_object_create(alloc, 10, "Cls", "p", "/p", "pos");
        child  = shm_object_create(alloc, 11, "Cls", "c", "/p/c", "pos");
        // 先加一次，让 slab 已存在
        shm_object_add_child(alloc, *parent, child);
    };
    auto op = [&](int /*i*/) {
        bool ok = shm_object_add_child(alloc, *parent, child);
        do_not_optimize(ok);
    };
    auto r = run_bench("add_child idempotent (same child)", ops, setup, op);
    if (parent != nullptr) {
        shm_object_destroy(alloc, parent);
    }
    if (child != nullptr) {
        shm_object_destroy(alloc, child);
    }
    return r;
}

// ---------------- 场景 4：add_child 不同 child（走 child_slab grow） ----------------

bench_result bench_add_child_growing(shm_allocator& alloc, std::size_t ops)
{
    shm_object*              parent = nullptr;
    std::vector<shm_object*> children;
    auto cleanup_children = [&]() {
        for (auto* c : children) {
            if (c != nullptr) {
                shm_object_destroy(alloc, c);
            }
        }
        children.clear();
    };
    auto setup = [&]() {
        cleanup_children();
        if (parent != nullptr) {
            shm_object_destroy(alloc, parent);
        }
        parent = shm_object_create(alloc, 20, "Cls", "p", "/p", "pos");
        children.reserve(ops);
        for (std::size_t i = 0; i < ops; ++i) {
            char path[32];
            std::snprintf(path, sizeof(path), "/p/c%05zu", i);
            children.push_back(shm_object_create(alloc, 100 + i, "Cls", "c", path, "pos"));
        }
    };
    auto op = [&](int i) {
        bool ok = shm_object_add_child(alloc, *parent,
                                        children[static_cast<std::size_t>(i)]);
        do_not_optimize(ok);
    };
    auto r = run_bench("add_child unique children (with grow)", ops, setup, op);
    cleanup_children();
    if (parent != nullptr) {
        shm_object_destroy(alloc, parent);
    }
    return r;
}

}  // namespace

int main(int argc, char** argv)
{
    // grow 路径在 property_slab 内部是 O(slot_count) 线性 find-by-key + 偶发拷贝，
    // 整体随 ops 平方放大；ops_small 控制在 5000 内总耗时可控。
    std::size_t ops_small = 5000;    // grow 路径（每次新 key/child）
    std::size_t ops_large = 200000;  // 同 key/child 的 cheap 路径
    if (argc >= 2) {
        ops_small = static_cast<std::size_t>(std::atoll(argv[1]));
    }
    if (argc >= 3) {
        ops_large = static_cast<std::size_t>(std::atoll(argv[2]));
    }

    auto region = make_region();
    if (!region.is_valid()) {
        std::fprintf(stderr, "shm_region invalid\n");
        return 2;
    }
    auto alloc = region.user_arena();
    if (!alloc.is_valid()) {
        std::fprintf(stderr, "shm_allocator invalid\n");
        return 2;
    }

    print_header();
    print_row(bench_set_property_same_key(alloc, ops_large));
    print_row(bench_set_property_growing_keys(alloc, ops_small));
    print_row(bench_add_child_idempotent(alloc, ops_large));
    print_row(bench_add_child_growing(alloc, ops_small));
    return 0;
}
