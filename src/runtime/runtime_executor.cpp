/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/runtime/runtime_context.h>
#include <mc/runtime/runtime_executor.h>
#include <mc/singleton.h>

namespace mc::runtime {

// TLS 变量：用于绑定目标 thread_pool
thread_local thread_pool* g_bound_pool = nullptr;

// scoped_pool_binding 实现
scoped_pool_binding::scoped_pool_binding(thread_pool* pool)
    : m_old_pool(g_bound_pool) {
    g_bound_pool = pool;
}

scoped_pool_binding::~scoped_pool_binding() {
    g_bound_pool = m_old_pool;
}

thread_pool* scoped_pool_binding::current_bound_pool() {
    return g_bound_pool;
}

runtime_executor::runtime_executor()
    : m_context(&mc::singleton<runtime_context>::instance()) {
}

runtime_executor::runtime_executor(runtime_context& ctx)
    : m_context(&ctx) {
}

bool runtime_executor::operator==(const runtime_executor& other) const noexcept {
    return m_context == other.m_context;
}

bool runtime_executor::operator!=(const runtime_executor& other) const noexcept {
    return m_context != other.m_context;
}

boost::asio::execution_context& runtime_executor::context() const noexcept {
    return select_pool();
}

void runtime_executor::on_work_started() const noexcept {
    select_pool().get_executor().on_work_started();
}

void runtime_executor::on_work_finished() const noexcept {
    select_pool().get_executor().on_work_finished();
}

runtime_context& runtime_executor::get_runtime_context() const noexcept {
    return *m_context;
}

bool runtime_executor::running_in_this_thread() const noexcept {
    if (auto* shard = thread_pool::get_current_shard()) {
        if (shard->pool != nullptr) {
            // 指定了绑定 pool 则必须精确匹配
            if (g_bound_pool != nullptr) {
                return g_bound_pool == shard->pool;
            }

            // 否则任何一个 pool 都行
            return shard->pool == &m_context->io() || shard->pool == &m_context->work();
        }
    }
    return false;
}

thread_pool& runtime_executor::select_pool() const {
    // 优先级1：使用 scoped_pool_binding 绑定的 pool
    if (g_bound_pool != nullptr) {
        return *g_bound_pool;
    }

    // 优先级2：使用当前线程所在的 pool
    if (auto* shard = thread_pool::get_current_shard()) {
        if (shard->pool != nullptr) {
            return *shard->pool;
        }
    }

    // 优先级3：默认使用 io pool
    return m_context->io();
}

boost::asio::io_context::executor_type runtime_executor::select_io_executor() const {
    // 优先级1：使用 scoped_pool_binding 绑定的 pool
    if (g_bound_pool != nullptr) {
        static std::atomic<std::size_t> bound_round_robin{0};
        auto                            idx = bound_round_robin.fetch_add(1, std::memory_order_relaxed) % g_bound_pool->shard_count();
        return g_bound_pool->get_shard(idx).get_executor();
    }

    // 优先级2：使用当前线程所在的 shard
    if (auto* shard = thread_pool::get_current_shard()) {
        return shard->ctx->get_executor();
    }

    // 优先级3：默认使用 io pool
    auto&                           pool = m_context->io();
    static std::atomic<std::size_t> round_robin{0};
    auto                            idx = round_robin.fetch_add(1, std::memory_order_relaxed) % pool.shard_count();
    return pool.get_shard(idx).get_executor();
}

runtime_executor::operator boost::asio::any_io_executor() const {
    return select_io_executor();
}

runtime_executor::operator boost::asio::io_context::executor_type() const {
    return select_io_executor();
}

} // namespace mc::runtime
