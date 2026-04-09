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

#ifndef MC_FUTURES_DETAIL_COMBINATOR_H
#define MC_FUTURES_DETAIL_COMBINATOR_H

#include <mc/common.h>
#include <mc/futures/future.h>
#include <mc/futures/promise.h>
#include <memory>

namespace mc::futures {

namespace detail {

class MC_API CombinatorState {
public:
    CombinatorState(std::size_t total_count, any_promise promise);

    void add_cancel_callback(any_future& future);
    void execute_cancel_callbacks();

protected:
    std::mutex    m_mutex;
    std::size_t   m_total_count;
    any_promise   m_promise;
    callback_list m_cancel_callbacks;
};

class MC_API AllStateBase : public CombinatorState {
public:
    AllStateBase(std::size_t total_count, any_promise promise);

    void set_exception(const mc::exception& e);
    bool set_completed(std::size_t index);
    void cancel();

protected:
    std::size_t m_completed_count = 0;
    bool        m_first_exception = false;
};

template <typename ResultType>
struct AllState : public AllStateBase {
    using future_type  = Future<ResultType>;
    using promise_type = Promise<ResultType>;
    ResultType results;

    AllState(std::size_t total_count, mc::any_executor executor)
        : AllStateBase(total_count, make_promise<ResultType>(std::move(executor)))
    {}

    template <typename ValueType, typename ForwardType>
    void set_value(std::size_t index, ValueType& result, ForwardType&& value)
    {
        result = std::forward<ForwardType>(value);
        if (!set_completed(index)) {
            return;
        }

        promise_type(any_promise(m_promise)).set_value(std::move(this->results));
    }

    future_type get_future()
    {
        return future_type(
            *reinterpret_cast<const state_ptr<typename future_type::state_type>*>(&m_promise.get_state()));
    }
};

class MC_API AnyStateBase : public CombinatorState {
public:
    AnyStateBase(std::size_t total_count, any_promise promise);

    void set_exception(const mc::exception& e);
    bool set_completed(std::size_t index);
    void cancel();

protected:
    bool              m_completed      = false;
    std::size_t       m_failed_count   = 0;       // 记录所有失败的future数量，包括取消和其他异常
    mc::exception_ptr m_last_exception = nullptr; // 记录最后一个异常
};

template <typename ResultType>
struct AnyState : public AnyStateBase {
    using result_type  = ResultType;
    using future_type  = Future<ResultType>;
    using promise_type = Promise<ResultType>;
    using variant_type = typename ResultType::second_type;

    AnyState(std::size_t total_count, mc::any_executor executor)
        : AnyStateBase(total_count, make_promise<ResultType>(std::move(executor)))
    {}

    template <typename ValueType>
    void set_value(std::size_t index, ValueType&& value)
    {
        if (!set_completed(index)) {
            return;
        }

        promise_type(any_promise(m_promise)).set_value(std::make_pair(index, variant_type(std::move(value))));
        execute_cancel_callbacks();
    }

