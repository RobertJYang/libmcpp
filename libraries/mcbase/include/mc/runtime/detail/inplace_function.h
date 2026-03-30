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

#ifndef MC_RUNTIME_DETAIL_INPLACE_FUNCTION_H
#define MC_RUNTIME_DETAIL_INPLACE_FUNCTION_H

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace mc::runtime::detail {

template <typename Signature, std::size_t InlineSize, std::size_t InlineAlign = alignof(std::max_align_t)>
class inplace_function;

template <typename R, typename... Args, std::size_t InlineSize, std::size_t InlineAlign>
class inplace_function<R(Args...), InlineSize, InlineAlign> {
public:
    inplace_function() noexcept = default;

    template <typename Function, typename = std::enable_if_t<!std::is_same_v<std::decay_t<Function>, inplace_function>>>
    explicit inplace_function(Function&& function)
    {
        emplace(std::forward<Function>(function));
    }

    inplace_function(inplace_function&& other) noexcept
    {
        move_from(std::move(other));
    }

    inplace_function& operator=(inplace_function&& other) noexcept
    {
        if (this != &other) {
            reset();
            move_from(std::move(other));
        }

        return *this;
    }

    inplace_function(const inplace_function&)            = delete;
    inplace_function& operator=(const inplace_function&) = delete;

    ~inplace_function()
    {
        reset();
    }

    explicit operator bool() const noexcept
    {
        return m_ops != nullptr;
    }

    void reset() noexcept
    {
        if (!m_ops) {
            return;
        }

        if (m_inline) {
            m_ops->destroy_inline(m_object);
        } else {
            m_ops->destroy_heap(m_object);
        }

        m_ops    = nullptr;
        m_object = nullptr;
        m_inline = false;
    }

    R operator()(Args... args)
    {
        if constexpr (std::is_void_v<R>) {
            m_ops->invoke(m_object, std::forward<Args>(args)...);
            return;
        } else {
            return m_ops->invoke(m_object, std::forward<Args>(args)...);
        }
    }

private:
    struct ops {
        R (*invoke)(void*, Args&&...);
        void (*destroy_inline)(void*) noexcept;
        void (*destroy_heap)(void*) noexcept;
        void (*move_inline)(void*, void*);
    };

    using storage_type = typename std::aligned_storage<InlineSize, InlineAlign>::type;

    template <typename Function>
    static constexpr bool fits_inline_v = sizeof(Function) <= InlineSize && alignof(Function) <= InlineAlign &&
                                          std::is_nothrow_move_constructible_v<Function>;

    template <typename Function>
    static const ops& get_ops()
    {
        static const ops instance = {
            [](void* object, Args&&... args) -> R {
            if constexpr (std::is_void_v<R>) {
                (*static_cast<Function*>(object))(std::forward<Args>(args)...);
                return;
            } else {
                return (*static_cast<Function*>(object))(std::forward<Args>(args)...);
            }
        },
            [](void* object) noexcept {
            static_cast<Function*>(object)->~Function();
        },
            [](void* object) noexcept {
            delete static_cast<Function*>(object);
        },
            [](void* src, void* dst) {
            auto* source = static_cast<Function*>(src);
            new (dst) Function(std::move(*source));
            source->~Function();
        },
        };

        return instance;
    }

    template <typename Function>
    void emplace(Function&& function)
    {
        using stored_type = std::decay_t<Function>;

        m_ops = &get_ops<stored_type>();
        if constexpr (fits_inline_v<stored_type>) {
            new (&m_storage) stored_type(std::forward<Function>(function));
            m_object = &m_storage;
            m_inline = true;
        } else {
            m_object = new stored_type(std::forward<Function>(function));
            m_inline = false;
        }
    }

    void move_from(inplace_function&& other) noexcept
    {
        m_ops = other.m_ops;
        if (!m_ops) {
            m_object = nullptr;
            m_inline = false;
            return;
        }

        m_inline = other.m_inline;
        if (m_inline) {
            m_ops->move_inline(other.m_object, &m_storage);
            m_object = &m_storage;
        } else {
            m_object = other.m_object;
        }

        other.m_ops    = nullptr;
        other.m_object = nullptr;
        other.m_inline = false;
    }

    const ops*   m_ops{nullptr};
    void*        m_object{nullptr};
    bool         m_inline{false};
    storage_type m_storage;
};

} // namespace mc::runtime::detail

#endif // MC_RUNTIME_DETAIL_INPLACE_FUNCTION_H
