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

#ifndef MC_RUNTIME_THREAD_LIST_H
#define MC_RUNTIME_THREAD_LIST_H

#include <mc/common.h>
#include <mc/intrusive/hook.h>
#include <mc/intrusive/list.h>
#include <mc/intrusive/options.h>

#include <functional>
#include <memory>
#include <thread>

namespace mc::runtime {

/**
 * @brief 线程节点
 */
class MC_API thread_node : public mc::intrusive::list_hook {
public:
    /**
     * @brief 构造函数
     * @param func 要执行的函数
     */
    MC_API explicit thread_node(std::function<void()> func);

    /**
     * @brief 移动构造函数
     */
    MC_API thread_node(thread_node&& other) noexcept;

    /**
     * @brief 析构函数
     */
    MC_API ~thread_node();

    /**
     * @brief 禁止拷贝
     */
    MC_API              thread_node(const thread_node&) = delete;
    MC_API thread_node& operator=(const thread_node&)   = delete;
    MC_API thread_node& operator=(thread_node&&)        = delete;

    /**
     * @brief 检查线程是否可以被 join
     */
    MC_API bool joinable() const;

    /**
     * @brief 等待线程完成
     */
    MC_API void join();

    /**
     * @brief 获取线程ID
     */
    MC_API std::thread::id get_id() const;

private:
    std::thread m_thread;
};

/**
 * @brief 线程列表，管理一组线程的生命周期
 *
 */
class MC_API thread_list {
public:
    using node_list_type = mc::intrusive::list<thread_node>;

    /**
     * @brief 默认构造函数
     */
    MC_API thread_list() = default;

    /**
     * @brief 析构函数，会等待所有线程完成
     */
    MC_API ~thread_list();

    /**
     * @brief 禁止拷贝和移动
     */
    thread_list(const thread_list&)            = delete;
    thread_list(thread_list&&)                 = delete;
    thread_list& operator=(const thread_list&) = delete;
    thread_list& operator=(thread_list&&)      = delete;

    /**
     * @brief 创建并启动指定数量的线程执行相同任务
     * @param thread_count 线程数量
     * @param func 要执行的函数
     */
    MC_API void start_threads(std::size_t thread_count, std::function<void()> func);

    /**
     * @brief 创建并启动指定数量的线程执行相同任务（带线程索引）
     * @param thread_count 线程数量
     * @param func 要执行的函数，参数为线程索引（从0开始）
     */
    MC_API void start_threads(std::size_t thread_count, std::function<void(std::size_t)> func);

    /**
     * @brief 添加单个线程
     * @param func 要执行的函数
     * @return 线程节点的原始指针（仅用于标识，不拥有所有权）
     */
    MC_API thread_node* add_thread(std::function<void()> func);

    /**
     * @brief 移除指定的线程节点（会等待线程完成）
     * @param node 要移除的线程节点指针
     * @return 是否成功移除
     */
    MC_API bool remove_thread(thread_node* node);

    /**
     * @brief 等待所有线程完成
     */
    MC_API void join_all();

    /**
     * @brief 清理所有已完成的线程
     */
    MC_API void clear();

    /**
     * @brief 获取当前线程数量
     */
    MC_API std::size_t get_thread_count() const;

    /**
     * @brief 检查是否为空
     */
    MC_API bool empty() const;

    /**
     * @brief 遍历所有线程节点
     * @param visitor 访问函数
     */
    MC_API void visit_threads(const std::function<void(thread_node&)>& visitor);

    /**
     * @brief 遍历所有线程节点（const版本）
     * @param visitor 访问函数
     */
    MC_API void visit_threads(const std::function<void(const thread_node&)>& visitor) const;

private:
    node_list_type m_threads;
};

} // namespace mc::runtime

#endif // MC_RUNTIME_THREAD_LIST_H