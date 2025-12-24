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

#include <boost/asio.hpp>
#include <mc/runtime/runtime_executor.h>
#include <mc/runtime/thread_pool.h>

#include <functional>
#include <memory>

namespace mc::runtime {

class runtime_context;

using strand = boost::asio::strand<runtime_executor>;

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

    runtime_executor           get_executor() noexcept;
    thread_pool::executor_type get_io_executor() noexcept;
    thread_pool::executor_type get_work_executor() noexcept;

    static strand       create_strand();
    thread_pool::strand create_io_strand();
    thread_pool::strand create_work_strand();

private:
    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace mc::runtime

#endif // MC_RUNTIME_RUNTIME_CONTEXT_H
