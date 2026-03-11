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

#ifndef MC_ALGORITHM_LRU_CACHE_H
#define MC_ALGORITHM_LRU_CACHE_H

#include <mc/common.h>

#include <functional>
#include <list>
#include <optional>
#include <unordered_map>

namespace mc::algorithm {

/**
 * @brief LRU (Least Recently Used) 缓存模板类
 *
 * 使用哈希表 + 双向链表实现，提供 O(1) 时间复杂度的操作。
 *
 * 特性：
 * - get/put 操作均为 O(1) 时间复杂度
 * - 支持自定义键值类型
 * - 支持可选的淘汰回调函数
 * - 非线程安全（需要外部同步）
 *
 * @tparam Key 键类型，需支持 std::hash
 * @tparam Value 值类型
 */
template <typename Key, typename Value>
class lru_cache {
public:
    // 淘汰回调函数类型：void callback(const Key& key, Value&& value)
    using eviction_callback = std::function<void(const Key&, Value&&)>;

    /**
     * @brief 构造函数
     * @param max_size 最大缓存容量，0 表示不限制
     * @param on_evict 可选的淘汰回调函数
     */
    explicit lru_cache(size_t max_size = 0, eviction_callback on_evict = nullptr)
        : m_max_size(max_size), m_on_evict(std::move(on_evict))
    {
    }

    /**
     * @brief 禁用拷贝构造
     */
    lru_cache(const lru_cache&)            = delete;
    lru_cache& operator=(const lru_cache&) = delete;

    /**
     * @brief 支持移动构造
     */
    lru_cache(lru_cache&&) noexcept            = default;
    lru_cache& operator=(lru_cache&&) noexcept = default;

    /**
     * @brief 获取缓存值
     * @param key 键
     * @return 如果存在则返回值的引用，否则返回 std::nullopt
     * @note 获取操作会更新该条目的访问时间（移到链表头部）
     */
    std::optional<std::reference_wrapper<Value>> get(const Key& key)
    {
        auto it = m_cache_map.find(key);
        if (it == m_cache_map.end()) {
            return std::nullopt;
        }

        // 移动到链表头部（最近使用）
        m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second);

        return std::ref(it->second->second);
    }

    /**
     * @brief 添加或更新缓存值
     * @param key 键
     * @param value 值
     * @note 如果缓存已满，会自动淘汰最久未使用的条目
     */
    void put(const Key& key, Value value)
    {
        auto it = m_cache_map.find(key);

        // 如果键已存在，更新值并移到头部
        if (it != m_cache_map.end()) {
            it->second->second = std::move(value);
            m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second);
            return;
        }

        // 检查容量限制
        if (m_max_size > 0 && m_cache_map.size() >= m_max_size) {
            evict_lru();
        }

        // 插入新条目到链表头部
        m_lru_list.emplace_front(key, std::move(value));
        m_cache_map[key] = m_lru_list.begin();
    }

    /**
     * @brief 删除指定键的缓存
     * @param key 键
     * @return 如果键存在则返回 true，否则返回 false
     */
    bool erase(const Key& key)
    {
        auto it = m_cache_map.find(key);
        if (it == m_cache_map.end()) {
            return false;
        }

        m_lru_list.erase(it->second);
        m_cache_map.erase(it);
        return true;
    }

    /**
     * @brief 清空所有缓存
     */
    void clear()
    {
        m_lru_list.clear();
        m_cache_map.clear();
    }

    /**
     * @brief 获取当前缓存大小
     * @return 当前缓存的条目数量
     */
    size_t size() const
    {
        return m_cache_map.size();
    }

    /**
     * @brief 获取最大缓存容量
     * @return 最大缓存容量，0 表示不限制
     */
    size_t max_size() const
    {
        return m_max_size;
    }

    /**
     * @brief 设置最大缓存容量
     * @param max_size 最大缓存容量，0 表示不限制
     * @note 如果新容量小于当前大小，会立即淘汰多余的条目
     */
    void set_max_size(size_t max_size)
    {
        m_max_size = max_size;

        // 如果设置了限制且当前大小超过限制，淘汰多余条目
        while (m_max_size > 0 && m_cache_map.size() > m_max_size) {
            evict_lru();
        }
    }

    /**
     * @brief 检查键是否存在
     * @param key 键
     * @return 如果键存在则返回 true
     */
    bool contains(const Key& key) const
    {
        return m_cache_map.find(key) != m_cache_map.end();
    }

    /**
     * @brief 判断缓存是否为空
     * @return 如果缓存为空则返回 true
     */
    bool empty() const
    {
        return m_cache_map.empty();
    }

    /**
     * @brief 设置淘汰回调函数
     * @param on_evict 淘汰回调函数
     */
    void set_eviction_callback(eviction_callback on_evict)
    {
        m_on_evict = std::move(on_evict);
    }

    /**
     * @brief 遍历所有缓存条目（按访问时间从新到旧）
     * @param func 遍历函数：void func(const Key& key, const Value& value)
     */
    template <typename Func>
    void for_each(Func&& func) const
    {
        for (const auto& [key, value] : m_lru_list) {
            func(key, value);
        }
    }

private:
    // 链表节点类型：pair<Key, Value>
    using list_node     = std::pair<Key, Value>;
    using list_type     = std::list<list_node>;
    using list_iterator = typename list_type::iterator;

    // 淘汰最久未使用的条目（链表尾部）
    void evict_lru()
    {
        if (m_lru_list.empty()) {
            return;
        }

        // 获取最久未使用的条目（链表尾部）
        auto&      oldest = m_lru_list.back();
        const Key& key    = oldest.first;

        // 调用淘汰回调
        if (m_on_evict) {
            m_on_evict(key, std::move(oldest.second));
        }

        // 从哈希表中删除
        m_cache_map.erase(key);

        // 从链表中删除
        m_lru_list.pop_back();
    }

    size_t                                 m_max_size;  // 最大缓存容量
    list_type                              m_lru_list;  // LRU 链表（头部=最近使用，尾部=最久未使用）
    std::unordered_map<Key, list_iterator> m_cache_map; // 哈希表：Key -> 链表迭代器
    eviction_callback                      m_on_evict;  // 淘汰回调函数
};

} // namespace mc::algorithm

#endif // MC_ALGORITHM_LRU_CACHE_H
