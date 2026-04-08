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

#ifndef MC_FUTURES_STATE_H
#define MC_FUTURES_STATE_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <variant>

#include <mc/futures/callback_list.h>
#include <mc/futures/exceptions.h>
#include <mc/futures/status.h>
#include <mc/memory.h>
#include <mc/runtime/any_executor.h>
#include <mc/runtime/condition_variable.h>

namespace mc::futures {

namespace detail {
template <typename T>
using state_tt = std::conditional_t<std::is_same_v<mc::traits::remove_cvref_t<T>, std::monostate>, void,
                                    mc::traits::remove_cvref_t<T>>;
} // namespace detail

class MC_API state_base : public shared_base {
public:
    using executor_type = mc::runtime::any_executor;
    using destory_type  = void (*)(state_base*);

    state_base(executor_type executor, destory_type destory, int value_size) noexcept;
    ~state_base();

    void set_executor(executor_type executor) noexcept;
    void reset();
    void mark_ready();
    void cancel();

    template <typename Executor>
    void reuse(Executor&& executor)
    {
        set_executor(executor_type(std::forward<Executor>(executor)));
        reuse_impl();
    }

    // 设置取消回调
    // 注意：回调函数会在取消时立即执行，不会投递到 executor 执行，不要在回调函数中作阻塞动作
    void add_cancel_callback(callback_type callback);

    inline bool is_ready() const noexcept
    {
        return m_ready.load(std::memory_order_acquire);
    }

    inline bool is_cancelled() const noexcept
    {
        return m_cancelled.load(std::memory_order_acquire);
    }

    inline bool is_rejected() const noexcept
    {
        return m_exception != nullptr;
    }

    inline bool is_bound() const noexcept
    {
        return m_bound.load(std::memory_order_acquire);
    }

    inline std::exception_ptr get_exception() const noexcept
    {
        return m_exception;
    }

    inline void set_exception(std::exception_ptr exception) noexcept
    {
        m_exception = std::move(exception);
    }

    inline void set_ready()
    {
        m_ready.store(true, std::memory_order_release);
    }

    void           destory();
    inline int32_t value_size() const noexcept
    {
        return m_value_size;
    }

    inline executor_type get_executor() const noexcept
    {
        return m_executor;
    }

protected:
    void reuse_impl();

    using condition_variable = mc::runtime::condition_variable;

    friend class any_future;
    friend class any_promise;

    std::atomic<bool>  m_ready{false};
    std::atomic<bool>  m_bound{false};
    std::atomic<bool>  m_cancelled{false};
    launch             m_policy{launch::async};
    callback_list      m_continuations;
    callback_list      m_cancel_callbacks;
    std::exception_ptr m_exception{nullptr};

    executor_type m_executor;
    destory_type  m_destory{nullptr};
    const int32_t m_value_size;

    mutable std::mutex m_mutex;
    condition_variable m_cv;
};

template <typename T>
class State : public state_base {
public:
    using value_type  = std::conditional_t<std::is_same_v<T, void>, std::monostate, std::decay_t<T>>;
    using result_type = std::optional<value_type>;

    template <typename Executor>
    State(Executor executor) : state_base(std::forward<Executor>(executor), get_destory_(), sizeof(result_type))
    {}

    void reset()
    {
        state_base::reset();
        m_result = std::nullopt;
    }

    template <typename Executor>
    void reuse(Executor executor)
    {
        static_assert(std::is_same_v<Executor, executor_type> || boost::asio::is_executor<Executor>::value,
                      "reuse() 参数必须是 executor 类型");
        // state_base::destory() 调用链末尾的 state_base::reset() 会将 m_destory 清为
        // nullptr。重新从池中取出时必须在此恢复，否则第二次释放时 destory_impl 不会被调
        // 用，m_result 持有的非平凡类型（如 mc::dbus::message 的 DBusMessage* 引用计数）
        // 将永远得不到析构，造成内存泄漏。
        m_destory = get_destory_();
        new (static_cast<result_type*>(&m_result)) result_type();
        state_base::reuse(executor_type(std::move(executor)));
    }

    template <typename U = std::conditional_t<std::is_same_v<T, void>, std::monostate, T>>
    void set_value(U&& value)
    {
        m_result.emplace(std::forward<U>(value));
    }

    void set_value()
    {
        static_assert(std::is_same_v<T, void>, "set_value() 只能用于 void 类型");
        m_result.emplace(std::monostate{});
    }

    auto get_value() const -> std::conditional_t<std::is_same_v<T, void>, void, const value_type&>
    {
        std::lock_guard lock(m_mutex);
        if (is_rejected()) {
            std::rethrow_exception(get_exception());
        }
        MC_ASSERT_THROW(is_ready(), mc::runtime_exception, "Future 未就绪");
        if constexpr (std::is_same_v<T, void>) {
            return;
        } else {
            return *m_result;
        }
    }

private:
    static inline destory_type get_destory_() noexcept
    {
        if constexpr (std::is_trivially_destructible_v<result_type>) {
            return nullptr;
        }
        return &State::destory_impl;
    }

    static void destory_impl(state_base* ptr)
    {
        auto* state = static_cast<State*>(ptr);
        state->reset();
    }

    friend class any_future;
    friend class any_promise;

    result_type m_result;
};

} // namespace mc::futures

#endif // MC_FUTURES_STATE_H
