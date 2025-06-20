/*
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

#ifndef MC_CORE_OBJECT_H
#define MC_CORE_OBJECT_H

#include <mc/common.h>
#include <mc/exception.h>
#include <mc/future.h>
#include <mc/memory.h>
#include <mc/runtime.h>
#include <mc/signal_slot.h>

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc::core {
class object;
class object_impl;

using object_ptr       = mc::shared_ptr<object>;
using const_object_ptr = mc::shared_ptr<const object>;
using object_weak_ptr  = mc::weak_ptr<object>;
using child_list       = std::vector<object_ptr>;
using signal_type      = void*;

using connection_id_type                           = uint32_t;
constexpr connection_id_type INVALID_CONNECTION_ID = std::numeric_limits<connection_id_type>::max();

enum class connection_type {
    Auto,
    Direct,
    Queued
};

using object_id_type = uint64_t;

class object_base : public shared_base<object_base> {
public:
    using executor_type = mc::any_executor;

    object_base()          = default;
    virtual ~object_base() = default;

    object_base(const object_base& other)
        : shared_base<object_base>(other), m_object_id(other.m_object_id) {
    }

    object_base(object_base&& other)
        : shared_base<object_base>(std::forward<object_base>(other)), m_object_id(other.m_object_id) {
        other.m_object_id = 0;
    }

    object_base& operator=(object_base&& other) {
        if (this != &other) {
            shared_base<object_base>::operator=(std::forward<object_base>(other));
            m_object_id       = other.m_object_id;
            other.m_object_id = 0;
        }
        return *this;
    }

    object_base& operator=(const object_base& other) {
        if (this != &other) {
            shared_base<object_base>::operator=(other);
            m_object_id = other.m_object_id;
        }
        return *this;
    }

    /**
     * 获取对象ID
     * @return 对象ID
     */
    virtual object_id_type get_object_id() const {
        return m_object_id;
    }

    /**
     * 设置对象ID
     * @param id 对象ID
     */
    void set_object_id(object_id_type id) {
        m_object_id = id;
    }

    /**
     * 检查对象ID是否有效
     * @return 如果ID不为0则返回true
     */
    bool has_valid_id() const {
        return m_object_id != 0;
    }

protected:
    object_id_type m_object_id{0};
};

/**
 * @brief 对象基类，提供对象层次结构和生命周期管理
 */
class object : public object_base {
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
     * @return 对象名称的副本
     */
    std::string get_name() const;

    /**
     * @brief 设置对象名称
     * @param name 新的对象名称
     */
    void set_name(std::string_view name);

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
     * @brief 获取子对象列表
     * @return 子对象指针列表的副本
     */
    child_list get_children() const;

    /**
     * @brief 查找子对象
     * @param name 子对象名称
     * @return 子对象指针，如果未找到则返回nullptr
     */
    object_ptr find_child(std::string_view name) const;

    /**
     * @brief 连接信号和槽
     * @param sig 信号对象
     * @param slot 槽函数
     * @param type 连接类型
     * @return 连接ID
     */
    template <typename RetType, typename... Args, typename SlotType>
    connection_id_type connect(mc::signal<RetType(Args...)>& sig, SlotType&& slot,
                               connection_type type = connection_type::Auto) {
        return connect(INVALID_CONNECTION_ID, sig, std::forward<SlotType>(slot), type);
    }

    /**
     * @brief 使用用户自定义的连接ID连接信号和槽
     * @param id 连接ID
     * @param sig 信号对象
     * @param slot 槽函数
     * @param type 连接类型
     */
    template <typename RetType, typename... Args, typename SlotType>
    connection_id_type connect(connection_id_type id, mc::signal<RetType(Args...)>& sig,
                               SlotType&& slot, connection_type type = connection_type::Auto) {
        mc::connection_type conn;

        if (type == connection_type::Auto) {
            conn = sig.connect([this, slot = std::forward<SlotType>(slot)](auto&&... args) mutable {
                this->dispatch([slot, args = std::make_tuple(
                                          std::forward<decltype(args)>(args)...)]() mutable {
                    std::apply(slot, std::move(args));
                });
            });
        } else if (type == connection_type::Queued) {
            conn = sig.connect([this, slot = std::forward<SlotType>(slot)](auto&&... args) mutable {
                this->post([slot, args = std::make_tuple(
                                      std::forward<decltype(args)>(args)...)]() mutable {
                    std::apply(slot, std::move(args));
                });
            });
        } else {
            conn = sig.connect(std::forward<SlotType>(slot));
        }

        return add_connection(&sig, std::move(conn), id);
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
    void disconnect(SignalType& sig) const {
        disconnect_all(&sig);
    }

    /**
     * @brief 异步投递任务到对象关联的执行器
     * @tparam CompletionToken 完成令牌类型
     * @param token 完成令牌
     * @return 根据完成令牌类型返回相应的结果
     */
    template <typename CompletionToken, typename Allocator = std::allocator<void>>
    auto post(CompletionToken&& token, Allocator alloc = Allocator()) const {
        return get_executor().post(std::forward<CompletionToken>(token), alloc);
    }

    /**
     * @brief 延迟投递任务到对象关联的执行器
     * @tparam CompletionToken 完成令牌类型
     * @param token 完成令牌
     * @return 根据完成令牌类型返回相应的结果
     */
    template <typename CompletionToken, typename Allocator = std::allocator<void>>
    auto defer(CompletionToken&& token, Allocator alloc = Allocator()) const {
        return get_executor().defer(std::forward<CompletionToken>(token), alloc);
    }

    /**
     * @brief 分派任务到对象关联的执行器（可能立即执行）
     * @tparam CompletionToken 完成令牌类型
     * @param token 完成令牌
     * @return 根据完成令牌类型返回相应的结果
     */
    template <typename CompletionToken, typename Allocator = std::allocator<void>>
    auto dispatch(CompletionToken&& token, Allocator alloc = Allocator()) const {
        return get_executor().dispatch(std::forward<CompletionToken>(token), alloc);
    }

    /**
     * @brief 在对象关联的执行器上绑定处理器
     * @tparam Handler 处理器类型
     * @param handler 处理器函数或仿函数
     * @return 绑定到执行器的处理器
     */
    template <typename Handler>
    auto bind_executor(Handler&& handler) const {
        return boost::asio::bind_executor(get_executor(), std::forward<Handler>(handler));
    }

    executor_type get_executor() const;
    void          set_executor(mc::executor executor);

    /**
     * @brief 获取当前对象的ref_ptr
     * @return 指向当前对象的object_ptr
     */
    object_ptr shared_from_this() {
        return object_base::shared_from_this().template static_pointer_cast<object>();
    }

    /**
     * @brief 获取当前对象的weak_ptr
     * @return 指向当前对象的mc::weak_ptr<object>
     */
    mc::weak_ptr<object> weak_from_this() {
        return mc::weak_ptr<object>(this);
    }

    /**
     * @brief 获取当前对象的weak_ptr (const版本)
     * @return 指向当前对象的mc::weak_ptr<const object>
     */
    mc::weak_ptr<const object> weak_from_this() const {
        return mc::weak_ptr<const object>(this);
    }

protected:
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

    connection_id_type add_connection(signal_type sig, mc::connection_type conn,
                                      connection_id_type id);
    void               disconnect_all(signal_type sig);

private:
    friend class object_impl;

    object_impl& ensure_impl() const;

    mutable std::atomic<object_impl*> m_object_impl{nullptr};

    // 辅助方法
    void cleanup_on_destroy() noexcept;
};

} // namespace mc::core

#endif // MC_CORE_OBJECT_H