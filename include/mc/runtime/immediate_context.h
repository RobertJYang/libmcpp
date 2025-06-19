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

#ifndef MC_RUNTIME_IMMEDIATE_CONTEXT_H
#define MC_RUNTIME_IMMEDIATE_CONTEXT_H

#include <mc/common.h>

#include <boost/asio.hpp>

namespace mc::runtime {

class immediate_context;
class immediate_executor {
public:
    immediate_executor()  = default;
    ~immediate_executor() = default;

    immediate_executor(const immediate_executor&)            = default;
    immediate_executor& operator=(const immediate_executor&) = default;

    // 在当前线程立即执行工作
    template <typename F>
    void execute(F&& f) const {
        std::forward<F>(f)();
    }

    bool operator==(const immediate_executor&) const noexcept {
        return true;
    }

    bool operator!=(const immediate_executor&) const noexcept {
        return false;
    }

    // 在当前线程立即派发工作
    template <typename F, typename Allocator>
    void dispatch(F&& f, const Allocator& alloc = Allocator()) const {
        std::forward<F>(f)();
    }

    // 在当前线程立即投递工作
    template <typename F, typename Allocator>
    void post(F&& f, const Allocator& alloc = Allocator()) const {
        std::forward<F>(f)();
    }

    // 延迟执行（在这里等同于立即执行）
    template <typename F, typename Allocator>
    void defer(F&& f, const Allocator& alloc = Allocator()) const {
        std::forward<F>(f)();
    }

    void on_work_started() const noexcept {
    }

    void on_work_finished() const noexcept {
    }

    immediate_context& context() const noexcept;

    // 支持 execution::blocking.never 属性
    immediate_executor require(boost::asio::execution::blocking_t::never_t) const {
        return *this;
    }

    // 查询执行器属性
    static constexpr boost::asio::execution::blocking_t query(boost::asio::execution::blocking_t) {
        return boost::asio::execution::blocking.never;
    }
};

// immediate_context 用于立即执行回调的上下文
class immediate_context {
public:
    using executor_type = immediate_executor;

    immediate_context() = default;

    executor_type get_executor() const noexcept {
        return executor_type();
    }
};

inline immediate_context& immediate_executor::context() const noexcept {
    static immediate_context ctx;
    return ctx;
}

} // namespace mc::runtime

namespace boost::asio {
template <>
struct is_executor<mc::runtime::immediate_executor> : std::true_type {};
} // namespace boost::asio

#endif // MC_RUNTIME_IMMEDIATE_CONTEXT_H