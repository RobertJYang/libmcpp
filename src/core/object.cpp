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

namespace mc {
namespace core {

object::object(std::string name, object* parent) : m_name(std::move(name)), m_parent(nullptr) {
    if (parent) {
        set_parent(parent);
    }
}

object::~object() {
    // 首先从父对象中移除自己
    if (m_parent) {
        m_parent->remove_child(this);
    }

    // 删除所有子对象
    // 注意：这里需要创建一个副本，因为在删除过程中m_children会被修改
    auto children_copy = m_children;
    for (auto* child : children_copy) {
        delete child;
    }

    // 确保m_children为空
    m_children.clear();
}

void object::set_parent(object* parent) {
    if (m_parent == parent) {
        return;
    }

    // 从原父对象中移除
    if (m_parent) {
        m_parent->remove_child(this);
    }

    // 设置新父对象
    m_parent = parent;

    // 添加到新父对象的子对象列表
    if (m_parent) {
        m_parent->add_child(this);
    }
}

object* object::find_child(const std::string& name) const {
    for (auto* child : m_children) {
        if (child->name() == name) {
            return child;
        }
    }

    return nullptr;
}

object* object::find_child_recursive(const std::string& name) const {
    // 首先在直接子对象中查找
    auto* child = find_child(name);
    if (child) {
        return child;
    }

    // 然后在子对象的子对象中递归查找
    for (auto* child : m_children) {
        auto* found = child->find_child_recursive(name);
        if (found) {
            return found;
        }
    }

    return nullptr;
}

void object::add_child(object* child) {
    if (!child) {
        return;
    }

    // 检查是否已经是子对象
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        return;
    }

    // 添加到子对象列表
    m_children.push_back(child);
}

void object::remove_child(object* child) {
    if (!child) {
        return;
    }

    // 从子对象列表中移除
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        m_children.erase(it);
    }
}

} // namespace core
} // namespace mc 