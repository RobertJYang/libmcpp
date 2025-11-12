/**
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MC_ENGINE_RESULT_H
#define MC_ENGINE_RESULT_H

#include <mc/error.h>
#include <mc/exception.h>
#include <mc/future.h>
#include <mc/reflect/base.h>
#include <mc/reflect/signature_helper.h>
#include <mc/runtime/any_executor.h>
#include <mc/runtime/executor.h>
#include <mc/time.h>
#include <mc/traits.h>
#include <mc/variant.h>

namespace mc {
using io_context     = boost::asio::io_context;
using system_context = boost::asio::system_context;
using thread_pool    = boost::asio::thread_pool;
template <typename Executor>
using strand_t = boost::asio::strand<Executor>;

namespace detail {
template <typename T, typename F, typename Arg>
static auto call_impl(F&& func, Arg v) {
    if constexpr (std::is_invocable_v<F>) {
        MC_UNUSED(v);
        return func();
    } else if constexpr (mc::is_variant_v<T>) {
        // 针对 lamda: [](auto &&) {} 这种场景可以直接调用，没有必要做参数转换。
        // 另外，lamda 中 auto 参数的场景 mc::traits::function_traits 无法推导参数类型，
        // 这里也必须通过 std::is_invocable_v 先行跳过。
        if constexpr (std::is_invocable_v<F, Arg>) {
            return func(v);
        } else {
            using function_traits = mc::traits::function_traits<F>;
            using args_type       = typename function_traits::args_type;
            using arg_type        = mc::traits::remove_cvref_t<std::tuple_element_t<0, args_type>>;
            return func(mc::detail::convert_arg<arg_type>("result", v));
        }
    } else {
        return func(v);
    }
}

[[noreturn]] MC_API void throw_method_call_exception(const mc::error_ptr& err);
MC_API mc::error_ptr get_default_error();
MC_API mc::method_call_exception make_method_call_exception(const mc::error_ptr& err);

template <typename T, typename = void>
struct future_value_type {
    using type = T;
};

template <typename T>
struct future_value_type<T, std::enable_if_t<mc::futures::detail::is_future_v<T>>> {
    using type = typename T::value_type;
};

template <typename T, typename F, typename P, typename = void>
struct result_traits : std::false_type {
};

template <typename T, typename F, typename P>
struct result_traits<
    T, F, P,
    std::void_t<decltype(call_impl<T>(std::declval<F>(), std::declval<P>()))>> : std::true_type {
    using call_result_type = decltype(call_impl<T>(std::declval<F>(), std::declval<P>()));

    static constexpr bool is_future = mc::futures::detail::is_future_v<call_result_type>;
    using return_type               = typename future_value_type<call_result_type>::type;
    using future_type               = mc::future<return_type, mc::any_executor>;
};

} // namespace detail

template <typename T>
using result_variant = std::variant<
    // 同步返回值类型
    std::conditional_t<std::is_same_v<T, void>, std::monostate, T>,
    // 异步返回值类型，暂时值支持这几种 Executor 类型，有需要再扩展
    mc::future<T, io_context::executor_type>,
    mc::future<T, system_context::executor_type>,
    mc::future<T, thread_pool::executor_type>,
    mc::future<T, strand_t<io_context::executor_type>>,
    mc::future<T, strand_t<system_context::executor_type>>,
    mc::future<T, strand_t<thread_pool::executor_type>>,
    mc::future<T, mc::immediate_executor>,
    mc::future<T, mc::executor>,
    mc::future<T, mc::any_executor>,
    mc::future<T, boost::asio::any_io_executor>,
    // 返回错误
    mc::error_ptr>;

template <typename T = mc::variant>
class result {
    static_assert(!mc::futures::detail::is_future_v<T> &&
                      (mc::is_variant_constructible_v<T> ||
                       mc::is_variant_v<T> ||
                       std::is_same_v<T, void>),
                  "T must be constructible to variant or void");
    using property_traits = mc::traits::property_traits<T>;

    using value_type  = typename property_traits::value_type;
    using param_type  = typename property_traits::param_type;
    using rvalue_type = typename property_traits::rvalue_type;

public:
    result() = default;

    // 构造通过 value_type 返回的结果（优化：基础类型传值，其他类型传递常量引用）
    result(param_type value) : m_value(value) {
    }

    // 构造通过 value_type 右值引用返回的结果（优化：基础类型屏蔽右值引用，其他类型允许右值引用）
    result(rvalue_type value) : m_value(std::move(value)) {
    }

    // 构造通过 mc::error_ptr 类型返回的结果
    result(mc::error_ptr err) : m_value(err) {
        if (!err || !err->is_set()) {
            // 如果给定的 err 为空或没有设置错误内容，则返回错误引擎中最后一个错误或者默认的 failed 错误
            m_value = detail::get_default_error();
        }
    }

    // 构造通过 mc::future<value_type> 类型返回的结果
    template <typename FutureType,
              std::enable_if_t<
                  mc::futures::detail::is_future_v<std::decay_t<FutureType>> &&
                      !std::is_same_v<typename std::decay_t<FutureType>::value_type, mc::error_ptr>,
                  int> = 0>
    result(FutureType&& v)
        : m_value(std::forward<FutureType>(v).template as_future<value_type>()) {
    }

    // 构造通过 mc::future<mc::error_ptr> 类型返回的结果
    template <typename FutureType,
              std::enable_if_t<
                  mc::futures::detail::is_future_v<std::decay_t<FutureType>> &&
                      std::is_same_v<typename std::decay_t<FutureType>::value_type, mc::error_ptr>,
                  int> = 0>
    result(FutureType&& v)
        : m_value(std::forward<FutureType>(v).then([](auto&& v) -> value_type {
              detail::throw_method_call_exception(v);
          })) {
    }

    // 从其他 result<OtherT> 类型构造
    template <typename OtherT, std::enable_if_t<!std::is_same_v<value_type, OtherT>, int> = 0>
    result(result<OtherT>&& other) {
        std::visit([this](auto&& v) -> void {
            using arg_type = std::decay_t<decltype(v)>;
            if constexpr (mc::futures::detail::is_future_v<arg_type>) {
                using executor_type  = typename arg_type::executor_type;
                using allocator_type = typename arg_type::allocator_type;
                m_value.template emplace<mc::future<value_type, executor_type, allocator_type>>(
                    std::move(v).template as_future<value_type>());
            } else if constexpr (std::is_same_v<arg_type, mc::error_ptr>) {
                m_value.template emplace<mc::error_ptr>(std::move(v));
            } else {
                m_value.template emplace<value_type>(std::move(v));
            }
        }, std::forward<result<OtherT>>(other).m_value);
    }

    result(const mc::exception& ex)
        : result(mc::error::from_exception(ex)) {
    }

    // 处理类型到 result<mc::variant> 的转换
    template <typename U>
    explicit result(U value, std::enable_if_t<
                                 std::is_same_v<T, mc::variant> &&
                                     mc::is_variant_constructible_v<U> &&
                                     !std::is_same_v<std::decay_t<U>, mc::exception> &&
                                     !std::is_same_v<std::decay_t<U>, mc::error_ptr> &&
                                     !mc::futures::detail::is_future_v<std::decay_t<U>>,
                                 int> = 0)
        : m_value(mc::variant(value)) {
    }

    // 不允许拷贝
    result(const result& other)            = delete;
    result& operator=(const result& other) = delete;

    // 允许移动
    result(result&& other)            = default;
    result& operator=(result&& other) = default;

    // 获取结果值
    // 如果结果为 future，则阻塞等待直到 future 就绪，然后返回结果值
    // 如果结果为 mc::error，则抛出 mc::method_call_exception 异常
    // 如果 future 异常，则抛出 future 异常
    value_type get() const {
        return std::visit([](auto&& v) -> value_type {
            using arg_type = std::decay_t<decltype(v)>;
            if constexpr (mc::futures::detail::is_future_v<arg_type>) {
                return v.get();
            } else if constexpr (std::is_same_v<arg_type, mc::error_ptr>) {
                detail::throw_method_call_exception(v);
            } else {
                return v;
            }
        }, m_value);
    }

    // 获取结果值，最长等待 timeout 时间
    template <typename Duration>
    value_type get_for(Duration timeout) const {
        return std::visit([timeout](auto&& v) -> value_type {
            using arg_type = std::decay_t<decltype(v)>;
            if constexpr (mc::futures::detail::is_future_v<arg_type>) {
                return v.get_for(timeout);
            } else if constexpr (std::is_same_v<arg_type, mc::error_ptr>) {
                detail::throw_method_call_exception(v);
            } else {
                return v;
            }
        }, m_value);
    }

    // 等待结果就绪
    void wait() const {
        std::visit([](auto&& v) {
            using arg_type = std::decay_t<decltype(v)>;
            if constexpr (mc::futures::detail::is_future_v<arg_type>) {
                v.wait();
            }
        }, m_value);
    }

    // 等待结果就绪，最长等待 timeout 时间
    template <typename Duration>
    void wait_for(Duration timeout) const {
        std::visit([timeout](auto&& v) {
            using arg_type = std::decay_t<decltype(v)>;
            if constexpr (mc::futures::detail::is_future_v<arg_type>) {
                v.wait_for(timeout);
            }
        }, m_value);
    }

    // 等待结果就绪，直到 timeout_time 时间点
    template <typename TimePoint>
    void wait_until(TimePoint timeout_time) const {
        std::visit([timeout_time](auto&& v) {
            using arg_type = std::decay_t<decltype(v)>;
            if constexpr (mc::futures::detail::is_future_v<arg_type>) {
                v.wait_until(timeout_time);
            }
        }, m_value);
    }

    // 判断结果是否为 value_type 类型
    bool is_value() const {
        return std::holds_alternative<value_type>(m_value);
    }

    // 获取结果值
    const value_type& get_value() const {
        return std::get<value_type>(m_value);
    }

    // 获取结果值
    value_type& get_value() {
        return std::get<value_type>(m_value);
    }

    /*
     * 判断结果是否为 future
     * 对于 future 类型，返回 true
     * 对于其他类型，返回 false
     */
    bool is_future() const {
        return std::visit([](auto&& v) {
            using arg_type = std::decay_t<decltype(v)>;
            return mc::futures::detail::is_future_v<arg_type>;
        }, m_value);
    }

    /*
     * 判断结果是否已完成
     * 对于 future 类型，未就绪返回 false，否则返回 true
     * 对于其他类型，直接返回 true
     */
    bool is_completed() const {
        return std::visit([](auto&& v) {
            using arg_type = std::decay_t<decltype(v)>;
            if constexpr (mc::futures::detail::is_future_v<arg_type>) {
                return v.is_ready();
            } else {
                return true;
            }
        }, m_value);
    }

    /*
     * 判断结果是否为错误
     * 对于 future 类型，如果未就绪，则等待就绪，然后判断是否被拒绝
     * 对于 mc::error_ptr 类型，直接返回 true
     * 对于其他类型，直接返回 false
     */
    bool is_error() const {
        return std::visit([](auto&& v) {
            using arg_type = std::decay_t<decltype(v)>;
            if constexpr (mc::futures::detail::is_future_v<arg_type>) {
                if (!v.is_ready()) {
                    v.wait();
                }
                return v.is_rejected();
            } else if constexpr (std::is_same_v<arg_type, mc::error_ptr>) {
                return v && v->is_set();
            } else {
                return false;
            }
        }, m_value);
    }

    /*
     * 获取错误信息
     * 对于 future 类型，如果未就绪，则等待就绪，然后判断是否被拒绝，如果被拒绝，则返回错误信息
     * 对于 mc::error_ptr 类型，直接返回
     * 对于非错误类型，返回 nullptr
     */
    mc::error_ptr get_error() const {
        return std::visit([](auto&& v) -> mc::error_ptr {
            using arg_type = std::decay_t<decltype(v)>;
            if constexpr (mc::futures::detail::is_future_v<arg_type>) {
                if (!v.is_ready()) {
                    v.wait();
                }
                if (v.is_rejected()) {
                    return mc::error::from_exception(v.get_exception());
                }
                return {};
            } else if constexpr (std::is_same_v<arg_type, mc::error_ptr>) {
                return v;
            } else {
                return {};
            }
        }, m_value);
    }

    /*
     * 用于在调用就绪后执行回调函数，返回 mc::future 类型用于链式调用
     * func 支持 0 个或 1 个参数
     * 0 个参数时会丢弃 result<T> 的值，只是执行回调函数
     * 1 个参数时会传递 result<T> 的值给回调函数
     * 针对 result<mc::variant>，允许参数类型为 mc::variant 可转换的类型，内部会自动转换
     * 如：result<mc::variant>(1).then([](int v) {
     *      return v + 1;
     *  })
     */
    template <typename F>
    auto then(F&& func)
        -> std::enable_if_t<
            detail::result_traits<T, F, param_type>::value,
            typename detail::result_traits<T, F, param_type>::future_type> {
        using result_traits = detail::result_traits<T, F, param_type>;
        using result_type   = typename result_traits::return_type;
        using future_type   = typename result_traits::future_type;

        return std::visit([f = std::forward<F>(func)](auto&& v) -> future_type {
            using arg_type = std::decay_t<decltype(v)>;
            if constexpr (mc::futures::detail::is_future_v<arg_type>) {
                return then_impl<result_type, result_traits::is_future>(
                    std::move(f), std::forward<decltype(v)>(v));
            } else if constexpr (std::is_same_v<arg_type, mc::error_ptr>) {
                return mc::reject<result_type>(detail::make_method_call_exception(v));
            } else if constexpr (std::is_same_v<arg_type, value_type>) {
                return then_impl<result_type, result_traits::is_future>(
                    std::move(f), mc::resolve(std::forward<decltype(v)>(v)));
            }
        }, m_value);
    }

    template <typename F>
    auto catch_error(F&& func)
        -> std::enable_if_t<
            detail::result_traits<T, F, const mc::exception&>::value,
            typename detail::result_traits<T, F, const mc::exception&>::future_type> {
        using result_traits = detail::result_traits<T, F, const mc::exception&>;
        using result_type   = typename result_traits::return_type;
        using future_type   = typename result_traits::future_type;

        return std::visit([f = std::forward<F>(func)](auto&& v) -> future_type {
            using arg_type = std::decay_t<decltype(v)>;
            if constexpr (mc::futures::detail::is_future_v<arg_type>) {
                return catch_error_impl<result_type, result_traits::is_future>(
                    std::move(f), std::forward<decltype(v)>(v));
            } else if constexpr (std::is_same_v<arg_type, mc::error_ptr>) {
                return catch_error_impl<result_type, result_traits::is_future>(
                    std::move(f), mc::reject<result_type>(detail::make_method_call_exception(v)));
            } else if constexpr (std::is_same_v<arg_type, value_type>) {
                return catch_error_impl<result_type, result_traits::is_future>(
                    std::move(f), mc::resolve(std::forward<decltype(v)>(v)));
            }
        }, m_value);
    }

    void cancel() {
        std::visit([](auto&& v) {
            using arg_type = std::decay_t<decltype(v)>;
            if constexpr (mc::futures::detail::is_future_v<arg_type>) {
                v.cancel();
            }
        }, m_value);
    }

    auto as_future() {
        if constexpr (std::is_void_v<value_type>) {
            return then([]() {
            });
        } else {
            return then([](auto&& v) -> param_type {
                return v;
            });
        }
    }

    const result_variant<value_type>& get_variant() const {
        return m_value;
    }

    result_variant<value_type>& get_variant() {
        return m_value;
    }

