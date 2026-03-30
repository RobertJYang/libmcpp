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

#ifndef MC_RUNTIME_RUNTIME_CONTEXT_H
#define MC_RUNTIME_RUNTIME_CONTEXT_H

#include <mc/common.h>

#include <mc/runtime/any_executor.h>
#include <mc/runtime/thread_pool.h>

#include <functional>
#include <memory>

namespace mc::runtime {

class runtime_context;

struct runtime_config {
    std::size_t io_threads   = 2;
    std::size_t work_threads = 2;
};

/**
 * @brief Runtime Context
 * 管理IO和Work线程池，以及任务调度。
 */
class MC_API runtime_context {
public:
    runtime_context();
    ~runtime_context();

    runtime_context(const runtime_context&)            = delete;
    runtime_context& operator=(const runtime_context&) = delete;

    void initialize(const runtime_config& config = runtime_config{});
    void start();
    void stop();
    void join();
    bool is_stopped() const noexcept;

    thread_pool& io() noexcept;
    thread_pool& work() noexcept;

    any_executor get_executor() noexcept;
    any_executor get_io_executor() noexcept;
    any_executor get_work_executor() noexcept;

    static any_executor create_strand();
    any_executor        create_io_strand();
    any_executor        create_work_strand();

private:
    class impl;
    std::unique_ptr<impl> m_impl;
};

/**
 * @brief 获取全局运行时上下文
 * @return 全局运行时上下文的引用
 */
MC_API runtime_context& get_runtime_context();
MC_API void             reset_runtime_context();

/**
 * @brief 获取IO上下文
 * @return IO上下文
 */
inline thread_pool& get_io_context()
{
    return get_runtime_context().io();
}

/**
 * @brief 获取工作上下文
 * @return 工作上下文
 */
inline thread_pool& get_work_context()
{
    return get_runtime_context().work();
}

/**
 * @brief 获取默认线程池
 * @return 默认线程池
 */
MC_API any_executor get_default_executor();

/**
 * @brief 获取IO线程池
 * @return IO线程池
 */
inline any_executor get_io_executor()
{
    return get_io_context().get_executor();
}

/**
 * @brief 获取系统线程池
 * @return 系统线程池
 */
inline any_executor get_work_executor()
{
    return get_work_context().get_executor();
}

} // namespace mc::runtime

#endif // MC_RUNTIME_RUNTIME_CONTEXT_H
