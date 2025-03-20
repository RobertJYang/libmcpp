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

/**
 * @file shared_tree_node.cpp
 * @brief 共享内存树节点实现
 */

#include "mc/database/shared_tree_db.h"
#include <algorithm>
#include <functional>

namespace mc {
namespace database {

// 创建根节点
shared_tree_node_ptr shared_tree_node::create_root(shared_memory_allocator& allocator)
{
    return std::make_shared<shared_tree_node>(allocator, "", variant());
}

// 创建常规节点
shared_tree_node_ptr shared_tree_node::create(shared_memory_allocator& allocator, 
                                            std::string name, variant data)
{
    return std::make_shared<shared_tree_node>(allocator, std::move(name), std::move(data));
}

// 构造函数
shared_tree_node::shared_tree_node(shared_memory_allocator& allocator, std::string name, variant data)
    : m_allocator(allocator)
    , m_name(std::move(name))
    , m_data(std::move(data))
    , m_hash_cache(0)
{
}

// 析构函数
shared_tree_node::~shared_tree_node()
{
    // 清空子节点
    clear_children();
}

// 获取节点名称
const std::string& shared_tree_node::get_name() const
{
    return m_name;
}

// 获取节点数据
const variant& shared_tree_node::get_data() const
{
    return m_data;
}

// 设置节点数据
void shared_tree_node::set_data(const variant& data)
{
    m_data = data;
    m_hash_cache = 0; // 重置哈希缓存
}

// 获取子节点
shared_tree_node_ptr shared_tree_node::get_child(std::string_view name) const
{
    auto it = m_children.find(std::string(name));
    if (it != m_children.end()) {
        return it->second;
    }
    return nullptr;
}

// 获取所有子节点
std::vector<shared_tree_node_ptr> shared_tree_node::get_children() const
{
    std::vector<shared_tree_node_ptr> children;
    children.reserve(m_children.size());
    
    for (const auto& pair : m_children) {
        children.push_back(pair.second);
    }
    
    return children;
}

// 获取子节点数量
size_t shared_tree_node::get_child_count() const
{
    return m_children.size();
}

// 添加子节点
bool shared_tree_node::add_child(std::string_view name, shared_tree_node_ptr node)
{
    if (!node) {
        return false;
    }
    
    std::string name_str(name);
    if (m_children.find(name_str) != m_children.end()) {
        return false; // 子节点已存在
    }
    
    m_children[name_str] = node;
    m_hash_cache = 0; // 重置哈希缓存
    return true;
}

// 替换子节点
bool shared_tree_node::replace_child(std::string_view name, shared_tree_node_ptr node)
{
    if (!node) {
        return false;
    }
    
    std::string name_str(name);
    if (m_children.find(name_str) == m_children.end()) {
        return false; // 子节点不存在
    }
    
    m_children[name_str] = node;
    m_hash_cache = 0; // 重置哈希缓存
    return true;
}

// 删除子节点
bool shared_tree_node::remove_child(std::string_view name)
{
    std::string name_str(name);
    auto it = m_children.find(name_str);
    if (it == m_children.end()) {
        return false; // 子节点不存在
    }
    
    m_children.erase(it);
    m_hash_cache = 0; // 重置哈希缓存
    return true;
}

// 清空所有子节点
void shared_tree_node::clear_children()
{
    m_children.clear();
    m_hash_cache = 0; // 重置哈希缓存
}

// 克隆节点（深拷贝）
shared_tree_node_ptr shared_tree_node::clone(shared_memory_allocator& allocator) const
{
    // 创建新节点，复制名称和数据
    auto new_node = create(allocator, m_name, m_data);
    
    // 递归克隆所有子节点
    for (const auto& pair : m_children) {
        auto child_clone = pair.second->clone(allocator);
        new_node->add_child(pair.first, child_clone);
    }
    
    return new_node;
}

// 计算节点哈希值
size_t shared_tree_node::hash() const
{
    // 如果已缓存哈希值，直接返回
    if (m_hash_cache) {
        return m_hash_cache;
    }
    
    // 计算哈希值
    size_t hash_value = std::hash<std::string>{}(m_name);
    
    // 结合数据的哈希值
    if (!m_data.is_null()) {
        // 简单使用数据字符串表示的哈希值
        std::string data_str = m_data.to_string();
        hash_value ^= std::hash<std::string>{}(data_str) + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
    }
    
    // 结合子节点的哈希值
    for (const auto& pair : m_children) {
        const std::string& child_name = pair.first;
        const shared_tree_node_ptr& child = pair.second;
        
        size_t child_hash = std::hash<std::string>{}(child_name);
        if (child) {
            child_hash ^= child->hash() + 0x9e3779b9 + (child_hash << 6) + (child_hash >> 2);
        }
        
        hash_value ^= child_hash + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
    }
    
    // 缓存并返回哈希值
    m_hash_cache = hash_value;
    return hash_value;
}

} // namespace database
} // namespace mc 