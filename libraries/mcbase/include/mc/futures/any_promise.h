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

#ifndef MC_FUTURES_ANY_PROMISE_H
#define MC_FUTURES_ANY_PROMISE_H

#include <mc/error.h>
#include <mc/exception.h>
#include <mc/futures/any_future.h>
#include <mc/futures/exceptions.h>
#include <mc/futures/state_pool.h>
#include <mc/futures/status.h>
#include <mc/runtime.h>

namespace mc::futures {

namespace detail {

template <typename T, typename = void>
struct is_promise : std::false_type {};

template <typename T>
struct is_promise<T, std::void_t<typename mc::traits::remove_cvref_t<T>::is_promise>> : std::true_type {};

template <typename T>
constexpr bool is_promise_v = is_promise<std::remove_cv_t<T>>::value;

} // namespace detail

class MC_API any_promise {
    friend class any_future;
    template <typename T>
    friend class Promise;

public:
    using is_promise = std::true_type;

    any_promise() = default;
    explicit any_promise(state_base_ptr state);

    any_promise(const any_promise&)                = default;
    any_promise& operator=(const any_promise&)     = default;
    any_promise(any_promise&&) noexcept            = default;
    any_promise& operator=(any_promise&&) noexcept = default;

    operator bool() const
    {
        return m_state != nullptr;
    }

    void reset();
    void cancel();

    void set_exception(const mc::exception& e, bool strict_once = true);
    void set_current_exception(bool strict_once = true);

    template <typename Exception,
              std::enable_if_t<!std::is_same_v<mc::traits::remove_cvref_t<Exception>, mc::exception>, int> = 0>
    void set_exception(Exception&& e, bool strict_once = true)
    {
        try {
            throw std::forward<Exception>(e);
        } catch (const mc::exception& mc_ex) {
            set_exception(mc_ex, strict_once);
        } catch (...) {
            set_current_exception(strict_once);
        }
    }

    inline launch get_policy() const noexcept
    {
        return m_state ? m_state->m_policy : launch::async;
    }
    inline void set_policy(launch policy) noexcept
    {
        if (m_state) {
            m_state->m_policy = policy;
        }
    }

    inline bool is_bound() const noexcept
    {
        return m_state->is_bound();
    }
    inline bool is_ready() const noexcept
    {
        return m_state->is_ready();
    }
    inline bool is_cancelled() const noexcept
    {
        return m_state->is_cancelled();
    }
    inline bool is_rejected() const noexcept
    {
        return m_state->is_rejected();
    }

    const state_base_ptr& get_state() const noexcept;

    void set_executor(const any_executor& executor);

protected:
    template <typename T, typename U = T>
    auto set_value(U&& value, bool strict_once = true) -> std::enable_if_t<!detail::is_future_v<T>, void>
    {
        if (!m_state || m_state->is_cancelled()) {
            return;
        }

        {
            std::lock_guard lock(m_state->m_mutex);
            set_result_inner(strict_once);
            static_cast<State<T>*>(m_state.get())->set_value(std::forward<U>(value));
        }

        m_state->mark_ready();
    }

    void set_value(bool strict_once = true)
    {
        if (!m_state || m_state->is_cancelled()) {
            return;
        }

        {
            std::lock_guard lock(m_state->m_mutex);
            set_result_inner(strict_once);
            static_cast<State<void>*>(m_state.get())->set_value();
        }

        m_state->mark_ready();
    }

    bool       set_bound();
    any_future get_future();

private:
    void set_result_inner(bool strict_once);

    state_base_ptr m_state;
    bool           m_future_retrieved{false};
};

} // namespace mc::futures

#endif // MC_FUTURES_ANY_PROMISE_H
