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

#ifndef MC_CORE_OBJECT_H
#define MC_CORE_OBJECT_H

#include <mc/common.h>
#include <mc/core/object_base.h>
#include <mc/event.h>
#include <mc/exception.h>
#include <mc/future.h>
#include <mc/memory.h>
#include <mc/runtime.h>
#include <mc/signal/connection.h>
#include <mc/signal/signal.h>

#include <atomic>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace mc::core {
class object;
class object_impl;

namespace detail {
enum class executor_delivery { dispatch, post };

template <typename Handler>
class object_executor_binder {
public:
    using handler_type = std::decay_t<Handler>;

    explicit object_executor_binder(mc::runtime::any_executor executor, Handler&& handler,
                                    executor_delivery delivery = executor_delivery::dispatch)
        : m_executor(std::move(executor)), m_handler(std::make_shared<handler_type>(std::forward<Handler>(handler))),
          m_delivery(delivery)
    {}

    template <typename... Args>
    void operator()(Args&&... args) const
    {
        auto handler = m_handler;
        auto tuple   = std::make_tuple(std::forward<Args>(args)...);
        auto task    = [handler = std::move(handler), tuple = std::move(tuple)]() mutable {
            std::apply([&handler](auto&&... unpacked) {
                std::invoke(*handler, std::forward<decltype(unpacked)>(unpacked)...);
            }, std::move(tuple));
        };

        if (m_delivery == executor_delivery::post) {
            m_executor.post(std::move(task));
        } else {
            m_executor.dispatch(std::move(task));
        }
    }

private:
    mc::runtime::any_executor     m_executor;
    std::shared_ptr<handler_type> m_handler;
    executor_delivery             m_delivery;
};
} // namespace detail

using object_ptr           = mc::shared_ptr<object>;
using const_object_ptr     = mc::shared_ptr<const object>;
using object_weak_ptr      = mc::weak_ptr<object>;
using child_list           = std::vector<object_ptr>;
using signal_type          = void*;
using event_filter_handler = std::function<bool(object*, mc::event&)>;
using event_filter_signal  = mc::signal<void(object*, mc::event&)>;

using connection_id_type                           = uint32_t;
constexpr connection_id_type INVALID_CONNECTION_ID = std::numeric_limits<connection_id_type>::max();

enum class connection_type { Auto, Direct, Queued };
enum class connection_mode { dispatch, direct, queued };

/**
 * @brief 对象基类，提供对象层次结构和生命周期管理
 *
 * 继承自 object_base，支持对象存储到数据库中统一查询
 */
class MC_API object : public mc::core::object_base {
public:
    /**
     * @brief 默认构造函数
     */
    object();

    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit object(object* parent);

    /**
     * @brief 析构函数，会自动删除所有子对象
     */
    virtual ~object() noexcept;

    /**
     * @brief 移动构造函数
     * @param other 右值对象
     */
    object(object&& other) = delete;

    /**
     * @brief 移动赋值运算符
     * @param other 右值对象
     */
    object& operator=(object&& other) = delete;

    /**
     * @brief 拷贝构造函数
     * @param other 右值对象
     */
    object(const object& other);

    /**
     * @brief 拷贝赋值运算符
     * @param other 右值对象
     */
    object& operator=(const object& other);

    /**
     * @brief 获取对象名称
     * @return 对象名称的视图
     */
    mc::string_view get_name() const;

    /**
     * @brief 设置对象名称
     * @param name 新的对象名称
     */
    void set_name(mc::string_view name);

    /**
     * @brief 获取父对象
     * @return 父对象指针
     */
    object_ptr get_parent() const;

    /**
     * @brief 设置父对象
     * @param parent 新的父对象指针
     *
     * 如果对象已经有父对象，则会先从原父对象的子对象列表中移除
     */
    void set_parent(object* parent);

    /**
     * @brief 设置父对象
     * @param parent 新的父对象指针
     *
     * 如果对象已经有父对象，则会先从原父对象的子对象列表中移除
     */
    template <typename Object, std::enable_if_t<std::is_base_of_v<object, Object>, int> = 0>
    void set_parent(const mc::shared_ptr<Object>& parent)
    {
        set_parent(parent.get());
    }

    /**
     * @brief 获取子对象列表
     * @return 子对象指针列表的副本
     */
    child_list get_children() const;

