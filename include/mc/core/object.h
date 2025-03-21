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

#include <algorithm>
#include <atomic>
#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mc {
namespace core {

// 前向声明
class event;

/**
 * @brief 对象基类，提供对象层次结构和生命周期管理
 *
 * object类是框架中所有对象的基类，提供以下功能：
 * - 对象命名
 * - 父子对象关系管理
 * - 对象树遍历
 * - 线程安全的事件处理
 * - 与boost::asio::io_context集成
 */
class object : public std::enable_shared_from_this<object> {
public:
    // 添加友元声明
    friend class event_dispatcher;
    friend class standard_event_dispatcher;

    // 类型定义
    using io_context_type = boost::asio::io_context;
    using executor_type = io_context_type::executor_type;
    using strand_type = boost::asio::strand<executor_type>;
    using object_ptr = std::shared_ptr<object>;
    using const_object_ptr = std::shared_ptr<const object>;
    using weak_object_ptr = std::weak_ptr<object>;

    /**
     * @brief 创建对象实例
     * @param name 对象名称
     * @param io_context IO上下文
     * @param parent 父对象指针，默认为nullptr
     * @return 对象智能指针
     */
    template <typename T, typename... Args>
    static std::shared_ptr<T> create(std::string name, io_context_type& io_context, 
                                     std::shared_ptr<object> parent = nullptr, Args&&... args) {
        auto obj = std::make_shared<T>(std::move(name), io_context, std::forward<Args>(args)...);
        if (parent) {
            obj->set_parent(parent);
        }
        return obj;
    }

    /**
     * @brief 构造函数
     * @param name 对象名称
     * @param io_context IO上下文
     * @param parent 父对象指针，默认为nullptr
     */
    explicit object(std::string name, io_context_type& io_context, 
                    std::shared_ptr<object> parent = nullptr);

    /**
     * @brief 析构函数，会自动删除所有子对象
     */
    virtual ~object() noexcept;

    /**
     * @brief 获取对象名称
     * @return 对象名称
     */
    const std::string& name() const {
        return m_name;
    }

    /**
     * @brief 设置对象名称
     * @param name 新的对象名称
     */
    void set_name(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_name = name;
    }

    /**
     * @brief 获取父对象
     * @return 父对象指针，如果没有父对象则返回nullptr
     */
    std::shared_ptr<object> parent() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_parent.lock();
    }

    /**
     * @brief 设置父对象
     * @param parent 新的父对象指针
     *
     * 如果对象已经有父对象，则会先从原父对象的子对象列表中移除
     */
    void set_parent(std::shared_ptr<object> parent);

    /**
     * @brief 获取子对象列表
     * @return 子对象指针列表的副本
     */
    std::vector<std::shared_ptr<object>> children() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_children;
    }

    /**
     * @brief 查找子对象
     * @param name 子对象名称
     * @return 子对象指针，如果未找到则返回nullptr
     */
    std::shared_ptr<object> find_child(const std::string& name) const;

    /**
     * @brief 查找子对象（递归）
     * @param name 子对象名称
     * @return 子对象指针，如果未找到则返回nullptr
     */
    std::shared_ptr<object> find_child_recursive(const std::string& name) const;

    /**
     * @brief 获取IO上下文
     * @return IO上下文引用
     */
    io_context_type& io_context() const {
        return m_io_context;
    }

    /**
     * @brief 获取执行器
     * @return 执行器引用
     */
    executor_type get_executor() const {
        return m_io_context.get_executor();
    }

    /**
     * @brief 获取strand
     * @return strand引用
     */
    strand_type& strand() const {
        return m_strand;
    }

    /**
     * @brief 在对象的执行器上异步执行任务
     * @param task 要执行的任务
     */
    template <typename Task>
    void post(Task&& task) {
        boost::asio::post(m_strand, std::forward<Task>(task));
    }

    /**
     * @brief 在对象的执行器上异步执行任务，带延迟
     * @param duration 延迟时间
     * @param task 要执行的任务
     */
    template <typename Duration, typename Task>
    void post_after(Duration duration, Task&& task) {
        auto timer = std::make_shared<boost::asio::steady_timer>(m_io_context, duration);
        timer->async_wait(
            boost::asio::bind_executor(m_strand, [timer, task = std::forward<Task>(task)](const boost::system::error_code& ec) {
                if (!ec) {
                    task();
                }
            }));
    }

    /**
     * @brief 向对象发送事件
     * @param e 要发送的事件
     * @return 如果事件被处理则返回true，否则返回false
     */
    virtual bool post_event(std::shared_ptr<event> e);

    /**
     * @brief 处理事件
     * @param e 要处理的事件
     * @return 如果事件被处理则返回true，否则返回false
     */
    virtual bool handle_event(std::shared_ptr<event> e);

    // 禁止拷贝构造和赋值操作
    object(const object&)            = delete;
    object& operator=(const object&) = delete;

protected:
    /**
     * @brief 获取事件过滤器列表
     * @return 事件过滤器列表的副本
     */
    std::vector<std::shared_ptr<object>> event_filters() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_event_filters;
    }

    /**
     * @brief 安装事件过滤器
     * @param filter 事件过滤器
     */
    void install_event_filter(std::shared_ptr<object> filter);

    /**
     * @brief 移除事件过滤器
     * @param filter 事件过滤器
     */
    void remove_event_filter(std::shared_ptr<object> filter);

    /**
     * @brief 过滤事件
     * @param target 事件目标对象
     * @param e 要过滤的事件
     * @return 如果事件被过滤器处理则返回true，否则返回false
     */
    virtual bool filter_event(std::shared_ptr<object> target, std::shared_ptr<event> e);

private:
    std::string m_name;                 ///< 对象名称
    weak_object_ptr m_parent;           ///< 父对象弱引用
    std::vector<object_ptr> m_children; ///< 子对象列表
    std::vector<object_ptr> m_event_filters; ///< 事件过滤器列表
    
    mutable std::mutex m_mutex;         ///< 对象锁，保护对象状态
    std::atomic<bool> m_deleting;       ///< 对象是否正在删除
    
    io_context_type& m_io_context;      ///< IO上下文引用
    mutable strand_type m_strand;       ///< 执行器strand

    /**
     * @brief 添加子对象
     * @param child 子对象指针
     */
    void add_child(std::shared_ptr<object> child);

    /**
     * @brief 移除子对象
     * @param child 子对象指针
     */
    void remove_child(std::shared_ptr<object> child);
};

} // namespace core
} // namespace mc

#endif // MC_CORE_OBJECT_H 