private:
    template <typename ResultType, bool IsFuture, typename F, typename FutureType>
    static auto then_impl(F&& func, FutureType&& v) {
        auto promise = mc::make_promise<ResultType>(mc::any_executor());
        auto future  = promise.get_future();

        v.then([f = std::forward<F>(func), promise](auto&& val) mutable {
            if constexpr (std::is_void_v<ResultType>) {
                if constexpr (IsFuture) {
                    detail::call_impl<T, F, param_type>(
                        std::forward<F>(f), std::forward<decltype(val)>(val))
                        .then([promise](auto&&) mutable {
                        promise.set_value();
                    }).catch_error([promise](auto&&) mutable {
                        promise.set_exception(std::current_exception());
                    }).on_cancel(promise);
                } else {
                    detail::call_impl<T, F, param_type>(
                        std::forward<F>(f), std::forward<decltype(val)>(val));
                    promise.set_value();
                }
            } else {
                if constexpr (IsFuture) {
                    detail::call_impl<T, F, param_type>(
                        std::forward<F>(f), std::forward<decltype(val)>(val))
                        .then([promise](auto&& next_val) mutable {
                        promise.set_value(std::forward<decltype(next_val)>(next_val));
                    }).catch_error([promise](auto&&) mutable {
                        promise.set_exception(std::current_exception());
                    }).on_cancel(promise);
                } else {
                    promise.set_value(
                        detail::call_impl<T, F, param_type>(
                            std::forward<F>(f), std::forward<decltype(val)>(val)));
                }
            }
        }).catch_error([promise](auto&&) mutable {
            promise.set_exception(std::current_exception());
        }).on_cancel(future);

        return future;
    }

    template <typename ResultType, bool IsFuture, typename F, typename FutureType>
    static auto catch_error_impl(F&& func, FutureType&& v) {
        auto promise = mc::make_promise<ResultType>(mc::any_executor());
        auto future  = promise.get_future();

        v.catch_error([f = std::forward<F>(func), promise](auto&& vv) mutable {
            if constexpr (std::is_void_v<ResultType>) {
                if constexpr (IsFuture) {
                    detail::call_impl<T, F, const mc::exception&>(
                        std::forward<F>(f), std::forward<decltype(vv)>(vv))
                        .then([promise](auto&&) mutable {
                        promise.set_value();
                    }).catch_error([promise](auto&&) mutable {
                        promise.set_exception(std::current_exception());
                    }).on_cancel(promise);
                } else {
                    detail::call_impl<T, F, const mc::exception&>(
                        std::forward<F>(f), std::forward<decltype(vv)>(vv));
                    promise.set_value();
                }
            } else {
                if constexpr (IsFuture) {
                    detail::call_impl<T, F, const mc::exception&>(
                        std::forward<F>(f), std::forward<decltype(vv)>(vv))
                        .then([promise](auto&& next_val) mutable {
                        promise.set_value(std::forward<decltype(next_val)>(next_val));
                    }).catch_error([promise](auto&&) mutable {
                        promise.set_exception(std::current_exception());
                    }).on_cancel(promise);
                } else {
                    promise.set_value(
                        detail::call_impl<T, F, const mc::exception&>(
                            std::forward<F>(f), std::forward<decltype(vv)>(vv)));
                }
            }
        }).on_cancel(future);

        return future;
    }

    template <typename OtherT>
    friend class result;

    result_variant<value_type> m_value;
};

