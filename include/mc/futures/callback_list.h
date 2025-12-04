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

#include <functional>
#include <memory>
#include <mutex>

namespace mc::futures {

template <typename F>
void safe_invoke(F&& callback) {
    try {
        callback();
    } catch (...) {
        // 忽略异常
    }
}

struct callback_node : public mc::noncopyable {
    explicit callback_node(std::function<void()> callback) : m_callback(std::move(callback)) {
    }

    std::function<void()>          m_callback;
    std::unique_ptr<callback_node> m_next;
};

using callback_node_ptr = std::unique_ptr<callback_node>;

class MC_API callback_pool {
public:
    template <typename Tag>
    static callback_pool& instance(Tag) {
        return mc::singleton<callback_pool, Tag>::instance();
    }

    static callback_pool& instance();

    std::unique_ptr<callback_node> acquire_node(std::function<void()> callback);

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

    void push_back(std::function<void()> callback);
    void swap(callback_list& other) noexcept;
    void clear();
    bool empty() const;

    void execute_and_clear();

private:
    callback_node_ptr m_head;
    callback_node*    m_tail{nullptr};
};

} // namespace mc::futures

#endif // MC_FUTURES_CALLBACK_POOL_H