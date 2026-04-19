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
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include <mc/exception.h>
#include <mc/futures/callback_list.h>
#include <mc/futures/exceptions.h>
#include <mc/futures/status.h>
#include <mc/memory.h>
#include <mc/runtime/any_executor.h>
#include <mc/runtime/condition_variable.h>
#include <mc/sync/small_mutex.h>

namespace mc::futures {

class any_promise;
class state_base;
class state_pool;

namespace detail {
template <typename T>
using state_tt = std::conditional_t<std::is_same_v<mc::traits::remove_cvref_t<T>, std::monostate>, void,
                                    mc::traits::remove_cvref_t<T>>;

struct storage_ops {
    void (*destroy_slot)(state_base&) noexcept;
    void (*construct_empty_slot)(state_base&);
};

enum class storage_kind : uint8_t {
    empty,
    inline_storage,
    external_storage,
};

enum class payload_layout : uint8_t {
    inline_object,
    external_pointer,
};

template <typename ResultType>
struct state_payload_binding;

template <typename T>
struct state_payload_access;

MC_API void recover_state_slot_to_pool(void* ptr);
} // namespace detail

enum class completion_state : uint8_t {
    pending,
    resolved,
    rejected,
    cancelled,
};

class MC_API state_base : public shared_base {
public:
    using executor_type           = mc::runtime::any_executor;
    using cached_state_destroy_fn = void (*)(state_base*) noexcept;

    static constexpr std::size_t s_inline_payload_bytes = 32;

    state_base(executor_type executor) noexcept;
    ~state_base();

    void set_executor(executor_type executor) noexcept;
    void reset();
    void mark_ready();
    void cancel();
    void set_policy(launch policy) noexcept;

    // 设置取消回调
    // 注意：回调函数会在取消时立即执行，不会投递到 executor 执行，不要在回调函数中作阻塞动作
    void add_cancel_callback(callback_type callback);

    inline bool is_ready() const noexcept
    {
        return get_completion_state() != completion_state::pending;
    }

    inline bool is_cancel_requested() const noexcept
    {
        return m_cancel_requested.load(std::memory_order_acquire);
    }

    inline bool is_cancelled() const noexcept
    {
        return get_completion_state() == completion_state::cancelled;
    }

    inline bool is_rejected() const noexcept
    {
        return get_completion_state() == completion_state::rejected;
    }

    inline bool is_bound() const noexcept
    {
        return has_flag(s_bound_flag, std::memory_order_acquire);
    }

    std::shared_ptr<mc::exception> get_exception_object() const noexcept;
    void                           rethrow_exception() const;
    void                           copy_exception_to(any_promise& promise, bool strict_once = true) const;

    std::exception_ptr get_exception() const noexcept;
    void               set_exception(std::exception_ptr exception) noexcept;

    void set_ready();

    void destory();

    inline executor_type get_executor() const noexcept
    {
        return m_executor;
    }

    inline launch get_policy() const noexcept
    {
        return decode_policy_bits(m_state_flags.load(std::memory_order_acquire));
    }

    void destroy_cached_state_object() noexcept
    {
        if (m_cached_state_destroy == nullptr) {
            return;
        }
        m_cached_state_destroy(this);
    }

    void reuse_storage(const detail::storage_ops* storage_ops)
    {
        m_storage_ops = storage_ops;
        MC_ASSERT(m_storage_ops != nullptr, "storage ops 必须已绑定");
        m_storage_ops->construct_empty_slot(*this);
    }

protected:
    using condition_variable = mc::runtime::condition_variable;

    friend class any_future;
    friend class any_promise;
    friend class state_pool;
    template <typename ResultType>
    friend struct detail::state_payload_binding;
    template <typename T>
    friend struct detail::state_payload_access;
    friend void detail::recover_state_slot_to_pool(void* ptr);

    static constexpr uint32_t s_completion_state_shift   = 0;
    static constexpr uint32_t s_completion_state_mask    = 0x3u << s_completion_state_shift;
    static constexpr uint32_t s_bound_flag               = 1u << 2;
    static constexpr uint32_t s_policy_shift             = 3;
    static constexpr uint32_t s_policy_mask              = 0x3u << s_policy_shift;
    static constexpr uint32_t s_storage_kind_shift       = 5;
    static constexpr uint32_t s_storage_kind_mask        = 0x3u << s_storage_kind_shift;
    static constexpr uint32_t s_payload_constructed_flag = 1u << 7;

