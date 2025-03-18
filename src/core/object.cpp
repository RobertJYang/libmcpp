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

#include "mc/core/object.h"
#include "mc/core/event.h"
#include <algorithm>

namespace mc {
namespace core {

object::object(std::string name, io_context_type& io_context, std::shared_ptr<object> parent)
    : m_name(std::move(name))
    , m_deleting(false)
    , m_io_context(io_context)
    , m_strand(boost::asio::make_strand(io_context)) {
    
    if (parent) {
        set_parent(parent);
    }
}

object::~object() noexcept {
    m_deleting = true;
    
    // 断开与父对象的连接
    set_parent(nullptr);
    
    // 删除所有子对象
    std::vector<std::shared_ptr<object>> children_to_delete;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        children_to_delete = m_children;
        m_children.clear();
    }
    
    // 释放所有子对象
    for (auto& child : children_to_delete) {
        child->set_parent(nullptr);
    }
}

void object::set_parent(std::shared_ptr<object> parent) {
    if (m_deleting) {
        return;
    }
    
    std::shared_ptr<object> old_parent;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        old_parent = m_parent.lock();
        m_parent = parent;
    }
    
    // 从旧父对象的子对象列表中移除
    if (old_parent) {
        old_parent->remove_child(shared_from_this());
    }
    
    // 添加到新父对象的子对象列表
    if (parent) {
        parent->add_child(shared_from_this());
    }
}

std::shared_ptr<object> object::find_child(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_children.begin(), m_children.end(),
                         [&name](const std::shared_ptr<object>& child) {
                             return child->name() == name;
                         });
    
    if (it != m_children.end()) {
        return *it;
    }
    
    return nullptr;
}

std::shared_ptr<object> object::find_child_recursive(const std::string& name) const {
    // 先查找直接子对象
    auto child = find_child(name);
    if (child) {
        return child;
    }
    
    // 然后递归查找子对象的子对象
    std::vector<std::shared_ptr<object>> children_copy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        children_copy = m_children;
    }
    
    for (const auto& c : children_copy) {
        auto result = c->find_child_recursive(name);
        if (result) {
            return result;
        }
    }
    
    return nullptr;
}

bool object::post_event(std::shared_ptr<event> e) {
    if (!e || m_deleting) {
        return false;
    }
    
    // 设置目标对象
    e->set_target(shared_from_this());
    
    // 在对象的执行器上异步处理事件
    post([this, e]() {
        this->handle_event(e);
    });
    
    return true;
}

bool object::handle_event(std::shared_ptr<event> e) {
    if (!e || m_deleting) {
        return false;
    }
    
    // 应用事件过滤器
    if (filter_event(shared_from_this(), e)) {
        return true;
    }
    
    // 在这里可以添加事件处理逻辑
    // 默认实现什么都不做
    
    return false;
}

void object::install_event_filter(std::shared_ptr<object> filter) {
    if (!filter || m_deleting) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 检查过滤器是否已存在
    auto it = std::find(m_event_filters.begin(), m_event_filters.end(), filter);
    if (it == m_event_filters.end()) {
        m_event_filters.push_back(filter);
    }
}

void object::remove_event_filter(std::shared_ptr<object> filter) {
    if (!filter) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find(m_event_filters.begin(), m_event_filters.end(), filter);
    if (it != m_event_filters.end()) {
        m_event_filters.erase(it);
    }
}

bool object::filter_event(std::shared_ptr<object> target, std::shared_ptr<event> e) {
    if (!target || !e) {
        return false;
    }
    
    // 获取过滤器列表的副本
    auto filters = event_filters();
    
    // 应用所有过滤器
    for (auto& filter : filters) {
        if (filter->filter_event(target, e)) {
            return true;
        }
    }
    
    return false;
}

void object::add_child(std::shared_ptr<object> child) {
    if (!child || m_deleting) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 检查子对象是否已存在
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it == m_children.end()) {
        m_children.push_back(child);
    }
}

void object::remove_child(std::shared_ptr<object> child) {
    if (!child) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        m_children.erase(it);
    }
}

} // namespace core
} // namespace mc 