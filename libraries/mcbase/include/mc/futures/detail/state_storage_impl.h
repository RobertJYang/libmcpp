/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_FUTURES_DETAIL_STATE_STORAGE_IMPL_H
#define MC_FUTURES_DETAIL_STATE_STORAGE_IMPL_H

namespace detail {

template <typename ResultType>
struct state_payload_allocator {
    static ResultType* allocate()
    {
        return new ResultType();
    }

    static void deallocate(ResultType* ptr) noexcept
    {
        delete ptr;
    }
};

template <typename ResultType>
struct state_payload_binding {
    using storage_type =
        std::conditional_t<state_base::template payload_uses_inline_storage<ResultType>(), ResultType, ResultType*>;

    static storage_type& storage(state_base& state) noexcept
    {
        return *std::launder(reinterpret_cast<storage_type*>(state.m_slot));
    }

    static const storage_type& storage(const state_base& state) noexcept
    {
        return *std::launder(reinterpret_cast<const storage_type*>(state.m_slot));
    }

    static void destroy_slot(state_base& state) noexcept
    {
        auto* const result = state.template payload_ptr<ResultType>();
        if (result != nullptr) {
            if constexpr (state_base::template payload_uses_inline_storage<ResultType>()) {
                if constexpr (!std::is_trivially_destructible_v<storage_type>) {
                    storage(state).~storage_type();
                }
            } else {
                state_payload_allocator<ResultType>::deallocate(storage(state));
                storage(state) = nullptr;
            }
        }

        state.set_storage_kind(detail::storage_kind::empty);
        state.set_payload_constructed(false);
    }

    static void construct_empty_slot(state_base& state)
    {
        if constexpr (state_base::template payload_uses_inline_storage<ResultType>()) {
            state.template construct_inline_slot<ResultType>();
        } else {
            auto* storage_ptr = state.template construct_external_slot<storage_type>();
            *storage_ptr      = state_payload_allocator<ResultType>::allocate();
        }
    }

    inline static const detail::storage_ops value{
        &destroy_slot,
        &construct_empty_slot,
    };
};

template <typename T>
struct state_payload_access {
    using value_type  = std::conditional_t<std::is_same_v<T, void>, std::monostate, std::decay_t<T>>;
    using result_type = std::optional<value_type>;

    template <typename U>
    static void set_value(state_base& state, U&& value)
    {
        static_assert(!std::is_same_v<T, void>, "带参数的 set_value() 不能用于 void 类型");
        result(state).emplace(std::forward<U>(value));
    }

    static void set_value(state_base& state)
    {
        static_assert(std::is_same_v<T, void>, "set_value() 只能用于 void 类型");
        result(state).emplace(std::monostate{});
    }

    static auto get_value(const state_base& state)
        -> std::conditional_t<std::is_same_v<T, void>, void, const value_type&>
    {
        std::lock_guard lock(state.m_mutex);
        state.validate_get_value_locked();
        if constexpr (std::is_same_v<T, void>) {
            return;
        } else {
            return *result(state);
        }
    }

private:
    static result_type& result(state_base& state)
    {
        MC_ASSERT(state.m_storage_ops != nullptr, "storage ops 必须已绑定");
        return *state.template payload_ptr<result_type>();
    }

    static const result_type& result(const state_base& state)
    {
        MC_ASSERT(state.m_storage_ops != nullptr, "storage ops 必须已绑定");
        return *state.template payload_ptr<result_type>();
    }
};

} // namespace detail

#endif // MC_FUTURES_DETAIL_STATE_STORAGE_IMPL_H
