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

#include <memory>

namespace mc::futures {

namespace detail {

// CombinatorState - 包含 all 和 any 的公共部分
template <typename ResultType, typename Executor, typename Allocator>
struct CombinatorState {
    using promise_type = Promise<ResultType, Executor, Allocator>;

    std::mutex    mutex;
    std::size_t   total_count;
    promise_type  promise;
    Executor      executor;
    Allocator     allocator;
    callback_list cancel_callbacks;

    template <typename Future>
    CombinatorState(std::size_t total_count, const Future& future)
        : total_count(total_count), promise(make_promise(future)),
          executor(future.state_->executor), allocator(future.state_->allocator) {
    }

    template <typename Future>
    void add_cancel_callback(Future& future) {
        cancel_callbacks.push_back([state = std::weak_ptr(future.get_state())]() {
            if (auto s = state.lock()) {
                s->cancel();
            }
        });
    }

    template <typename Futures>
    void add_cancel_callbacks(Futures& futures) {
        mc::traits::tuple_for_each(futures, [&](auto& fut) {
            add_cancel_callback(fut);
        });
    }

    template <typename Iterator>
    void add_cancel_callbacks(Iterator begin, Iterator end) {
        for (auto it = begin; it != end; ++it) {
            add_cancel_callback(*it);
        }
    }

    void execute_cancel_callbacks() {
        callback_list callbacks;
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            callbacks.swap(cancel_callbacks);
        }

        callbacks.execute_and_clear();
    }

    template <typename Future>
    static auto make_promise(const Future& future) {
        return mc::make_promise<ResultType>(
            future.state_->executor, future.state_->allocator);
    }
};

// all 操作专用的 CombinatorState
template <typename ResultType, typename Executor, typename Allocator>
struct AllState : public CombinatorState<ResultType, Executor, Allocator> {
    using state_type = AllState<ResultType, Executor, Allocator>;

    std::size_t completed_count = 0;
    ResultType  results;
    bool        first_exception = false;

    template <typename Future>
    AllState(std::size_t total_count, const Future& future)
        : CombinatorState<ResultType, Executor, Allocator>(total_count, future) {
    }

    template <typename Value>
    void set_value(Value& result, Value&& value) {
        std::lock_guard<std::mutex> lock(this->mutex);
        // 如果已经有异常了，忽略正常的结果
        if (this->first_exception) {
            return;
        }

        result = std::forward<Value>(value);
        this->completed_count++;
        if (this->completed_count == this->total_count) {
            this->promise.set_value(std::move(this->results));
        }
    }

    void set_exception(const mc::exception& e) {
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            this->completed_count++;
            if (this->first_exception) {
                // 如果已经异常了，忽略其他异常
                return;
            }

            // all 操作传播第一个异常，因为这个异常发生后整体就失败了
            this->first_exception = true;
            this->promise.set_exception(e);
        }

        // 异常后取消所有子 future
        this->execute_cancel_callbacks();
    }
};

// any 操作专用的 CombinatorState
template <typename ResultType, typename Executor, typename Allocator>
struct AnyState : public CombinatorState<ResultType, Executor, Allocator> {
    using state_type = AnyState<ResultType, Executor, Allocator>;

    bool              completed      = false;
    std::size_t       failed_count   = 0;       // 记录所有失败的future数量，包括取消和其他异常
    mc::exception_ptr last_exception = nullptr; // 记录最后一个异常

    template <typename Future>
    AnyState(std::size_t total_count, const Future& future)
        : CombinatorState<ResultType, Executor, Allocator>(total_count, future) {
    }

    template <typename Value>
    bool set_value(std::size_t index, Value&& value) {
        std::lock_guard<std::mutex> lock(this->mutex);
        if (this->completed) {
            return false;
        }

        this->completed = true;
        this->promise.set_value(std::make_pair(index, std::forward<Value>(value)));
        return true;
    }