    future_type get_future()
    {
        return future_type(
            *reinterpret_cast<const state_ptr<typename future_type::state_type>*>(&m_promise.get_state()));
    }
};

template <typename State, typename Future, typename Result>
void process_all(std::shared_ptr<State> state, std::size_t index, Result& result, Future& future)
{
    using result_type = typename Future::result_type;

    auto f = future
                 .then([state, index, &result](auto&& value) mutable {
        // 某些编译器对参数为 auto&& 的 lambda 函数用 std::monostate 作为参数调用，
        // 会错误的将 value 推导成 int 类型导致编译错误，这里特殊处理一下
        if constexpr (std::is_same_v<result_type, void> || std::is_same_v<result_type, std::monostate>) {
            MC_UNUSED(value);
            state->set_value(index, result, std::monostate{});
        } else {
            state->set_value(index, result, std::forward<decltype(value)>(value));
        }
    })
                 .catch_error([state](const mc::exception& e) mutable {
        state->set_exception(e);
    }).on_cancel([wstate = std::weak_ptr(state)]() mutable {
        if (auto state = wstate.lock()) {
            state->cancel();
        }
    });
    state->add_cancel_callback(f);
}

template <std::size_t I, std::size_t N, typename State, typename Futures>
void process_all(std::shared_ptr<State> state, Futures& futures)
{
    if constexpr (I < N) {
        process_all(state, I, std::get<I>(state->results), std::get<I>(futures));
        process_all<I + 1, N>(state, futures);
    }
}

template <typename State, typename Future>
void process_any(std::shared_ptr<State> state, std::size_t index, Future& future)
{
    using result_type = typename Future::result_type;

    auto f = future
                 .then([state, index](auto&& value) mutable {
        // 某些编译器对参数为 auto&& 的 lambda 函数用 std::monostate 作为参数调用，
        // 会错误的将 value 推导成 int 类型导致编译错误，这里特殊处理一下
        if constexpr (std::is_same_v<result_type, void> || std::is_same_v<result_type, std::monostate>) {
            MC_UNUSED(value);
            state->set_value(index, std::monostate{});
        } else {
            state->set_value(index, std::forward<decltype(value)>(value));
        }
    })
                 .catch_error([state](const mc::exception& e) mutable {
        state->set_exception(e);
    }).on_cancel([wstate = std::weak_ptr(state)]() {
        if (auto state = wstate.lock()) {
            state->cancel();
        }
    });
    state->add_cancel_callback(f);
}

template <std::size_t I, std::size_t N, typename State, typename Futures>
void process_any(std::shared_ptr<State> state, Futures& futures)
{
    if constexpr (I < N) {
        process_any(state, I, std::get<I>(futures));
        process_any<I + 1, N>(state, futures);
    }
}

} // namespace detail

// 可变参数版本的 all 函数实现
template <typename... Futures>
auto all(Futures&&... futures) -> Future<std::tuple<typename std::decay_t<Futures>::result_type...>>
{
    static_assert(sizeof...(Futures) > 0, "all requires at least one future");

    using ResultType = std::tuple<typename std::decay_t<Futures>::result_type...>;

    auto  tuple_futures = std::forward_as_tuple(futures...);
    auto& first         = std::get<0>(tuple_futures);
    using State         = detail::AllState<ResultType>;
    auto state          = std::make_shared<State>(sizeof...(Futures), first.get_executor());
    auto result         = state->get_future();

    // 当 result 被取消时，取消所有子 future
    result.on_cancel([wstate = std::weak_ptr(state)]() {
        if (auto state = wstate.lock()) {
            state->execute_cancel_callbacks();
        }
    });

    detail::process_all<0, sizeof...(Futures)>(state, tuple_futures);
    return result;
}

// 容器版本的 all 函数实现
template <typename Iterator>
auto all(Iterator begin,
         Iterator end) -> Future<std::vector<typename std::iterator_traits<Iterator>::value_type::result_type>>
{
    using future_type = typename std::iterator_traits<Iterator>::value_type;
    using value_type  = typename future_type::result_type;
    using ResultType  = std::vector<value_type>;
    using State       = detail::AllState<ResultType>;

    MC_ASSERT(begin != end, "all requires at least one future");
    auto total  = std::distance(begin, end);
    auto state  = std::make_shared<State>(total, begin->get_executor());
    auto result = state->get_future();

    std::size_t index = 0;
    state->results.resize(total);
    for (auto it = begin; it != end; ++it) {
        auto idx = index++;
        detail::process_all(state, idx, state->results[idx], *it);
    }

    result.on_cancel([wstate = std::weak_ptr(state)]() {
        if (auto state = wstate.lock()) {
            state->execute_cancel_callbacks();
        }
    });

    return result;
}

// any 实现：等待任意一个 future 完成
template <typename... Futures>
auto any(Futures&&... futures)
    -> Future<std::pair<
        std::size_t,
        mc::traits::apply_type_t<std::variant, mc::traits::type_set_t<typename std::decay_t<Futures>::result_type...>>>>
{
    static_assert(sizeof...(Futures) > 0, "any requires at least one future");

    using VariantType =
        mc::traits::apply_type_t<std::variant, mc::traits::type_set_t<typename std::decay_t<Futures>::result_type...>>;
    using ResultType = std::pair<std::size_t, VariantType>;
    using State      = detail::AnyState<ResultType>;

    auto  tuple_futures = std::forward_as_tuple(futures...);
    auto& first         = std::get<0>(tuple_futures);
    auto  state         = std::make_shared<State>(sizeof...(Futures), first.get_executor());
    auto  result        = state->get_future();

    // 当 result 被取消时，取消所有子 future
    result.on_cancel([wstate = std::weak_ptr(state)]() {
        if (auto state = wstate.lock()) {
            state->execute_cancel_callbacks();
        }
    });

    detail::process_any<0, sizeof...(Futures)>(state, tuple_futures);
    return result;
}

// 容器版本的 any 函数实现
template <typename Iterator>
auto any(Iterator begin, Iterator end)
    -> Future<std::pair<std::size_t, typename std::iterator_traits<Iterator>::value_type::result_type>>
{
    using future_type = typename std::iterator_traits<Iterator>::value_type;
    using value_type  = typename future_type::result_type;
    using ResultType  = std::pair<std::size_t, value_type>;
    using State       = detail::AnyState<ResultType>;

    MC_ASSERT(begin != end, "Empty future sequence");
    auto total  = std::distance(begin, end);
    auto state  = std::make_shared<State>(total, begin->get_executor());
    auto result = state->get_future();

    std::size_t index = 0;
    for (auto it = begin; it != end; ++it) {
        detail::process_any(state, index++, *it);
    }

    result.on_cancel([wstate = std::weak_ptr(state)]() {
        if (auto state = wstate.lock()) {
            state->execute_cancel_callbacks();
        }
    });

    return result;
}

} // namespace mc::futures

#endif // MC_FUTURES_DETAIL_COMBINATOR_H