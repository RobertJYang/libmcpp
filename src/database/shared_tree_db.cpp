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
 * @file shared_tree_db.cpp
 * @brief 基于共享内存的树结构数据库实现
 */

#include "mc/database/shared_tree_db.h"
#include "mc/database/path_iterator.h"
#include "mc/log.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <stack>
#include <stdexcept>

namespace mc {
namespace database {

//=============================================================================
// shared_tree_node 实现
//=============================================================================

shared_tree_node::shared_tree_node(shared_memory_allocator& allocator, intrusive_tree_node* node)
    : m_allocator(allocator), m_node(node) {
}

shared_tree_node_ptr shared_tree_node::create_root(shared_memory_allocator& allocator) {
    // 分配根节点
    auto node = make_shared<intrusive_tree_node>(allocator, "", variant());
    if (!node) {
        elog("无法在共享内存中创建根节点");
        return nullptr;
    }
    
    // 创建共享树节点
    return make_shared<shared_tree_node>(allocator, allocator, node.get());
}

shared_tree_node_ptr shared_tree_node::create(shared_memory_allocator& allocator, 
                                            std::string name, variant data) {
    // 分配节点
    auto node = make_shared<intrusive_tree_node>(allocator, name, data);
    if (!node) {
        elog("无法在共享内存中创建节点：${name}", ("name", name));
        return nullptr;
    }
    
    // 创建共享树节点
    return make_shared<shared_tree_node>(allocator, allocator, node.get());
}

const std::string& shared_tree_node::get_name() const {
    return m_node->get_name();
}

const variant& shared_tree_node::get_data() const {
    return m_node->get_data();
}

void shared_tree_node::set_data(const variant& data) {
    m_node->set_data(data);
}

shared_tree_node_ptr shared_tree_node::get_child(std::string_view name) const {
    auto child = m_node->get_child(name);
    if (!child) {
        return nullptr;
    }
    
    // 创建共享树节点
    return make_shared<shared_tree_node>(m_allocator, m_allocator, child);
}

std::vector<shared_tree_node_ptr> shared_tree_node::get_children() const {
    auto& children = m_node->get_children();
    std::vector<shared_tree_node_ptr> result;
    result.reserve(children.size());
    
    for (auto& child : children) {
        auto node = make_shared<shared_tree_node>(m_allocator, m_allocator, &child);
        if (node) {
            result.push_back(std::move(node));
        }
    }
    
    return result;
}

size_t shared_tree_node::get_child_count() const {
    return m_node->get_children().size();
}

bool shared_tree_node::add_child(std::string_view name, shared_tree_node_ptr node) {
    if (!node) {
        return false;
    }
    
    // 添加子节点
    return m_node->add_child(node->m_node);
}

bool shared_tree_node::replace_child(std::string_view name, shared_tree_node_ptr node) {
    if (!node) {
        return false;
    }
    
    // 先移除旧节点
    m_node->remove_child(name);
    
    // 添加新节点
    return m_node->add_child(node->m_node);
}

bool shared_tree_node::remove_child(std::string_view name) {
    return m_node->remove_child(name);
}

intrusive_tree_node* shared_tree_node::get_internal_node() {
    return m_node;
}

const intrusive_tree_node* shared_tree_node::get_internal_node() const {
    return m_node;
}

//=============================================================================
// shared_tree_db 实现
//=============================================================================

// 创建或打开数据库
shared_tree_db_ptr shared_tree_db::create(const std::string& name, size_t size)
{
    // 创建共享内存
    auto shm = std::make_shared<shared_memory>(name, size);
    auto& allocator = shm->get_allocator();
    
    // 创建根节点
    auto root = shared_tree_node::create_root(allocator);
    
    // 创建数据库对象
    return std::make_shared<shared_tree_db>(shm, root);
}

// 构造函数
shared_tree_db::shared_tree_db(std::shared_ptr<shared_memory> shm, shared_tree_node_ptr root)
    : m_shm(shm)
    , m_root(root)
{
}

// 析构函数
shared_tree_db::~shared_tree_db()
{
    // 会自动释放共享内存和根节点
}

// 获取节点
shared_tree_node_ptr shared_tree_db::get_node(std::string_view path) const
{
    std::shared_lock<shared_mutex> lock(m_mutex);
    return find_node(m_root, path);
}

// 获取节点数据
variant shared_tree_db::get_data(std::string_view path) const
{
    std::shared_lock<shared_mutex> lock(m_mutex);
    auto node = find_node(m_root, path);
    if (node) {
        return node->get_data();
    }
    return variant();
}

// 检查节点是否存在
bool shared_tree_db::has_node(std::string_view path) const
{
    std::shared_lock<shared_mutex> lock(m_mutex);
    return find_node(m_root, path) != nullptr;
}

// 设置节点数据
bool shared_tree_db::set_data(std::string_view path, const variant& data)
{
    std::unique_lock<shared_mutex> lock(m_mutex);
    auto node = find_node(m_root, path);
    if (node) {
        node->set_data(data);
        return true;
    }
    return false;
}

// 创建节点
bool shared_tree_db::create_node(std::string_view path, const variant& data)
{
    if (path.empty()) {
        return false;
    }
    
    std::unique_lock<shared_mutex> lock(m_mutex);
    
    // 处理根节点情况
    if (path == "/") {
        m_root->set_data(data);
        return true;
    }
    
    // 解析路径
    auto [parent_path, node_name] = split_path(path);
    
    // 获取父节点
    auto parent = find_node(m_root, parent_path);
    if (!parent) {
        return false;
    }
    
    // 检查节点是否已存在
    if (parent->get_child(node_name)) {
        return false;
    }
    
    // 创建新节点
    return create_node_internal(parent, node_name, data);
}

// 删除节点
bool shared_tree_db::delete_node(std::string_view path)
{
    if (path.empty() || path == "/") {
        return false; // 不能删除根节点
    }
    
    std::unique_lock<shared_mutex> lock(m_mutex);
    
    // 解析路径
    auto [parent_path, node_name] = split_path(path);
    
    // 获取父节点
    auto parent = find_node(m_root, parent_path);
    if (!parent) {
        return false;
    }
    
    // 删除节点
    return parent->remove_child(node_name);
}

// 遍历所有节点
void shared_tree_db::traverse(std::function<void(std::string_view, const variant&)> callback) const
{
    if (!callback) {
        return;
    }
    
    std::shared_lock<shared_mutex> lock(m_mutex);
    
    std::stack<std::pair<shared_tree_node_ptr, std::string>> stack;
    stack.push({m_root, "/"});
    
    while (!stack.empty()) {
        auto [node, path] = stack.top();
        stack.pop();
        
        // 调用回调函数
        callback(path, node->get_data());
        
        // 获取所有子节点
        auto children = node->get_children();
        for (const auto& child : children) {
            std::string child_path;
            if (path == "/") {
                child_path = path + child->get_name();
            } else {
                child_path = path + "/" + child->get_name();
            }
            stack.push({child, child_path});
        }
    }
}

// 创建事务
shared_tree_transaction_ptr shared_tree_db::create_transaction()
{
    return std::make_shared<shared_tree_transaction>(std::shared_ptr<shared_tree_db>(this));
}

// 导出为标准树结构（序列化为dict）
dict shared_tree_db::export_to_dict() const
{
    std::shared_lock<shared_mutex> lock(m_mutex);
    
    dict result;
    
    // 辅助函数：递归导出节点
    std::function<void(const shared_tree_node_ptr&, dict&)> export_node = 
        [&export_node](const shared_tree_node_ptr& node, dict& node_dict) {
            // 添加节点数据
            node_dict["_data"] = node->get_data();
            
            // 添加子节点
            dict children;
            for (const auto& child : node->get_children()) {
                dict child_dict;
                export_node(child, child_dict);
                children[child->get_name()] = child_dict;
            }
            
            if (!children.empty()) {
                node_dict["_children"] = children;
            }
        };
    
    export_node(m_root, result);
    return result;
}

// 从标准树结构导入（从dict反序列化）
bool shared_tree_db::import_from_dict(const dict& tree_dict)
{
    std::unique_lock<shared_mutex> lock(m_mutex);
    
    // 清空现有树
    m_root->clear_children();
    
    try {
        // 辅助函数：递归导入节点
        std::function<void(shared_tree_node_ptr&, const dict&)> import_node = 
            [this, &import_node](shared_tree_node_ptr& node, const dict& node_dict) {
                // 设置节点数据
                if (node_dict.contains("_data")) {
                    node->set_data(node_dict.at("_data"));
                }
                
                // 导入子节点
                if (node_dict.contains("_children")) {
                    const dict& children = node_dict.at("_children").as<dict>();
                    
                    for (const auto& [name, child_dict] : children) {
                        // 创建子节点
                        auto child = shared_tree_node::create(this->get_allocator(), name);
                        
                        // 递归导入子节点
                        import_node(child, child_dict.as<dict>());
                        
                        // 添加到父节点
                        node->add_child(name, child);
                    }
                }
            };
        
        import_node(m_root, tree_dict);
        return true;
    } catch (const std::exception&) {
        // 导入失败，恢复空树
        m_root->clear_children();
        m_root->set_data(variant());
        return false;
    }
}

// 获取数据库名称
std::string_view shared_tree_db::get_name() const
{
    return m_shm->get_name();
}

// 获取共享内存大小
size_t shared_tree_db::get_memory_size() const
{
    return m_shm->get_size();
}

// 获取已使用内存大小
size_t shared_tree_db::get_used_memory_size() const
{
    return m_shm->get_used_size();
}

// 获取共享内存分配器
shared_memory_allocator& shared_tree_db::get_allocator()
{
    return m_shm->get_allocator();
}

// 内部方法：查找节点
shared_tree_node_ptr shared_tree_db::find_node(shared_tree_node_ptr start_node, std::string_view path) const
{
    if (!start_node) {
        return nullptr;
    }
    
    // 处理空路径或根路径
    if (path.empty() || path == "/") {
        return m_root;
    }
    
    // 使用path_iterator高效遍历路径
    path_iterator iter(path);
    shared_tree_node_ptr current = m_root;
    
    // 遍历路径的每一段
    while (iter.to_next()) {
        std::string_view segment = iter.current();
        if (segment.empty()) {
            continue;
        }
        
        // 查找子节点
        current = current->get_child(segment);
        if (!current) {
            return nullptr; // 路径不存在
        }
    }
    
    return current;
}

// 内部方法：创建子节点
bool shared_tree_db::create_node_internal(shared_tree_node_ptr parent, std::string_view name, const variant& data)
{
    if (!parent) {
        return false;
    }
    
    // 创建新节点
    auto new_node = shared_tree_node::create(m_shm->get_allocator(), std::string(name), data);
    
    // 添加到父节点
    return parent->add_child(name, new_node);
}

// 辅助方法：解析路径为段
std::vector<std::string> shared_tree_db::parse_path(std::string_view path)
{
    std::vector<std::string> segments;
    
    // 使用path_iterator高效获取路径段
    path_iterator iter(path);
    while (iter.to_next()) {
        std::string_view segment = iter.current();
        if (!segment.empty()) {
            segments.emplace_back(segment);
        }
    }
    
    return segments;
}

// 辅助方法：拆分路径为父路径和节点名称
std::pair<std::string, std::string> shared_tree_db::split_path(std::string_view path)
{
    // 处理空路径
    if (path.empty()) {
        return {"", ""};
    }
    
    // 处理根路径
    if (path == "/") {
        return {"/", ""};
    }
    
    // 规范化路径（去除尾部斜杠）
    std::string_view norm_path = path;
    if (norm_path.back() == IMMUTABLE_PATH_SEP) {
        norm_path = norm_path.substr(0, norm_path.size() - 1);
    }
    
    // 查找最后一个斜杠
    size_t pos = norm_path.find_last_of(IMMUTABLE_PATH_SEP);
    
    // 处理不同情况
    if (pos == std::string_view::npos) {
        // 没有斜杠，在根目录下
        return {"/", std::string(norm_path)};
    } else if (pos == 0) {
        // 根目录下的节点
        return {"/", std::string(norm_path.substr(1))};
    } else {
        // 一般情况
        return {std::string(norm_path.substr(0, pos)), std::string(norm_path.substr(pos + 1))};
    }
}

// =============================================================================
// 事务实现
// =============================================================================

// 构造函数
shared_tree_transaction::shared_tree_transaction(shared_tree_db_ptr db)
    : m_db(db)
    , m_committed(false)
{
    // 获取原始树的只读锁
    std::shared_lock<shared_mutex> lock(m_db->m_mutex);
    
    // 创建快照
    m_snapshot_root = m_db->m_root;
    
    // 创建工作副本
    m_working_root = m_snapshot_root->clone(m_db->get_allocator());
}

// 析构函数
shared_tree_transaction::~shared_tree_transaction()
{
    if (!m_committed) {
        rollback();
    }
}

// 获取节点
shared_tree_node_ptr shared_tree_transaction::get_node(std::string_view path) const
{
    return find_node(m_working_root, path);
}

// 获取节点数据
variant shared_tree_transaction::get_data(std::string_view path) const
{
    auto node = find_node(m_working_root, path);
    if (node) {
        return node->get_data();
    }
    return variant();
}

// 检查节点是否存在
bool shared_tree_transaction::has_node(std::string_view path) const
{
    return find_node(m_working_root, path) != nullptr;
}

// 设置节点数据
bool shared_tree_transaction::set_data(std::string_view path, const variant& data)
{
    auto node = find_node(m_working_root, path);
    if (node) {
        node->set_data(data);
        return true;
    }
    return false;
}

// 创建节点
bool shared_tree_transaction::create_node(std::string_view path, const variant& data)
{
    if (path.empty()) {
        return false;
    }
    
    // 处理根节点情况
    if (path == "/") {
        m_working_root->set_data(data);
        return true;
    }
    
    // 解析路径
    auto [parent_path, node_name] = shared_tree_db::split_path(path);
    
    // 获取父节点
    auto parent = find_node(m_working_root, parent_path);
    if (!parent) {
        return false;
    }
    
    // 检查节点是否已存在
    if (parent->get_child(node_name)) {
        return false;
    }
    
    // 创建新节点
    return create_node_internal(parent, node_name, data);
}

// 删除节点
bool shared_tree_transaction::delete_node(std::string_view path)
{
    if (path.empty() || path == "/") {
        return false; // 不能删除根节点
    }
    
    // 解析路径
    auto [parent_path, node_name] = shared_tree_db::split_path(path);
    
    // 获取父节点
    auto parent = find_node(m_working_root, parent_path);
    if (!parent) {
        return false;
    }
    
    // 删除节点
    return parent->remove_child(node_name);
}

// 提交事务
bool shared_tree_transaction::commit()
{
    if (m_committed) {
        return false; // 已经提交过
    }
    
    // 计算修改前后的哈希值
    size_t original_hash = m_snapshot_root->hash();
    size_t new_hash = m_working_root->hash();
    
    // 如果没有变化，直接返回成功
    if (original_hash == new_hash) {
        m_committed = true;
        return true;
    }
    
    // 获取原始树的写锁
    std::unique_lock<shared_mutex> lock(m_db->m_mutex);
    
    // 检查原始树是否被修改（乐观并发控制）
    if (m_db->m_root->hash() != original_hash) {
        return false; // 并发冲突
    }
    
    // 用工作副本替换原始树
    m_db->m_root = m_working_root;
    
    m_committed = true;
    return true;
}

// 回滚事务
void shared_tree_transaction::rollback()
{
    // 丢弃工作副本
    m_working_root = nullptr;
    m_committed = true; // 标记为已处理
}

// 内部方法：查找节点
shared_tree_node_ptr shared_tree_transaction::find_node(shared_tree_node_ptr start_node, std::string_view path) const
{
    return m_db->find_node(start_node, path);
}

// 内部方法：创建子节点
bool shared_tree_transaction::create_node_internal(shared_tree_node_ptr parent, std::string_view name, const variant& data)
{
    if (!parent) {
        return false;
    }
    
    // 创建新节点
    auto new_node = shared_tree_node::create(m_db->get_allocator(), std::string(name), data);
    
    // 添加到父节点
    return parent->add_child(name, new_node);
}

} // namespace database
} // namespace mc 