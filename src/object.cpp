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

#include "mc/object.h"

namespace mc {

object::object(std::string name, object* parent) : name_(std::move(name)), parent_(nullptr) {
    if (parent) {
        set_parent(parent);
    }
}

object::~object() {
    // 首先从父对象中移除自己
    if (parent_) {
        parent_->remove_child(this);
    }

    // 删除所有子对象
    // 注意：这里需要创建一个副本，因为在删除过程中children_会被修改
    auto children_copy = children_;
    for (auto* child : children_copy) {
        delete child;
    }

    // 确保children_为空
    children_.clear();
}

void object::set_parent(object* parent) {
    if (parent_ == parent) {
        return;
    }

    // 从原父对象中移除
    if (parent_) {
        parent_->remove_child(this);
    }

    // 设置新父对象
    parent_ = parent;

    // 添加到新父对象的子对象列表
    if (parent_) {
        parent_->add_child(this);
    }
}

object* object::find_child(const std::string& name) const {
    for (auto* child : children_) {
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
    for (auto* child : children_) {
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
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        return;
    }

    // 添加到子对象列表
    children_.push_back(child);
}

void object::remove_child(object* child) {
    if (!child) {
        return;
    }

    // 从子对象列表中移除
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        children_.erase(it);
    }
}

} // namespace mc