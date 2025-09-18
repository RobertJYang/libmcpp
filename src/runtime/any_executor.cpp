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

namespace mc::runtime {

any_executor::any_executor(boost::asio::io_context::executor_type executor)
    : m_executor(std::move(executor)) {
}

any_executor::any_executor(boost::asio::system_context::executor_type executor)
    : m_executor(std::move(executor)) {
}

any_executor::any_executor(runtime::executor executor)
    : m_executor(std::move(executor)) {
}

bool any_executor::valid() const noexcept {
    return std::visit([](const auto& exec) -> bool {
        using T = std::decay_t<decltype(exec)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return false;
        } else if constexpr (std::is_same_v<T, runtime::executor>) {
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
        using T = std::decay_t<decltype(exec)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return true; // 两个都是无效状态
        } else {
            const auto& other_exec = std::get<T>(other.m_executor);
            return exec == other_exec;
        }
    }, m_executor);
}

bool any_executor::operator!=(const any_executor& other) const noexcept {
    return !(*this == other);
}

void any_executor::on_work_started() const noexcept {
    std::visit([&](const auto& exec) {
        if constexpr (!std::is_same_v<std::decay_t<decltype(exec)>, std::monostate>) {
            exec.on_work_started();
        }
    }, m_executor);
}

void any_executor::on_work_finished() const noexcept {
    std::visit([&](const auto& exec) {
        if constexpr (!std::is_same_v<std::decay_t<decltype(exec)>, std::monostate>) {
            exec.on_work_finished();
        }
    }, m_executor);
}

execution_context& any_executor::context() const {
    return std::visit([&](const auto& exec) -> execution_context& {
        if constexpr (std::is_same_v<std::decay_t<decltype(exec)>, std::monostate>) {
            MC_THROW(mc::invalid_op_exception, "Cannot get context from invalid executor");
        } else {
            return exec.context();
        }
    }, m_executor);
}

} // namespace mc