    bool set_exception(const mc::exception& e) {
        std::lock_guard<std::mutex> lock(this->mutex);
        if (this->completed) {
            return false;
        }

        // 记录异常
        auto ex = e.dynamic_copy_exception();
        if (this->last_exception) {
            auto old_messsgs = this->last_exception->take_messages();
            ex->append_log(std::move(old_messsgs));
        }
        this->last_exception = ex;

        // 增加失败计数
        this->failed_count++;
        if (this->failed_count < this->total_count) {
            return false;
        }

        // 全部失败了
        this->completed = true;
        this->promise.set_exception(*this->last_exception);
        return true;
    }
};

template <typename State, typename Future, typename Result>
void process_all(std::shared_ptr<State> state, Result& result, Future& future) {
    future.then([state, &result](Result&& value) {
        state->set_value(result, std::forward<Result>(value));
    }).catch_error([state](const mc::exception& e) {
        state->set_exception(e);
    });

    future.on_cancel([state = std::weak_ptr(state)]() {
        if (auto state_ptr = state.lock()) {
            state_ptr->set_exception(mc::canceled_exception());
        }
    });
}

template <std::size_t I, std::size_t N, typename State, typename Futures>
void process_all(std::shared_ptr<State> state, Futures& futures) {
    if constexpr (I < N) {
        process_all(state, std::get<I>(state->results), std::get<I>(futures));
        process_all<I + 1, N>(state, futures);
    }
}

template <typename State, typename Future>
void process_any(std::shared_ptr<State> state, std::size_t index, Future& future) {
    using result_type = typename Future::result_type;
    future.then([state, index](result_type&& value) {
        if (state->set_value(index, std::forward<result_type>(value))) {
            state->execute_cancel_callbacks();
        }
    }).catch_error([state](const mc::exception& e) {
        if (state->set_exception(e)) {
            state->execute_cancel_callbacks();
        }
    });

    future.on_cancel([state = std::weak_ptr(state)]() {
        if (auto state_ptr = state.lock()) {
            if (state_ptr->set_exception(mc::canceled_exception())) {
                state_ptr->execute_cancel_callbacks();
            }
        }
    });
}

template <std::size_t I, std::size_t N, typename State, typename Futures>
void process_any(std::shared_ptr<State> state, Futures& futures) {
    if constexpr (I < N) {
        process_any(state, I, std::get<I>(futures));
        process_any<I + 1, N>(state, futures);
    }
}

} // namespace detail

// 可变参数版本的 all 函数实现
template <typename... Futures>
auto all(Futures&&... futures)
    -> Future<std::tuple<typename std::decay_t<Futures>::result_type...>,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::executor_type,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::allocator_type> {
    static_assert(sizeof...(Futures) > 0, "all requires at least one future");

    using FirstFuture = std::tuple_element_t<0, std::tuple<Futures...>>;
    using Executor    = typename std::decay_t<FirstFuture>::executor_type;
    using Allocator   = typename std::decay_t<FirstFuture>::allocator_type;
    using ResultType  = std::tuple<typename std::decay_t<Futures>::result_type...>;

    auto  tuple_futures = std::forward_as_tuple(futures...);
    auto& first         = std::get<0>(tuple_futures);
    using State         = detail::AllState<ResultType, Executor, Allocator>;
    auto state          = std::make_shared<State>(sizeof...(Futures), first);
    auto result         = state->promise.get_future();

    state->add_cancel_callbacks(tuple_futures);

    // 当 result 被取消时，取消所有子 future
    result.on_cancel([state = std::weak_ptr(state)]() {
        if (auto state_ptr = state.lock()) {
            state_ptr->execute_cancel_callbacks();
        }
    });

    detail::process_all<0, sizeof...(Futures)>(state, tuple_futures);
    return result;
}

