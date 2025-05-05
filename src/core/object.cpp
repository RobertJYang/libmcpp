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

#include <mc/core/object.h>

#include <algorithm>

namespace mc::core {

struct object::object_impl {
    object_impl(object* parent) : m_parent(parent) {
    }

    ~object_impl();

    object* find_child(std::string_view name) const;
    void    add_child(object* child);
    void    remove_child(object* child);

    std::string m_name;
    object*     m_object{nullptr};
    object*     m_parent{nullptr};
    child_list  m_children;

    uint32_t is_deleted : 1;
};

object::object_impl::~object_impl() {
    is_deleted = true;
    m_object   = nullptr;

    if (m_parent) {
        m_parent->remove_child(m_object);
    }

    m_children.clear();
}

object::object(object* parent) : m_impl(std::make_unique<object_impl>(parent)) {
    m_impl->m_object = this;
    if (parent) {
        parent->add_child(this);
    }
}

object::~object() noexcept {
    m_impl.reset();
}

object::object(object&& other) noexcept {
    m_impl           = std::move(other.m_impl);
    m_impl->m_object = this;
}

object& object::operator=(object&& other) noexcept {
    if (this != &other) {
        m_impl           = std::move(other.m_impl);
        m_impl->m_object = this;
    }
    return *this;
}

void object::set_parent(object* parent) {
    object* old_parent = m_impl->m_parent;

    if (old_parent) {
        old_parent->remove_child(this);
        m_impl->m_parent = nullptr;
    }

    if (parent) {
        parent->add_child(this);
        m_impl->m_parent = parent;
    }
}

object* object::parent() const {
    return m_impl->m_parent;
}

void object::set_name(std::string_view name) {
    m_impl->m_name = name;
}

std::string_view object::get_name() const {
    return m_impl->m_name;
}

const child_list& object::children() const {
    return m_impl->m_children;
}

object* object::find_child(std::string_view name) const {
    return m_impl->find_child(name);
}

object* object::object_impl::find_child(std::string_view name) const {
    auto it = std::find_if(m_children.begin(), m_children.end(), [&name](const object_ptr& child) {
        return child->m_impl->m_name == name;
    });

    if (it != m_children.end()) {
        return it->get();
    }

    return nullptr;
}

void object::add_child(object* child) {
    m_impl->add_child(child);
}

void object::object_impl::add_child(object* child) {
    if (!child || is_deleted) {
        return;
    }

    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it == m_children.end()) {
        m_children.emplace_back(child);
    }
}

void object::remove_child(object* child) {
    m_impl->remove_child(child);
}

void object::object_impl::remove_child(object* child) {
    if (!child) {
        return;
    }

    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        m_children.erase(it);
    }
}

} // namespace mc::core