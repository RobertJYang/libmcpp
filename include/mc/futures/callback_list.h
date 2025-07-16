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

struct  callback_node : public mc::noncopyable {
    explicit callback_node(std::function<void()> callback) : m_callback(std::move(callback)) {
    }

    std::function<void()>          m_callback;
    std::unique_ptr<callback_node> m_next;
};

using callback_node_ptr = std::unique_ptr<callback_node>;

class MC_API callback_pool {
public:
    static MC_API callback_pool& instance();

    MC_API std::unique_ptr<callback_node> acquire_node(std::function<void()> callback);

    MC_API void release_node(std::unique_ptr<callback_node> node);

    MC_API void set_max_pool_size(std::size_t max_size);

    struct pool_stats {
        std::size_t pool_size = 0; // 池中缓存的节点数量
        std::size_t max_size  = 0; // 池的最大容量
    };

    MC_API pool_stats get_stats() const;

    MC_API void clear();

private:
    callback_pool()  = default;
    ~callback_pool() = default;

    mutable std::mutex             m_mutex;               // 保护池的互斥锁
    std::unique_ptr<callback_node> m_pool_head;           // 池的头节点
    std::size_t                    m_pool_size{0};        // 池中节点数量
    std::size_t                    m_max_pool_size{1000}; // 默认最大池大小
};

class MC_API callback_list {
public:
    MC_API callback_list() = default;

    callback_list(const callback_list&)            = delete;
    callback_list& operator=(const callback_list&) = delete;

    MC_API                callback_list(callback_list&& other) noexcept = default;
    MC_API callback_list& operator=(callback_list&& other) noexcept     = default;

    MC_API void push_back(std::function<void()> callback);
    MC_API void swap(callback_list& other) noexcept;
    MC_API void clear();
    MC_API bool empty() const;

    MC_API void execute_and_clear();

private:
    callback_node_ptr m_head;
    callback_node*    m_tail{nullptr};
};

} // namespace mc::futures

#endif // MC_FUTURES_CALLBACK_POOL_H