// 容器版本的 all 函数实现
template <typename Iterator>
auto all(Iterator begin, Iterator end)
    -> Future<std::vector<typename std::iterator_traits<Iterator>::value_type::result_type>,
              typename std::iterator_traits<Iterator>::value_type::executor_type,
              typename std::iterator_traits<Iterator>::value_type::allocator_type> {
    using future_type = typename std::iterator_traits<Iterator>::value_type;
    using value_type  = typename future_type::result_type;
    using Executor    = typename future_type::executor_type;
    using Allocator   = typename future_type::allocator_type;
    using ResultType  = std::vector<value_type>;
    using State       = detail::AllState<ResultType, Executor, Allocator>;

    MC_ASSERT(begin != end, "all requires at least one future");
    auto total  = std::distance(begin, end);
    auto state  = std::make_shared<State>(total, *begin);
    auto result = state->promise.get_future();

    std::size_t index = 0;
    state->results.resize(total);
    state->add_cancel_callbacks(begin, end);
    for (auto it = begin; it != end; ++it) {
        detail::process_all(state, state->results[index++], *it);
    }

    result.on_cancel([state = std::weak_ptr(state)]() {
        if (auto state_ptr = state.lock()) {
            state_ptr->execute_cancel_callbacks();
        }
    });

    return result;
}

// any 实现：等待任意一个 future 完成
template <typename... Futures>
auto any(Futures&&... futures)
    -> Future<std::pair<std::size_t,
                        mc::traits::apply_type_t<
                            std::variant,
                            mc::traits::type_set_t<typename std::decay_t<Futures>::result_type...>>>,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::executor_type,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::allocator_type> {
    static_assert(sizeof...(Futures) > 0, "any requires at least one future");

    using FirstFuture = std::tuple_element_t<0, std::tuple<Futures...>>;
    using Executor    = typename std::decay_t<FirstFuture>::executor_type;
    using Allocator   = typename std::decay_t<FirstFuture>::allocator_type;
    using VariantType = mc::traits::apply_type_t<
        std::variant,
        mc::traits::type_set_t<typename std::decay_t<Futures>::result_type...>>;
    using ResultType = std::pair<std::size_t, VariantType>;
    using State      = detail::AnyState<ResultType, Executor, Allocator>;

    auto  tuple_futures = std::forward_as_tuple(futures...);
    auto& first         = std::get<0>(tuple_futures);
    auto  state         = std::make_shared<State>(sizeof...(Futures), first);
    auto  result        = state->promise.get_future();

    state->add_cancel_callbacks(tuple_futures);

    // 当 result 被取消时，取消所有子 future
    result.on_cancel([state = std::weak_ptr(state)]() {
        if (auto state_ptr = state.lock()) {
            state_ptr->execute_cancel_callbacks();
        }
    });

    detail::process_any<0, sizeof...(Futures)>(state, tuple_futures);
    return result;
}

// 容器版本的 any 函数实现
template <typename Iterator>
auto any(Iterator begin, Iterator end)
    -> Future<std::pair<std::size_t, typename std::iterator_traits<Iterator>::value_type::result_type>,
              typename std::iterator_traits<Iterator>::value_type::executor_type,
              typename std::iterator_traits<Iterator>::value_type::allocator_type> {
    using future_type = typename std::iterator_traits<Iterator>::value_type;
    using value_type  = typename future_type::result_type;
    using Executor    = typename future_type::executor_type;
    using Allocator   = typename future_type::allocator_type;
    using ResultType  = std::pair<std::size_t, value_type>;
    using State       = detail::AnyState<ResultType, Executor, Allocator>;

    MC_ASSERT(begin != end, "Empty future sequence");
    auto total  = std::distance(begin, end);
    auto state  = std::make_shared<State>(total, *begin);
    auto result = state->promise.get_future();

    std::size_t index = 0;
    state->add_cancel_callbacks(begin, end);
    for (auto it = begin; it != end; ++it) {
        detail::process_any(state, index++, *it);
    }

    result.on_cancel([state = std::weak_ptr(state)]() {
        if (auto state_ptr = state.lock()) {
            state_ptr->execute_cancel_callbacks();
        }
    });

    return result;
}
} // namespace mc::futures

#endif // MC_FUTURES_DETAIL_COMBINATOR_H