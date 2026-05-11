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
    void add_cancel_callback(const state_base_ptr& state);
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

template <typename FutureType>
FutureType get_future_from_promise(any_promise& promise);

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
        return get_future_from_promise<future_type>(m_promise);
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

template <typename FutureType>
FutureType get_future_from_promise(any_promise& promise)
{
    return FutureType(promise.get_state());
}

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
        return get_future_from_promise<future_type>(m_promise);
    }
};

template <typename ResultType>
void handle_child_cancel(const std::shared_ptr<AllState<ResultType>>& state)
{
    state->cancel();
}

template <typename ResultType>
void handle_child_cancel(const std::shared_ptr<AnyState<ResultType>>& state)
{
    state->set_exception(mc::canceled_exception());
}

template <typename State>
auto make_child_cancel_callback(const std::shared_ptr<State>& state)
{
    return [wstate = std::weak_ptr(state)]() mutable {
        if (auto current_state = wstate.lock()) {
            handle_child_cancel(current_state);
        }
    };
}

template <typename State>
auto make_result_cancel_callback(const std::shared_ptr<State>& state)
{
    return [wstate = std::weak_ptr(state)]() mutable {
        if (auto current_state = wstate.lock()) {
            current_state->execute_cancel_callbacks();
        }
    };
}

template <typename ResultFuture, typename State>
void bind_result_cancel(ResultFuture& result, const std::shared_ptr<State>& state)
{
    result.on_cancel(make_result_cancel_callback(state));
}

template <typename State>
auto make_combined_state(std::size_t total_count, mc::any_executor executor)
{
    auto state  = std::make_shared<State>(total_count, std::move(executor));
    auto result = state->get_future();
    bind_result_cancel(result, state);
    return std::make_pair(std::move(state), std::move(result));
}

template <typename State, typename Future, typename SuccessHandler>
void observe_child_future(const std::shared_ptr<State>& state, Future& future, SuccessHandler&& on_success)
{
    using result_type = typename Future::result_type;
    auto child_state  = future.get_state();

    future.add_continuation([state, child_state, success = std::forward<SuccessHandler>(on_success)]() mutable {
        if (!child_state) {
            return;
        }

        if (child_state->is_cancelled()) {
            return;
        }

        if (child_state->is_rejected()) {
            auto exception = child_state->get_exception_object();
            if (exception) {
                state->set_exception(*exception);
            }
            return;
        }

        if constexpr (std::is_same_v<result_type, void> || std::is_same_v<result_type, std::monostate>) {
            success(std::monostate{});
        } else {
            success(mc::futures::State<result_type>::get_value(*child_state));
        }
    });
    future.on_cancel(make_child_cancel_callback(state));
    state->add_cancel_callback(child_state);
}

template <std::size_t I, std::size_t N, typename Futures, typename Observer>
void process_tuple_futures(Futures& futures, Observer&& observer)
{
    if constexpr (I < N) {
        observer(std::integral_constant<std::size_t, I>{}, std::get<I>(futures));
        process_tuple_futures<I + 1, N>(futures, std::forward<Observer>(observer));
    }
}

template <typename Iterator, typename Observer>
void process_future_range(Iterator begin, Iterator end, Observer&& observer)
{
    std::size_t index = 0;
    for (auto it = begin; it != end; ++it) {
        observer(index++, *it);
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
    auto combined       = detail::make_combined_state<State>(sizeof...(Futures), first.get_executor());
    auto state          = std::move(combined.first);
    auto result         = std::move(combined.second);
    detail::process_tuple_futures<0, sizeof...(Futures)>(tuple_futures, [state](auto index_tag, auto& future) mutable {
        constexpr auto index = decltype(index_tag)::value;
        detail::observe_child_future(
            state, future, [state, index, &result_slot = std::get<index>(state->results)](auto&& value) mutable {
            state->set_value(index, result_slot, std::forward<decltype(value)>(value));
        });
    });
    return result;
}

// 容器版本的 all 函数实现
template <typename Iterator>
auto all(Iterator begin, Iterator end)
    -> Future<std::vector<typename std::iterator_traits<Iterator>::value_type::result_type>>
{
    using future_type = typename std::iterator_traits<Iterator>::value_type;
    using value_type  = typename future_type::result_type;
    using ResultType  = std::vector<value_type>;
    using State       = detail::AllState<ResultType>;

    MC_ASSERT(begin != end, "all requires at least one future");
    auto total    = std::distance(begin, end);
    auto combined = detail::make_combined_state<State>(total, begin->get_executor());
    auto state    = std::move(combined.first);
    auto result   = std::move(combined.second);

    std::size_t index = 0;
    state->results.resize(total);
    detail::process_future_range(begin, end, [state, &index](std::size_t, auto& future) mutable {
        auto  current_index = index++;
        auto& result_slot   = state->results[current_index];
        detail::observe_child_future(state, future, [state, current_index, &result_slot](auto&& value) mutable {
            state->set_value(current_index, result_slot, std::forward<decltype(value)>(value));
        });
    });

    return result;
}

// any 实现：等待任意一个 future 完成
template <typename... Futures>
auto any(Futures&&... futures) -> Future<std::pair<
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
    auto  combined      = detail::make_combined_state<State>(sizeof...(Futures), first.get_executor());
    auto  state         = std::move(combined.first);
    auto  result        = std::move(combined.second);
    detail::process_tuple_futures<0, sizeof...(Futures)>(tuple_futures, [state](auto index_tag, auto& future) mutable {
        constexpr auto index = decltype(index_tag)::value;
        detail::observe_child_future(state, future, [state, index](auto&& value) mutable {
            state->set_value(index, std::forward<decltype(value)>(value));
        });
    });
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
    auto total    = std::distance(begin, end);
    auto combined = detail::make_combined_state<State>(total, begin->get_executor());
    auto state    = std::move(combined.first);
    auto result   = std::move(combined.second);
    detail::process_future_range(begin, end, [state](std::size_t index, auto& future) mutable {
        detail::observe_child_future(state, future, [state, index](auto&& value) mutable {
            state->set_value(index, std::forward<decltype(value)>(value));
        });
    });

    return result;
}

} // namespace mc::futures

#endif // MC_FUTURES_DETAIL_COMBINATOR_H