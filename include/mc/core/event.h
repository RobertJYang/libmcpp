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

#ifndef MC_CORE_EVENT_H
#define MC_CORE_EVENT_H

#include <atomic>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>

namespace mc {
namespace core {

// 前向声明
class object;

/**
 * @brief 事件基类
 *
 * 所有事件类型都应该继承自此类，支持多线程环境下的事件处理
 */
class event : public std::enable_shared_from_this<event> {
public:
    /**
     * @brief 事件类型枚举
     */
    enum class event_type {
        none = 0,           ///< 未定义事件
        timer = 1,          ///< 定时器事件
        application = 100,  ///< 应用程序事件
        window = 200,       ///< 窗口事件
        input = 300,        ///< 输入事件
        device = 400,       ///< 设备事件
        network = 500,      ///< 网络事件
        service = 600,      ///< 服务事件
        plugin = 700,       ///< 插件事件
        supervisor = 800,   ///< 监督器事件
        user = 1000,        ///< 用户自定义事件
        max_event = 65535   ///< 最大事件类型
    };

    /**
     * @brief 构造函数
     * @param type 事件类型
     */
    explicit event(event_type type = event_type::none)
        : m_type(type)
        , m_accepted(false)
        , m_handled(false)
        , m_propagate(true) {}

    /**
     * @brief 虚析构函数
     */
    virtual ~event() = default;

    /**
     * @brief 获取事件类型
     * @return 事件类型
     */
    event_type type() const {
        return m_type;
    }

    /**
     * @brief 获取事件的类型标识
     * @return 事件类型的type_index
     */
    virtual std::type_index type_index() const {
        return std::type_index(typeid(*this));
    }

    /**
     * @brief 判断事件是否被接受
     * @return 如果事件被接受则返回true，否则返回false
     */
    bool is_accepted() const {
        return m_accepted;
    }

    /**
     * @brief 接受事件
     */
    void accept() {
        m_accepted = true;
    }

    /**
     * @brief 忽略事件
     */
    void ignore() {
        m_accepted = false;
    }

    /**
     * @brief 判断事件是否被处理
     * @return 如果事件被处理则返回true，否则返回false
     */
    bool is_handled() const {
        return m_handled;
    }

    /**
     * @brief 设置事件处理状态
     * @param handled 事件是否被处理
     */
    void set_handled(bool handled) {
        m_handled = handled;
    }

    /**
     * @brief 判断事件是否继续传播
     * @return 如果事件继续传播则返回true，否则返回false
     */
    bool should_propagate() const {
        return m_propagate;
    }

    /**
     * @brief 设置事件传播状态
     * @param propagate 事件是否继续传播
     */
    void set_propagate(bool propagate) {
        m_propagate = propagate;
    }

    /**
     * @brief 获取事件的发送者
     * @return 事件发送者的指针
     */
    std::shared_ptr<object> sender() const {
        return m_sender.lock();
    }

    /**
     * @brief 设置事件的发送者
     * @param sender 事件发送者的指针
     */
    void set_sender(std::shared_ptr<object> sender) {
        m_sender = sender;
    }

    /**
     * @brief 获取事件的目标
     * @return 事件目标的指针
     */
    std::shared_ptr<object> target() const {
        return m_target.lock();
    }

    /**
     * @brief 设置事件的目标
     * @param target 事件目标的指针
     */
    void set_target(std::shared_ptr<object> target) {
        m_target = target;
    }

    /**
     * @brief 克隆事件
     * @return 事件的克隆
     */
    virtual std::shared_ptr<event> clone() const {
        return std::make_shared<event>(m_type);
    }

private:
    event_type m_type;                  ///< 事件类型
    std::atomic<bool> m_accepted;       ///< 事件是否被接受
    std::atomic<bool> m_handled;        ///< 事件是否被处理
    std::atomic<bool> m_propagate;      ///< 事件是否继续传播
    std::weak_ptr<object> m_sender;     ///< 事件发送者
    std::weak_ptr<object> m_target;     ///< 事件目标
};

/**
 * @brief 事件处理器接口
 *
 * 所有需要处理事件的类都应该实现此接口
 */
class event_handler {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~event_handler() = default;

    /**
     * @brief 事件处理函数
     * @param e 要处理的事件
     * @return 如果事件被处理则返回true，否则返回false
     */
    virtual bool handle_event(event& e) = 0;
};

/**
 * @brief 事件过滤器接口
 *
 * 事件过滤器可以在事件到达目标对象之前拦截和处理事件
 */
class event_filter {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~event_filter() = default;

    /**
     * @brief 过滤事件
     * @param e 要过滤的事件
     * @param target 事件目标对象
     * @return 如果事件被过滤器处理则返回true，否则返回false
     */
    virtual bool filter_event(event& e, event_handler* target) = 0;
};

/**
 * @brief 事件分发器类
 *
 * 负责在多线程环境中安全地分发事件
 */
class event_dispatcher {
public:
    /**
     * @brief 构造函数
     */
    event_dispatcher() = default;

    /**
     * @brief 虚析构函数
     */
    virtual ~event_dispatcher() = default;

    /**
     * @brief 分发事件到指定目标
     * @param e 事件指针
     * @param target 目标对象指针
     * @return 如果事件被处理则返回true，否则返回false
     */
    virtual bool dispatch(std::shared_ptr<event> e, std::shared_ptr<object> target) = 0;

    /**
     * @brief 过滤事件
     * @param e 事件指针
     * @param target 目标对象指针
     * @return 如果事件被过滤则返回true，否则返回false
     */
    virtual bool filter(std::shared_ptr<event> e, std::shared_ptr<object> target) = 0;
};

/**
 * @brief 标准事件分发器实现
 */
class standard_event_dispatcher : public event_dispatcher {
public:
    /**
     * @brief 构造函数
     */
    standard_event_dispatcher() = default;

    /**
     * @brief 析构函数
     */
    ~standard_event_dispatcher() override = default;

    /**
     * @brief 分发事件到指定目标
     * @param e 事件指针
     * @param target 目标对象指针
     * @return 如果事件被处理则返回true，否则返回false
     */
    bool dispatch(std::shared_ptr<event> e, std::shared_ptr<object> target) override;

    /**
     * @brief 过滤事件
     * @param e 事件指针
     * @param target 目标对象指针
     * @return 如果事件被过滤则返回true，否则返回false
     */
    bool filter(std::shared_ptr<event> e, std::shared_ptr<object> target) override;
};

/**
 * @brief 获取全局事件分发器
 * @return 事件分发器指针
 */
std::shared_ptr<event_dispatcher> global_event_dispatcher();

/**
 * @brief 设置全局事件分发器
 * @param dispatcher 新的事件分发器
 */
void set_global_event_dispatcher(std::shared_ptr<event_dispatcher> dispatcher);

} // namespace core
} // namespace mc

#endif // MC_CORE_EVENT_H 