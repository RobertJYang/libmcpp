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

#ifndef MC_FUTURES_CALLBACK_POOL_H
#define MC_FUTURES_CALLBACK_POOL_H

#include <mc/common.h>
#include <mc/singleton.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

namespace mc::futures {

class any_promise;
class state_base;

template <typename F>
void safe_invoke(F&& callback)
{
    try {
        callback();
    } catch (...) {
        // 忽略异常
    }
}

namespace detail {

struct future_result_ready_holder_base {
    any_promise* promise{nullptr};
    state_base*  state{nullptr};
    void (*invoke_body)(void*){nullptr};
    void (*destroy_self)(future_result_ready_holder_base*) noexcept {nullptr};
    std::atomic<std::size_t> ref_count{1};
};

MC_API void invoke_future_result_ready_holder(void* ptr);
// catch_error 等：不能在 invoke 前走 handle_result_state_common 的 rejected 直通分支
MC_API void  invoke_future_error_handler_holder(void* ptr);
MC_API void* clone_future_result_ready_holder(const void* ptr);
MC_API void  destroy_future_result_ready_holder(void* ptr) noexcept;

template <typename Context>
struct future_result_ready_holder : future_result_ready_holder_base {
    using context_type = Context;

    template <typename U>
    explicit future_result_ready_holder(U&& context) : m_context(std::forward<U>(context))
    {
        this->promise      = &m_context.promise;
        this->state        = m_context.state.get();
        this->invoke_body  = &future_result_ready_holder::invoke_body_impl;
        this->destroy_self = &future_result_ready_holder::destroy_self_impl;
    }

private:
    static void invoke_body_impl(void* ptr)
    {
        auto* holder = static_cast<future_result_ready_holder*>(ptr);
        holder->m_context.apply();
    }

    static void destroy_self_impl(future_result_ready_holder_base* ptr) noexcept
    {
        delete static_cast<future_result_ready_holder*>(ptr);
    }

    Context m_context;
};

} // namespace detail

class MC_API callback_type {
public:
    callback_type() noexcept = default;
    callback_type(std::nullptr_t) noexcept
    {}

    template <typename F, std::enable_if_t<!std::is_same_v<std::decay_t<F>, callback_type>, int> = 0>
    callback_type(F&& callback)
    {
        bind(std::forward<F>(callback));
    }

    callback_type(const callback_type& other)
    {
        copy_from(other);
    }

    callback_type(callback_type&& other) noexcept
    {
        move_from(std::move(other));
    }

    callback_type& operator=(const callback_type& other)
    {
        if (this != &other) {
            reset();
            copy_from(other);
        }
        return *this;
    }

    callback_type& operator=(callback_type&& other) noexcept
    {
        if (this != &other) {
            reset();
            move_from(std::move(other));
        }
        return *this;
    }

    callback_type& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    ~callback_type();

    explicit operator bool() const noexcept
    {
        return m_invoke != nullptr;
    }

    template <typename Context>
    static callback_type make_future_result_ready(Context&& context)
    {
        using holder_type = detail::future_result_ready_holder<std::decay_t<Context>>;
        static_assert(std::is_copy_constructible_v<typename holder_type::context_type>,
                      "callback_type 只支持可拷贝回调");

        callback_type callback;
        callback.m_data    = new holder_type(std::forward<Context>(context));
        callback.m_invoke  = &detail::invoke_future_result_ready_holder;
        callback.m_clone   = &detail::clone_future_result_ready_holder;
        callback.m_destroy = &detail::destroy_future_result_ready_holder;
        return callback;
    }

    template <typename Context>
    static callback_type make_future_error_ready(Context&& context)
    {
        using holder_type = detail::future_result_ready_holder<std::decay_t<Context>>;
        static_assert(std::is_copy_constructible_v<typename holder_type::context_type>,
                      "callback_type 只支持可拷贝回调");

        callback_type callback;
        callback.m_data    = new holder_type(std::forward<Context>(context));
        callback.m_invoke  = &detail::invoke_future_error_handler_holder;
        callback.m_clone   = &detail::clone_future_result_ready_holder;
        callback.m_destroy = &detail::destroy_future_result_ready_holder;
        return callback;
    }

