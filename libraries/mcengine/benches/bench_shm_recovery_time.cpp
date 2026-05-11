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

// bench_shm_recovery_time
//
// 端到端 takeover + recover 时延：父进程注册 N 个对象 + 每对象 K 个 properties，
// 然后 fork。子进程 force-attach 触发 takeover，再 service.start()
// 的 recover 路径重建 N 个 heap 对象，计时 service.start() 的耗时。
// 通过 pipe 把子进程时长返回父进程统一打印。
//
// 输出 markdown 表格行。

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <mc/engine.h>
#include <mc/format.h>
#include <mc/runtime.h>
#include <mc/shm/byte_string.h>
#include <mc/shm/container/map.h>

#include "shm_object_ops.h"
#include "shm_runtime_provider.h"
#include "shm_service.h"
#include "shm_service_ops.h"

namespace bench {

template <typename T>
using property = mc::engine::property<T>;

class rec_iface : public mc::engine::interface<rec_iface> {
public:
    MC_INTERFACE("org.bench.recovery.iface")

    property<int32_t>     m_counter;
    property<std::string> m_label;
};

class rec_object : public mc::engine::object<rec_object> {
public:
    MC_OBJECT(rec_object, "RecObject", "/org/bench/recovery", (rec_iface))

    rec_iface m_iface;
};

struct rec_service : public mc::engine::service {
    rec_service() : mc::engine::service("org.openubmc.bench.recovery.svc") {}
};

}  // namespace bench

MC_REFLECT(bench::rec_iface,
           (m_counter, "counter")(m_label, "label"), )
MC_REFLECT(bench::rec_object, (m_iface, "iface"), )

namespace bench {

mc::engine::shm_service_map _open_service_map()
{
    using mc::engine::shm_byte_string;
    using mc::engine::shm_byte_string_less;
    using mc::engine::shm_ptr;
    using mc::engine::shm_service;
    auto& rt  = mc::engine::shm_runtime_provider::instance();
    auto  opt = rt.get_or_create_map<shm_byte_string,
                                     shm_ptr<shm_service>,
                                     shm_byte_string_less>("mcengine.service_map");
    if (!opt) {
        std::fprintf(stderr, "shm_service_map open failed\n");
        std::abort();
    }
    return std::move(*opt);
}

void _force_takeover_for_child(mc::string_view svc_name)
{
    auto& rt    = mc::engine::shm_runtime_provider::instance();
    auto  arena = rt.user_arena();
    auto  smap  = _open_service_map();
    (void)mc::engine::shm_service_attach(arena, smap, svc_name,
                                         static_cast<std::uint32_t>(::getpid()),
                                         /*force=*/true);
}

struct sample {
    std::size_t obj_count;
    std::size_t props_per_obj;
    double      ns_per_obj;
    double      total_ms;
    double      objs_per_sec;
};

sample run_one(std::size_t obj_count, std::size_t props_per_obj)
{
    mc::engine::engine::reset_for_test();

    auto svc = std::make_unique<rec_service>();
    if (!svc->start()) {
        std::fprintf(stderr, "parent svc.start failed\n");
        std::abort();
    }

    for (std::size_t i = 0; i < obj_count; ++i) {
        auto obj = rec_object::create();
        obj->set_object_path(sformat("/org/bench/recovery/obj_{}", i));
        obj->set_object_name(sformat("obj_{}", i));
        obj->set_position(sformat("{:06d}", 10000 + i));
        svc->register_object(obj);
        obj->m_iface.m_counter.set_value(static_cast<int32_t>(i));
        // props_per_obj 中第一个被 m_counter 占了，剩余通过 label 字段 + 多次写入近似（API 限制）
        // 实际 reflect interface 只暴露两个 property，这里用 m_label.set_value 多次 set 模拟"写流量"。
        for (std::size_t k = 1; k < props_per_obj; ++k) {
            obj->m_iface.m_label.set_value(sformat("label-{}-{}", i, k));
        }
    }

    int pipefd[2];
    if (::pipe(pipefd) != 0) {
        std::fprintf(stderr, "pipe failed\n");
        std::abort();
    }
    pid_t child = ::fork();
    if (child < 0) {
        std::fprintf(stderr, "fork failed\n");
        std::abort();
    }
    if (child == 0) {
        ::close(pipefd[0]);
        try {
            _force_takeover_for_child("org.openubmc.bench.recovery.svc");
            const auto t0      = std::chrono::steady_clock::now();
            auto       child_svc = std::make_unique<rec_service>();
            if (!child_svc->start()) {
                std::_Exit(31);
            }
            const auto t1 = std::chrono::steady_clock::now();
            std::uint64_t ns =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
            ssize_t written = ::write(pipefd[1], &ns, sizeof(ns));
            (void)written;
            std::_Exit(0);
        } catch (...) {
            std::_Exit(99);
        }
    }

    ::close(pipefd[1]);
    std::uint64_t child_ns = 0;
    ssize_t       got      = ::read(pipefd[0], &child_ns, sizeof(child_ns));
    ::close(pipefd[0]);
    int status = -1;
    ::waitpid(child, &status, 0);
    if (got != static_cast<ssize_t>(sizeof(child_ns)) || !WIFEXITED(status)
        || WEXITSTATUS(status) != 0) {
        std::fprintf(stderr,
                     "child failed (got=%zd exit=%d) for N=%zu props=%zu\n",
                     got, WIFEXITED(status) ? WEXITSTATUS(status) : -1,
                     obj_count, props_per_obj);
        std::abort();
    }

    svc->stop();

    sample s;
    s.obj_count    = obj_count;
    s.props_per_obj = props_per_obj;
    s.total_ms     = static_cast<double>(child_ns) / 1.0e6;
    s.ns_per_obj   = static_cast<double>(child_ns) / static_cast<double>(obj_count);
    s.objs_per_sec = obj_count > 0 ? 1.0e9 / s.ns_per_obj : 0.0;
    return s;
}

void print_header()
{
    std::printf("\n## bench_shm_recovery_time\n\n");
    std::printf("| obj_count | props/obj (set流量) | total ms | us/obj | objs/sec |\n");
    std::printf("|---:|---:|---:|---:|---:|\n");
}

void print_row(const sample& s)
{
    std::printf("| %9zu | %19zu | %8.3f | %6.2f | %8.0f |\n",
                s.obj_count, s.props_per_obj, s.total_ms, s.ns_per_obj / 1000.0,
                s.objs_per_sec);
    std::fflush(stdout);
}

}  // namespace bench

int main()
{
    mc::runtime::reset_runtime_context();

    bench::print_header();

    const std::vector<std::pair<std::size_t, std::size_t>> grid = {
        {10, 4}, {100, 4}, {500, 4}, {1000, 4},
        {1000, 16},
    };
    for (const auto& [n, p] : grid) {
        auto s = bench::run_one(n, p);
        bench::print_row(s);
    }
    return 0;
}
