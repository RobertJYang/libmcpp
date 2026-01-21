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

namespace detail {
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
    std::void_t<decltype(mc::futures::detail::invoke_func<T>(std::declval<F>(), std::declval<P>()))>> : std::true_type {
    using call_result_type = decltype(mc::futures::detail::invoke_func<T>(std::declval<F>(), std::declval<P>()));

    static constexpr bool is_future = mc::futures::detail::is_future_v<call_result_type>;
    using return_type               = typename future_value_type<call_result_type>::type;
    using future_type               = mc::future<return_type>;
};

} // namespace detail

template <typename T = mc::variant>
class result {
    static_assert(!mc::futures::detail::is_future_v<T>,
                  "T must be constructible to variant or void");
    using property_traits = mc::traits::property_traits<T>;

    using value_type  = mc::futures::detail::state_tt<typename property_traits::value_type>;
    using param_type  = typename property_traits::param_type;
    using rvalue_type = typename property_traits::rvalue_type;

public:
    template <typename U                                                                     = value_type,
              std::enable_if_t<std::is_void_v<U> || std::is_default_constructible_v<U>, int> = 0>
    result() {
        if constexpr (std::is_void_v<value_type>) {
            m_future = mc::resolve();
        } else {
            m_future = mc::resolve<value_type>(value_type{});
        }
    }

    result(param_type value) {
        if constexpr (std::is_void_v<value_type>) {
            (MC_UNUSED(value));
            m_future = mc::resolve();
        } else {
            // 处理 const 引用：如果是 const 引用，需要复制；否则可以移动
            if constexpr (std::is_reference_v<param_type> && std::is_const_v<std::remove_reference_t<param_type>>) {
                m_future = mc::resolve<value_type>(value_type(value));
            } else {
                m_future = mc::resolve<value_type>(std::forward<param_type>(value));
            }
        }
    }

    result(rvalue_type value) {
        if constexpr (std::is_void_v<value_type>) {
            m_future = mc::resolve();
        } else {
            m_future = mc::resolve<value_type>(std::move(value));
        }
    }

    result(mc::error_ptr err)
        : m_future(mc::reject<value_type>(detail::make_method_call_exception(
              err && err->is_set() ? err : detail::get_default_error()))) {
    }

    // 构造通过 mc::future<value_type> 类型返回的结果
    template <typename FutureType,
              std::enable_if_t<
                  mc::futures::detail::is_future_v<std::decay_t<FutureType>> &&
                      !std::is_same_v<typename std::decay_t<FutureType>::value_type, mc::error_ptr>,
                  int> = 0>
    result(FutureType&& v)
        : m_future(std::forward<FutureType>(v).template as_future<value_type>()) {
    }

    // 构造通过 mc::future<mc::error_ptr> 类型返回的结果
    template <typename FutureType,
              std::enable_if_t<
                  mc::futures::detail::is_future_v<std::decay_t<FutureType>> &&
                      std::is_same_v<typename std::decay_t<FutureType>::value_type, mc::error_ptr>,
                  int> = 0>
    result(FutureType&& v)
        : m_future(std::forward<FutureType>(v)
                       .then([](auto&& v) -> value_type {
                           detail::throw_method_call_exception(v);
                       })
                       .template as_future<value_type>()) {
    }

