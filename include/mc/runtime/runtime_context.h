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

#include <functional>
#include <memory>

namespace mc::runtime {
using io_context   = boost::asio::io_context;
using work_context = boost::asio::io_context;
using io_strand    = boost::asio::strand<io_context::executor_type>;
using work_strand  = boost::asio::strand<work_context::executor_type>;

struct runtime_config {
    std::size_t io_threads   = 2;
    std::size_t work_threads = 2;
};

/**
 * @brief 运行时上下文
 *
 * 内部管理多种执行器，对外提供统一接口。支持：
 * 1. IO执行器：基于 io_context，用于网络IO等事件驱动操作
 * 2. 工作执行器：基于 io_context，用于阻塞操作（如硬件IO）
 */
class MC_API runtime_context {
public:
    /**
     * @brief 构造函数
     */
    runtime_context();

    /**
     * @brief 析构函数
     */
    ~runtime_context();

    /**
     * @brief 禁止拷贝构造
     */
    runtime_context(const runtime_context&) = delete;

    /**
     * @brief 禁止拷贝赋值
     */
    runtime_context& operator=(const runtime_context&) = delete;

    /**
     * @brief 初始化运行时上下文
     * @param io_threads IO线程数，默认为1，0 表示使用 std::thread::hardware_concurrency() 的值
     */
    void initialize(const runtime_config& config = runtime_config{});

    /**
     * @brief 启动运行时上下文
     */
    void start();

    /**
     * @brief 停止运行时上下文
     */
    void stop();

    /**
     * @brief 等待所有线程结束
     */
    void join();

    /**
     * @brief 检查是否已停止
     * @return 如果已停止则返回true
     */
    bool is_stopped() const noexcept;

    /**
     * @brief 获取IO上下文
     * @return IO上下文引用
     */
    io_context& get_io_context() noexcept;

    /**
     * @brief 获取工作上下文
     * @return 工作上下文引用
     */
    work_context& get_work_context() noexcept;

    /**
     * @brief 获取IO执行器
     * @return IO执行器
     */
    io_context::executor_type get_io_executor() noexcept;

    /**
     * @brief 获取工作执行器
     * @return 工作执行器
     */
    work_context::executor_type get_work_executor() noexcept;

    /**
     * @brief 创建IO strand执行器
     * @return IO strand执行器
     */
    io_strand make_io_strand() noexcept;

    /**
     * @brief 创建工作strand执行器
     * @return 工作strand执行器
     */
    work_strand make_work_strand() noexcept;

private:
    class impl;
    std::unique_ptr<impl> m_impl;

    friend class executor_type;
};

} // namespace mc::runtime

#endif // MC_RUNTIME_RUNTIME_CONTEXT_H