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

#ifndef MC_RUNTIME_H
#define MC_RUNTIME_H

#include <mc/runtime/any_executor.h>
#include <mc/runtime/executor.h>
#include <mc/runtime/immediate_context.h>
#include <mc/runtime/runtime_context.h>
#include <mc/runtime/thread_list.h>

namespace mc::runtime {

struct io_executor_tag {};
struct work_executor_tag {};

constexpr auto io_executor   = io_executor_tag{};
constexpr auto work_executor = work_executor_tag{};

namespace detail {
template <typename ExecutorTag>
inline constexpr bool is_executor_tag = std::is_same_v<ExecutorTag, io_executor_tag> ||
                                        std::is_same_v<ExecutorTag, work_executor_tag>;
}

/**
 * @brief 获取全局运行时上下文
 * @return 全局运行时上下文的引用
 */
runtime_context& get_runtime_context();

/**
 * @brief 获取IO上下文
 * @return IO上下文
 */
inline io_context& get_io_context() {
    return get_runtime_context().get_io_context();
}

/**
 * @brief 获取工作上下文
 * @return 工作上下文
 */
inline work_context& get_work_context() {
    return get_runtime_context().get_work_context();
}

/**
 * @brief 获取默认执行器
 * @return 默认执行器（IO执行器）
 */
inline auto get_default_executor() {
    return get_runtime_context().get_io_executor();
}

/**
 * @brief 获取IO执行器
 * @return IO执行器
 */
inline auto get_io_executor() {
    return get_runtime_context().get_io_executor();
}

/**
 * @brief 获取系统执行器
 * @return 系统执行器
 */
inline auto get_work_executor() {
    return get_runtime_context().get_work_executor();
}

/**
 * @brief 便利函数 - 投递任务到默认执行器（IO执行器）
 * @tparam CompletionToken 完成令牌类型
 * @param token 完成令牌
 * @return 根据完成令牌类型返回相应的结果
 */
template <typename CompletionToken>
auto post(CompletionToken&& token) {
    return boost::asio::post(get_default_executor(), std::forward<CompletionToken>(token));
}

/**
 * @brief 便利函数 - 投递任务到指定执行器
 * @tparam CompletionToken 完成令牌类型
 * @tparam ExecutorTag 执行器标签类型
 * @param token 完成令牌
 * @param tag 执行器标签
 * @return 根据完成令牌类型返回相应的结果
 */
template <typename CompletionToken, typename ExecutorTag>
auto post(CompletionToken&& token, ExecutorTag) {
    if constexpr (std::is_same_v<ExecutorTag, io_executor_tag>) {
        return boost::asio::post(get_io_executor(), std::forward<CompletionToken>(token));
    } else if constexpr (std::is_same_v<ExecutorTag, work_executor_tag>) {
        return boost::asio::post(get_work_executor(), std::forward<CompletionToken>(token));
    } else {
        static_assert(detail::is_executor_tag<ExecutorTag>, "Invalid executor tag");
    }
}

/**
 * @brief 便利函数 - 延迟投递任务到默认执行器（IO执行器）
 * @tparam CompletionToken 完成令牌类型
 * @param token 完成令牌
 * @return 根据完成令牌类型返回相应的结果
 */
template <typename CompletionToken>
auto defer(CompletionToken&& token) {
    return boost::asio::defer(get_default_executor(), std::forward<CompletionToken>(token));
}

/**
 * @brief 便利函数 - 延迟投递任务到指定执行器
 * @tparam CompletionToken 完成令牌类型
 * @tparam ExecutorTag 执行器标签类型
 * @param token 完成令牌
 * @param tag 执行器标签
 * @return 根据完成令牌类型返回相应的结果
 */
template <typename CompletionToken, typename ExecutorTag>
auto defer(CompletionToken&& token, ExecutorTag) {
    if constexpr (std::is_same_v<ExecutorTag, io_executor_tag>) {
        return boost::asio::defer(get_io_executor(), std::forward<CompletionToken>(token));
    } else if constexpr (std::is_same_v<ExecutorTag, work_executor_tag>) {
        return boost::asio::defer(get_work_executor(), std::forward<CompletionToken>(token));
    } else {
        static_assert(detail::is_executor_tag<ExecutorTag>, "Invalid executor tag");
    }
}

/**
 * @brief 便利函数 - 分派任务到默认执行器（IO执行器）
 * @tparam CompletionToken 完成令牌类型
 * @param token 完成令牌
 * @return 根据完成令牌类型返回相应的结果
 */
template <typename CompletionToken>
auto dispatch(CompletionToken&& token) {
    return boost::asio::dispatch(get_default_executor(), std::forward<CompletionToken>(token));
}

/**
 * @brief 便利函数 - 分派任务到指定执行器
 * @tparam CompletionToken 完成令牌类型
 * @tparam ExecutorTag 执行器标签类型
 * @param token 完成令牌
 * @param tag 执行器标签
 * @return 根据完成令牌类型返回相应的结果
 */
template <typename CompletionToken, typename ExecutorTag>
auto dispatch(CompletionToken&& token, ExecutorTag) {
    if constexpr (std::is_same_v<ExecutorTag, io_executor_tag>) {
        return boost::asio::dispatch(get_io_executor(), std::forward<CompletionToken>(token));
    } else if constexpr (std::is_same_v<ExecutorTag, work_executor_tag>) {
        return boost::asio::dispatch(get_work_executor(), std::forward<CompletionToken>(token));
    } else {
        static_assert(detail::is_executor_tag<ExecutorTag>, "Invalid executor tag");
    }
}

/**
 * @brief 创建strand执行器
 * @return strand执行器
 */
inline auto make_io_strand() {
    return get_runtime_context().make_io_strand();
}

inline auto make_work_strand() {
    return get_runtime_context().make_work_strand();
}

} // namespace mc::runtime

namespace mc {
using runtime::any_executor;
using runtime::executor;
using runtime::immediate_executor;
using runtime::io_executor;
using runtime::work_executor;

using runtime::immediate_context;
using runtime::io_context;
using runtime::runtime_context;
using runtime::work_context;

using runtime::runtime_config;
using runtime::thread_list;
using runtime::thread_node;

using runtime::defer;
using runtime::dispatch;
using runtime::get_default_executor;
using runtime::get_io_context;
using runtime::get_io_executor;
using runtime::get_runtime_context;
using runtime::get_work_context;
using runtime::get_work_executor;
using runtime::make_io_strand;
using runtime::make_work_strand;
using runtime::post;

} // namespace mc

#endif // MC_RUNTIME_H