    /**
     * @brief 查找子对象
     * @param name 子对象名称
     * @return 子对象指针，如果未找到则返回nullptr
     */
    object_ptr find_child(mc::string_view name) const;

    void           post_event(mc::event& e);
    void           send_event_to_children(mc::event& e);
    mc::connection install_event_filter(event_filter_handler filter, int priority = mc::signal_priority::normal);

    /**
     * @brief 连接信号和槽
     * @param sig 信号对象
     * @param slot 槽函数
     * @param mode 连接模式
     * @param priority 槽优先级，默认 normal
     * @return 连接ID
     */
    template <typename RetType, typename... Args, typename SlotType>
    connection_id_type connect(mc::signal<RetType(Args...)>& sig, SlotType&& slot, connection_mode mode,
                               int priority = mc::signal_priority::normal)
    {
        return connect(INVALID_CONNECTION_ID, sig, std::forward<SlotType>(slot), mode, priority);
    }

    /**
     * @brief 使用用户自定义的连接ID连接信号和槽
     * @param id 连接ID
     * @param sig 信号对象
     * @param slot 槽函数
     * @param mode 连接模式
     * @param priority 槽优先级，默认 normal
     */
    template <typename RetType, typename... Args, typename SlotType>
    connection_id_type connect(connection_id_type id, mc::signal<RetType(Args...)>& sig, SlotType&& slot,
                               connection_mode mode, int priority = mc::signal_priority::normal)
    {
        mc::connection_type conn;

        if constexpr (std::is_void_v<RetType>) {
            if (mode == connection_mode::direct) {
                conn = sig.connect(std::forward<SlotType>(slot), priority);
            } else {
                auto delivery = mode == connection_mode::queued ? detail::executor_delivery::post
                                                                : detail::executor_delivery::dispatch;
                conn          = sig.connect(
                    detail::object_executor_binder<SlotType>(get_executor(), std::forward<SlotType>(slot), delivery),
                    priority);
            }
        } else {
            MC_ASSERT(mode == connection_mode::direct, "非 void 信号仅支持 direct 连接模式");
            conn = sig.connect(std::forward<SlotType>(slot), priority);
        }

        return add_connection(&sig, std::move(conn), id);
    }

    /**
     * @brief 使用指定 executor 连接信号和槽
     * @param sig 信号对象
     * @param slot 槽函数
     * @param executor 目标执行器
     * @param priority 槽优先级，默认 normal
     * @return 连接ID
     */
    template <typename RetType, typename... Args, typename SlotType>
    connection_id_type connect(mc::signal<RetType(Args...)>& sig, SlotType&& slot, executor_type executor,
                               int priority = mc::signal_priority::normal)
    {
        return connect(INVALID_CONNECTION_ID, sig, std::forward<SlotType>(slot), std::move(executor), priority);
    }

    /**
     * @brief 使用用户自定义的连接ID和指定 executor 连接信号和槽
     * @param id 连接ID
     * @param sig 信号对象
     * @param slot 槽函数
     * @param executor 目标执行器
     * @param priority 槽优先级，默认 normal
     */
    template <typename RetType, typename... Args, typename SlotType>
    connection_id_type connect(connection_id_type id, mc::signal<RetType(Args...)>& sig, SlotType&& slot,
                               executor_type executor, int priority = mc::signal_priority::normal)
    {
        static_assert(std::is_void_v<RetType>, "executor 连接仅支持 void 信号");

        auto conn = sig.connect(detail::object_executor_binder<SlotType>(
                                    std::move(executor), std::forward<SlotType>(slot), detail::executor_delivery::post),
                                priority);
        return add_connection(&sig, std::move(conn), id);
    }

    /**
     * @brief 连接信号和槽
     * @param sig 信号对象
     * @param slot 槽函数
     * @param type 兼容旧接口的连接类型
     * @param priority 槽优先级，默认 normal
     * @return 连接ID
     */
    template <typename RetType, typename... Args, typename SlotType>
    connection_id_type connect(mc::signal<RetType(Args...)>& sig, SlotType&& slot,
                               connection_type type = connection_type::Auto, int priority = mc::signal_priority::normal)
    {
        return connect(INVALID_CONNECTION_ID, sig, std::forward<SlotType>(slot), type, priority);
    }