    // 从其他 result<OtherT> 类型构造
    template <typename OtherT, std::enable_if_t<!std::is_same_v<value_type, OtherT>, int> = 0>
    result(result<OtherT>&& other) : m_future(std::move(other.m_future).template as_future<value_type>()) {
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
        : m_future(mc::resolve<value_type>(mc::variant(value))) {
    }

    // 不允许拷贝
    result(const result& other)            = delete;
    result& operator=(const result& other) = delete;

    // 允许移动
    result(result&& other)            = default;
    result& operator=(result&& other) = default;

    // 获取结果值
    // 阻塞等待直到 future 就绪，然后返回结果值
    // 如果 future 异常，则抛出异常
    value_type get() const {
        return m_future.get();
    }

    // 获取结果值，最长等待 timeout 时间
    template <typename Duration>
    value_type get_for(Duration timeout) const {
        return m_future.get_for(timeout);
    }

    // 等待结果就绪
    void wait() const {
        m_future.wait();
    }

    // 等待结果就绪，最长等待 timeout 时间
    template <typename Duration>
    void wait_for(Duration timeout) const {
        m_future.wait_for(timeout);
    }

    // 等待结果就绪，直到 timeout_time 时间点
    template <typename TimePoint>
    void wait_until(TimePoint timeout_time) const {
        m_future.wait_until(timeout_time);
    }

    // 判断结果是否为 value_type 类型（已就绪且不是错误）
    bool is_value() const {
        return m_future.is_ready() && !m_future.is_rejected();
    }

    // 获取结果值
    value_type get_value() const {
        if (!m_future.is_ready()) {
            m_future.wait();
        }
        if (m_future.is_rejected()) {
            MC_THROW(mc::method_call_exception, "result 包含错误，无法获取值");
        }
        return m_future.get();
    }

    /*
     * 判断结果是否为 future
     */
    bool is_future() const {
        return true;
    }

    /*
     * 判断结果是否已完成
     */
    bool is_ready() const {
        return m_future.is_ready();
    }

    /*
     * 判断结果是否为错误
     */
    bool is_error() const {
        if (!m_future.is_ready()) {
            m_future.wait();
        }
        return m_future.is_rejected();
    }

    /*
     * 获取错误信息
     * 如果 future 被拒绝，则返回错误信息
     * 否则返回 nullptr
     * 空状态下抛出异常
     */
    mc::error_ptr get_error() const {
        if (!m_future.valid()) {
            MC_THROW(mc::futures::invalid_future_exception, "result 包含无效的 future");
        }
        if (!m_future.is_ready()) {
            m_future.wait();
        }
        if (m_future.is_rejected()) {
            return mc::error::from_exception(m_future.get_exception());
        }
        return {};
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
    auto then(F&& func, launch policy = launch::async, std::optional<mc::any_executor> executor = std::nullopt)
        -> std::enable_if_t<
            detail::result_traits<T, F, param_type>::value,
            typename detail::result_traits<T, F, param_type>::future_type> {
        return m_future.then(std::forward<F>(func), policy, executor);
    }

    template <typename F>
    auto catch_error(F&& func, launch policy = launch::async, std::optional<mc::any_executor> executor = std::nullopt)
        -> std::enable_if_t<
            detail::result_traits<T, F, const mc::exception&>::value,
            typename detail::result_traits<T, F, const mc::exception&>::future_type> {
        return m_future.catch_error(std::forward<F>(func), policy, executor);
    }

    void cancel() {
        m_future.cancel();
    }

    template <typename F>
    auto finally(F&& func, launch policy = launch::async, std::optional<mc::any_executor> executor = std::nullopt) {
        return m_future.finally(std::forward<F>(func), policy, executor);
    }

    template <typename F>
    auto tap(F&& func, launch policy = launch::async) {
        return m_future.tap(std::forward<F>(func), policy);
    }

    template <typename F>
    auto tap_error(F&& func, launch policy = launch::async) {
        return m_future.tap_error(std::forward<F>(func), policy);
    }

    auto as_future() {
        return m_future;
    }

    bool valid() const {
        return m_future.valid();
    }

    // 获取内部的 future
    const mc::future<value_type>& get_future() const {
        return m_future;
    }

    mc::future<value_type>& get_future() {
        return m_future;
    }

private:
    template <typename OtherT>
    friend class result;

    mc::future<value_type> m_future;
};

using async_result = result<mc::variant>;

// 一些辅助函数，用于简化 result 的创建
template <typename T>
auto make_result(T&& value)
    -> std::enable_if_t<
        !mc::futures::detail::is_future_v<T>,
        result<mc::futures::detail::state_tt<T>>> {
    return result<mc::futures::detail::state_tt<T>>(std::forward<T>(value));
}

template <typename T>
auto make_result(mc::future<T>&& value) {
    using value_type = typename mc::future<T>::value_type;
    return result<mc::futures::detail::state_tt<value_type>>(std::forward<mc::future<T>>(value));
}

inline auto make_result() {
    return result<void>(mc::resolve());
}

template <typename T>
auto make_result(const mc::exception& ex) {
    return result<mc::futures::detail::state_tt<T>>(ex);
}

template <typename T>
auto make_result(mc::error_ptr&& err) {
    return result<mc::futures::detail::state_tt<T>>(std::forward<mc::error_ptr>(err));
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
    obj.wait(); // 阻塞等待直到 future 就绪
    v = mc::variant();
}

inline void from_variant(const mc::variant& v, result<void>& obj) {
    MC_UNUSED(v);
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