    static constexpr uint32_t encode_completion_state_bits(completion_state state) noexcept
    {
        return (static_cast<uint32_t>(state) & 0x3u) << s_completion_state_shift;
    }

    static constexpr completion_state decode_completion_state_bits(uint32_t flags) noexcept
    {
        return static_cast<completion_state>((flags & s_completion_state_mask) >> s_completion_state_shift);
    }

    static constexpr uint32_t encode_policy_bits(launch policy) noexcept
    {
        return (static_cast<uint32_t>(policy) & 0x3u) << s_policy_shift;
    }

    static constexpr launch decode_policy_bits(uint32_t flags) noexcept
    {
        return static_cast<launch>((flags & s_policy_mask) >> s_policy_shift);
    }

    static constexpr uint32_t encode_storage_kind_bits(detail::storage_kind kind) noexcept
    {
        return (static_cast<uint32_t>(kind) & 0x3u) << s_storage_kind_shift;
    }

    static constexpr detail::storage_kind decode_storage_kind_bits(uint32_t flags) noexcept
    {
        return static_cast<detail::storage_kind>((flags & s_storage_kind_mask) >> s_storage_kind_shift);
    }

    template <typename ResultType>
    static constexpr bool payload_uses_inline_storage() noexcept
    {
        return sizeof(ResultType) <= s_inline_payload_bytes && alignof(ResultType) <= alignof(std::max_align_t);
    }

    bool has_flag(uint32_t flag, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return (m_state_flags.load(order) & flag) != 0;
    }

    void set_flag(uint32_t flag, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        MC_UNUSED(order);
        m_state_flags.fetch_or(flag, std::memory_order_acq_rel);
    }

    void clear_flag(uint32_t flag, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        MC_UNUSED(order);
        m_state_flags.fetch_and(~flag, std::memory_order_acq_rel);
    }

    completion_state get_completion_state() const noexcept
    {
        return decode_completion_state_bits(m_state_flags.load(std::memory_order_acquire));
    }

    void set_completion_state(completion_state state) noexcept;

    detail::storage_kind get_storage_kind() const noexcept
    {
        return decode_storage_kind_bits(m_state_flags.load(std::memory_order_acquire));
    }

    void set_storage_kind(detail::storage_kind kind) noexcept;

    bool is_payload_constructed() const noexcept
    {
        return has_flag(s_payload_constructed_flag, std::memory_order_acquire);
    }

    void set_payload_constructed(bool constructed) noexcept
    {
        if (constructed) {
            set_flag(s_payload_constructed_flag, std::memory_order_release);
            return;
        }
        clear_flag(s_payload_constructed_flag, std::memory_order_release);
    }

    void destroy_slot_locked() noexcept;

    template <typename ResultType>
    ResultType* construct_inline_slot()
    {
        auto* ptr = ::new (static_cast<void*>(m_slot)) ResultType();
        set_storage_kind(detail::storage_kind::inline_storage);
        set_payload_constructed(true);
        return std::launder(ptr);
    }

    template <typename ResultType>
    ResultType* construct_external_slot()
    {
        auto* ptr = ::new (static_cast<void*>(m_slot)) ResultType();
        set_storage_kind(detail::storage_kind::external_storage);
        set_payload_constructed(true);
        return std::launder(ptr);
    }

    template <typename ResultType>
    ResultType* slot_ptr() noexcept
    {
        return std::launder(reinterpret_cast<ResultType*>(m_slot));
    }

    template <typename ResultType>
    const ResultType* slot_ptr() const noexcept
    {
        return std::launder(reinterpret_cast<const ResultType*>(m_slot));
    }

    void*       payload_ptr_erased(detail::payload_layout layout) noexcept;
    const void* payload_ptr_erased(detail::payload_layout layout) const noexcept;

    template <typename ResultType>
    ResultType* payload_ptr() noexcept
    {
        auto* raw_ptr =
            payload_ptr_erased(payload_uses_inline_storage<ResultType>() ? detail::payload_layout::inline_object
                                                                         : detail::payload_layout::external_pointer);
        return raw_ptr ? std::launder(reinterpret_cast<ResultType*>(raw_ptr)) : nullptr;
    }