using async_result = result<mc::variant>;

// 一些辅助函数，用于简化 result 的创建
template <typename T>
auto make_result(T&& value)
    -> std::enable_if_t<!mc::futures::detail::is_future_v<T> &&
                            mc::is_variant_constructible_v<T>,
                        result<T>> {
    return result<T>(std::forward<T>(value));
}

template <typename T, typename Executor, typename Allocator>
auto make_result(mc::future<T, Executor, Allocator>&& value) {
    using value_type = typename mc::future<T, Executor, Allocator>::value_type;
    return result<value_type>(std::forward<mc::future<T, Executor, Allocator>>(value));
}

inline auto make_result() {
    return result<void>();
}

template <typename T>
auto make_result(const mc::exception& ex) {
    return result<T>(ex);
}

template <typename T>
auto make_result(mc::error_ptr&& err) {
    return result<T>(std::forward<mc::error_ptr>(err));
}

// mc::reflect 反射系统要求函数返回值必须可转换成 mc::variant
// 所以这里对 mc::future 和 mc::result 类型特化 to_variant 和 from_variant 函数

template <typename FutureType,
          std::enable_if_t<mc::futures::detail::is_future_v<FutureType>, int> = 0>
inline void to_variant(const FutureType& obj, mc::variant& v) {
    v = mc::variant(obj.get()); // 阻塞等待直到 future 就绪
}

