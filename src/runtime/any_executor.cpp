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

#include <mc/runtime/any_executor.h>
#include <mc/runtime/executor.h>
#include <mc/runtime/runtime_context.h>

namespace mc::runtime {

any_executor::any_executor()
    : m_executor(immediate_executor()) {
}

any_executor::any_executor(thread_pool::executor_type executor)
    : m_executor(std::move(executor)) {
}

any_executor::any_executor(runtime_strand executor)
    : m_executor(std::move(executor)) {
}

any_executor::any_executor(runtime::executor executor)
    : m_executor(std::move(executor)) {
}

bool any_executor::valid() const noexcept {
    return std::visit([](const auto& exec) -> bool {
        using T = std::decay_t<decltype(exec)>;
        if constexpr (std::is_same_v<T, runtime::executor>) {
            return exec.valid();
        } else {
            return true; // boost::asio 执行器总是有效的
        }
    }, m_executor);
}

bool any_executor::operator==(const any_executor& other) const noexcept {
    // 如果类型不同，则不相等
    if (m_executor.index() != other.m_executor.index()) {
        return false;
    }

    return std::visit([&other](const auto& exec) -> bool {
        using T                = std::decay_t<decltype(exec)>;
        const auto& other_exec = std::get<T>(other.m_executor);
        return exec == other_exec;
    }, m_executor);
}

bool any_executor::operator!=(const any_executor& other) const noexcept {
    return !(*this == other);
}

void any_executor::on_work_started() const noexcept {
    std::visit([&](const auto& exec) {
        exec.on_work_started();
    }, m_executor);
}

void any_executor::on_work_finished() const noexcept {
    std::visit([&](const auto& exec) {
        exec.on_work_finished();
    }, m_executor);
}

execution_context& any_executor::context() const {
    return std::visit([&](const auto& exec) -> execution_context& {
        return exec.context();
    }, m_executor);
}

namespace detail {
// 检测是否有 running_in_this_thread() 方法
template <typename T, typename = void>
struct has_running_in_this_thread : std::false_type {};

template <typename T>
struct has_running_in_this_thread<T, std::void_t<decltype(std::declval<T>().running_in_this_thread())>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_running_in_this_thread_v = has_running_in_this_thread<T>::value;
} // namespace detail

bool any_executor::running_in_this_thread() const noexcept {
    return std::visit([](const auto& exec) -> bool {
        using T = std::decay_t<decltype(exec)>;
        if constexpr (detail::has_running_in_this_thread_v<T>) {
            return exec.running_in_this_thread();
        } else {
            return false; // 保守返回 false，走异步路径
        }
    }, m_executor);
}

any_executor& any_executor::bound_pool(thread_pool* pool) noexcept {
    std::visit([&](auto& exec) {
        using T = std::decay_t<decltype(exec)>;
        if constexpr (detail::has_bound_pool_v<T>) {
            exec.bound_pool(pool);
        }
    }, m_executor);
    return *this;
}

thread_pool* any_executor::get_bound_pool() const noexcept {
    return std::visit([](const auto& exec) -> thread_pool* {
        using T = std::decay_t<decltype(exec)>;
        if constexpr (detail::has_bound_pool_v<T>) {
            return exec.get_bound_pool();
        }
        return nullptr;
    }, m_executor);
}

any_executor::operator boost::asio::any_io_executor() const {
    return std::visit([](const auto& exec) -> boost::asio::any_io_executor {
        using T = std::decay_t<decltype(exec)>;
        if constexpr (detail::can_convert_to_any_io_executor_v<T>) {
            return exec;
        }
        // 如果不能转换，则返回 IO 线程池的执行器
        return runtime::get_io_executor();
    }, m_executor);
}

any_executor::operator boost::asio::io_context::executor_type() const {
    return std::visit([](const auto& exec) -> boost::asio::io_context::executor_type {
        using T = std::decay_t<decltype(exec)>;
        if constexpr (detail::can_convert_to_io_executor_v<T>) {
            return exec;
        }
        // 如果不能转换，则返回 IO 线程池的执行器
        return runtime::get_io_executor();
    }, m_executor);
}

} // namespace mc::runtime