    template <typename ResultType>
    const ResultType* payload_ptr() const noexcept
    {
        auto* raw_ptr =
            payload_ptr_erased(payload_uses_inline_storage<ResultType>() ? detail::payload_layout::inline_object
                                                                         : detail::payload_layout::external_pointer);
        return raw_ptr ? std::launder(reinterpret_cast<const ResultType*>(raw_ptr)) : nullptr;
    }

    bool try_mark_cancelled() noexcept;
    void mark_bound() noexcept;
    void bind_cached_state_destroy(cached_state_destroy_fn destroy_fn) noexcept
    {
        m_cached_state_destroy = destroy_fn;
    }

    std::exception_ptr get_exception_locked() const noexcept;
    void               validate_get_value_locked() const;

    executor_type      m_executor;
    callback_list      m_continuations;
    callback_list      m_cancel_callbacks;
    std::exception_ptr m_exception{nullptr};
    alignas(std::max_align_t) unsigned char m_slot[s_inline_payload_bytes];
    const detail::storage_ops* m_storage_ops{nullptr};
    condition_variable         m_cv;
    std::atomic<uint32_t>      m_state_flags{encode_completion_state_bits(completion_state::pending) |
                                        encode_policy_bits(launch::async)};
    std::atomic_bool           m_cancel_requested{false};
    mutable mc::small_mutex    m_mutex;
    cached_state_destroy_fn    m_cached_state_destroy{nullptr};
};

namespace detail {
MC_API bool pooled_state_try_release(state_base* ptr);
}

template <typename T>
class State : public state_base {
public:
    using value_type  = std::conditional_t<std::is_same_v<T, void>, std::monostate, std::decay_t<T>>;
    using result_type = std::optional<value_type>;

    template <typename Executor>
    State(Executor executor) : state_base(std::forward<Executor>(executor))
    {
        bind_cached_type_();
        bind_storage_();
        construct_empty_storage_();
    }

    template <typename U = std::conditional_t<std::is_same_v<T, void>, std::monostate, T>>
    void set_value(U&& value)
    {
        State::set_value(*this, std::forward<U>(value));
    }

    void set_value()
    {
        static_assert(std::is_same_v<T, void>, "set_value() 只能用于 void 类型");
        State::set_value(*this);
    }

    template <typename U>
    static void set_value(state_base& state, U&& value)
    {
        detail::state_payload_access<T>::set_value(state, std::forward<U>(value));
    }

    static void set_value(state_base& state)
    {
        detail::state_payload_access<T>::set_value(state);
    }

    auto get_value() const -> std::conditional_t<std::is_same_v<T, void>, void, const value_type&>
    {
        if constexpr (std::is_same_v<T, void>) {
            return State::get_value(*this);
        } else {
            return State::get_value(*this);
        }
    }

    static auto get_value(const state_base& state)
        -> std::conditional_t<std::is_same_v<T, void>, void, const value_type&>
    {
        return detail::state_payload_access<T>::get_value(state);
    }

private:
    friend class any_future;
    friend class any_promise;

    static void destroy_cached_state_(state_base* state) noexcept
    {
        static_cast<State*>(state)->~State();
    }

    void bind_cached_type_() noexcept
    {
        bind_cached_state_destroy(&State::destroy_cached_state_);
    }

    void bind_storage_() noexcept
    {
        m_storage_ops = &detail::state_payload_binding<result_type>::value;
    }

    void construct_empty_storage_()
    {
        MC_ASSERT(m_storage_ops != nullptr, "storage ops 必须已绑定");
        m_storage_ops->construct_empty_slot(*this);
    }

    result_type& result()
    {
        MC_ASSERT(m_storage_ops != nullptr, "storage ops 必须已绑定");
        return *payload_ptr<result_type>();
    }

    const result_type& result() const
    {
        MC_ASSERT(m_storage_ops != nullptr, "storage ops 必须已绑定");
        return *payload_ptr<result_type>();
    }
};

#include <mc/futures/detail/state_storage_impl.h>

} // namespace mc::futures

namespace mc::memory::detail {

template <>
struct shared_release_ops_for<mc::futures::state_base> {
    static void destroy(shared_base* base) noexcept
    {
        static_cast<mc::futures::state_base*>(base)->destory();
    }

    static void deallocate(shared_base* base) noexcept;

    inline static const shared_release_ops value{&destroy, &deallocate};
};

} // namespace mc::memory::detail

#endif // MC_FUTURES_STATE_H