template <typename FutureType,
          std::enable_if_t<mc::futures::detail::is_future_v<FutureType>, int> = 0>
inline void from_variant(const mc::variant& v, FutureType& obj) {
    obj = mc::resolve(v);
}

template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
inline void to_variant(const result<T>& obj, mc::variant& v) {
    v = mc::variant(obj.get()); // 阻塞等待直到 future 就绪
}

template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
inline void from_variant(const mc::variant& v, result<T>& obj) {
    obj = std::move(result<T>(v));
}

inline void to_variant(const result<mc::variant>& obj, mc::variant& v) {
    v = obj.get(); // 阻塞等待直到 future 就绪
}

inline void from_variant(const mc::variant& v, result<mc::variant>& obj) {
    obj = result<mc::variant>(v);
}

inline void to_variant(const result<void>& obj, mc::variant& v) {
    if (obj.is_future()) {
        obj.wait(); // 阻塞等待直到 future 就绪
    }

    v = mc::variant();
}

inline void from_variant(const mc::variant& v, result<void>& obj) {
    obj = result<void>();
}

namespace reflect::detail {
// 特化 signature_helper，用于获取 future 和 result 的返回值类型

template <typename FutureType>
struct signature_helper<FutureType,
                        std::enable_if_t<mc::futures::detail::is_future_v<FutureType>>> {
    static void apply(std::string& sig) {
        sig = mc::reflect::detail::get_signature<typename FutureType::value_type>();
    }
};

template <typename T>
struct signature_helper<result<T>, void> {
    static void apply(std::string& sig) {
        sig = mc::reflect::detail::get_signature<T>();
    }
};

} // namespace reflect::detail
} // namespace mc

#endif