    /**
     * @brief 使用用户自定义的连接ID连接信号和槽
     * @param id 连接ID
     * @param sig 信号对象
     * @param slot 槽函数
     * @param type 连接类型
     * @param priority 槽优先级，默认 normal
     */
    template <typename RetType, typename... Args, typename SlotType>
    connection_id_type connect(connection_id_type id, mc::signal<RetType(Args...)>& sig, SlotType&& slot,
                               connection_type type = connection_type::Auto, int priority = mc::signal_priority::normal)
    {
        switch (type) {
            case connection_type::Direct:
                return connect(id, sig, std::forward<SlotType>(slot), connection_mode::direct, priority);
            case connection_type::Queued:
                return connect(id, sig, std::forward<SlotType>(slot), connection_mode::queued, priority);
            case connection_type::Auto:
            default:
                return connect(id, sig, std::forward<SlotType>(slot), connection_mode::dispatch, priority);
        }
    }

    /**
     * @brief 断开连接
     * @param id 连接ID
     * @return 是否成功断开
     */
    void disconnect(connection_id_type id) const;

    /**
     * @brief 断开信号的所有连接
     * @param sig 信号对象
     * @return 断开的连接数量
     */

    template <typename SignalType>
    void disconnect(SignalType& sig) const
    {
        disconnect_all(&sig);
    }

    /**
     * @brief 异步投递任务到对象关联的执行器
     * @tparam CompletionToken 完成令牌类型
     * @param token 完成令牌
     * @return 根据完成令牌类型返回相应的结果
     */
    template <typename CompletionToken, typename Allocator = std::allocator<void>>
    auto post(CompletionToken&& token, Allocator alloc = Allocator()) const
    {
        return get_executor().post(std::forward<CompletionToken>(token), alloc);
    }

    /**
     * @brief 延迟投递任务到对象关联的执行器
     * @tparam CompletionToken 完成令牌类型
     * @param token 完成令牌
     * @return 根据完成令牌类型返回相应的结果
     */
    template <typename CompletionToken, typename Allocator = std::allocator<void>>
    auto defer(CompletionToken&& token, Allocator alloc = Allocator()) const
    {
        return get_executor().defer(std::forward<CompletionToken>(token), alloc);
    }

    /**
     * @brief 分派任务到对象关联的执行器（可能立即执行）
     * @tparam CompletionToken 完成令牌类型
     * @param token 完成令牌
     * @return 根据完成令牌类型返回相应的结果
     */
    template <typename CompletionToken, typename Allocator = std::allocator<void>>
    auto dispatch(CompletionToken&& token, Allocator alloc = Allocator()) const
    {
        return get_executor().dispatch(std::forward<CompletionToken>(token), alloc);
    }

    /**
     * @brief 在对象关联的执行器上绑定处理器
     * @tparam Handler 处理器类型
     * @param handler 处理器函数或仿函数
     * @return 绑定到执行器的处理器
     */
    template <typename Handler>
    auto bind_executor(Handler&& handler) const
    {
        return detail::object_executor_binder<Handler>(get_executor(), std::forward<Handler>(handler));
    }

    executor_type get_executor() const;
    void          set_executor(executor_type executor);

    /**
     * @brief 获取当前对象的ref_ptr
     * @return 指向当前对象的object_ptr
     */
    object_ptr shared_from_this();

    /**
     * @brief 获取当前对象的weak_ptr
     * @return 指向当前对象的mc::weak_ptr<object>
     */
    mc::weak_ptr<object> weak_from_this();

    /**
     * @brief 获取当前对象的weak_ptr (const版本)
     * @return 指向当前对象的mc::weak_ptr<const object>
     */
    mc::weak_ptr<const object> weak_from_this() const;

protected:
    virtual void on_event(mc::event& e);
    virtual bool event_filter(object* child, mc::event& e);

    /**
     * @brief 添加子对象
     * @param child 子对象指针，必须是ref_ptr管理的对象
     */
    void add_child(object* child);

    /**
     * @brief 移除子对象
     * @param child 子对象指针
     */
    void remove_child(object* child);

    connection_id_type add_connection(signal_type sig, mc::connection_type conn, connection_id_type id);
    void               disconnect_all(signal_type sig);

private:
    friend class object_impl;

    object_impl& ensure_impl() const;
    bool         dispatch_event_filters(object* child, mc::event& e);
    void         propagate_event_from_child(object* child, mc::event& e);

    mutable std::atomic<object_impl*> m_object_impl{nullptr};

    // 辅助方法
    void cleanup_on_destroy() noexcept;
};

} // namespace mc::core

#endif // MC_CORE_OBJECT_H