    void operator()();
    void reset() noexcept;

private:
    template <typename F>
    void bind(F&& callback)
    {
        using holder_type = callback_holder<std::decay_t<F>>;
        static_assert(std::is_copy_constructible_v<typename holder_type::callback_storage_type>,
                      "callback_type 只支持可拷贝回调");
        m_data    = new holder_type(std::forward<F>(callback));
        m_invoke  = &holder_type::invoke;
        m_clone   = &holder_type::clone;
        m_destroy = &holder_type::destroy;
    }

    void copy_from(const callback_type& other)
    {
        if (!other.m_clone) {
            return;
        }
        m_data    = other.m_clone(other.m_data);
        m_invoke  = other.m_invoke;
        m_clone   = other.m_clone;
        m_destroy = other.m_destroy;
    }

    void move_from(callback_type&& other) noexcept
    {
        m_data          = other.m_data;
        m_invoke        = other.m_invoke;
        m_clone         = other.m_clone;
        m_destroy       = other.m_destroy;
        other.m_data    = nullptr;
        other.m_invoke  = nullptr;
        other.m_clone   = nullptr;
        other.m_destroy = nullptr;
    }

    template <typename F>
    struct callback_holder {
        using callback_storage_type = F;

        template <typename U>
        explicit callback_holder(U&& callback) : m_callback(std::forward<U>(callback))
        {}

        static void invoke(void* ptr)
        {
            auto* holder = static_cast<callback_holder*>(ptr);
            holder->m_callback();
        }

        static void* clone(const void* ptr)
        {
            const auto* holder = static_cast<const callback_holder*>(ptr);
            return new callback_holder(holder->m_callback);
        }

        static void destroy(void* ptr) noexcept
        {
            delete static_cast<callback_holder*>(ptr);
        }

        F m_callback;
    };

    void* m_data{nullptr};
    void (*m_invoke)(void*){nullptr};
    void* (*m_clone)(const void*){nullptr};
    void (*m_destroy)(void*) noexcept {nullptr};
};

struct callback_node : public mc::noncopyable {
    explicit callback_node(callback_type callback) : m_callback(std::move(callback))
    {}

    callback_type                  m_callback;
    std::unique_ptr<callback_node> m_next;
};

using callback_node_ptr = std::unique_ptr<callback_node>;

class MC_API callback_pool {
public:
    template <typename Tag>
    static callback_pool& instance(Tag)
    {
        return mc::singleton<callback_pool, Tag>::instance();
    }

    static callback_pool& instance();

    std::unique_ptr<callback_node> acquire_node(callback_type callback);

    void release_node(std::unique_ptr<callback_node> node);

    void set_max_pool_size(std::size_t max_size);

    struct pool_stats {
        std::size_t pool_size = 0; // 池中缓存的节点数量
        std::size_t max_size  = 0; // 池的最大容量
    };

    pool_stats get_stats() const;

    void clear();

private:
    callback_pool()  = default;
    ~callback_pool() = default;

    template <typename Class, typename Tag>
    friend class mc::detail::singleton_impl;

    template <typename Class, typename Tag>
    friend class mc::singleton;

    mutable std::mutex             m_mutex;               // 保护池的互斥锁
    std::unique_ptr<callback_node> m_pool_head;           // 池的头节点
    std::size_t                    m_pool_size{0};        // 池中节点数量
    std::size_t                    m_max_pool_size{1000}; // 默认最大池大小
};

class MC_API callback_list {
public:
    callback_list() = default;

    callback_list(const callback_list&)            = delete;
    callback_list& operator=(const callback_list&) = delete;

    callback_list(callback_list&& other) noexcept            = default;
    callback_list& operator=(callback_list&& other) noexcept = default;

    void        push_back(callback_type callback);
    void        swap(callback_list& other) noexcept;
    void        clear();
    bool        empty() const;
    std::size_t size() const;

    void execute_and_clear();

private:
    callback_node_ptr m_head;
    callback_node*    m_tail{nullptr};
};

} // namespace mc::futures

#endif // MC_FUTURES_CALLBACK_POOL_H
