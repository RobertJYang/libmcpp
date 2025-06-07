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

#include "include/connection_manager.h"

#include <mc/core/object.h>
#include <mc/core/service.h>
#include <mc/exception.h>

#include <algorithm>

namespace mc::core {

class object_impl {
public:
    object_impl() = default;

    object_impl(const object_impl& other);
    object_impl& operator=(const object_impl& other);

    object_impl(object_impl&& other)            = delete;
    object_impl& operator=(object_impl&& other) = delete;

    object* find_child(std::string_view name) const;
    void    add_child(object* child);
    void    remove_child(object* child);

    std::string        m_name;
    child_list         m_children;
    connection_manager m_connection_manager;

    uint32_t is_deleted : 1;
};

object_impl::object_impl(const object_impl& other)
    : m_name(other.m_name), m_children(other.m_children) {
}

object_impl& object_impl::operator=(const object_impl& other) {
    if (this != &other) {
        m_name     = other.m_name;
        m_children = other.m_children;
    }

    return *this;
}

object* object_impl::find_child(std::string_view name) const {
    auto it = std::find_if(m_children.begin(), m_children.end(), [&name](const object_ptr& child) {
        return child->m_impl->m_name == name;
    });

    if (it != m_children.end()) {
        return it->get();
    }

    return nullptr;
}

void object_impl::add_child(object* child) {
    if (!child || is_deleted) {
        return;
    }

    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it == m_children.end()) {
        m_children.emplace_back(child);
    }
}

void object_impl::remove_child(object* child) {
    if (!child) {
        return;
    }

    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        m_children.erase(it);
    }
}

/* ------------------------ object ----------------------- */
object::object() {
}

object::object(object* parent) : m_impl(std::make_unique<object_impl>()), m_parent(parent) {
    if (parent) {
        parent->add_child(this);
        m_service = parent->get_service();
    }
}

object::~object() noexcept {
    if (!m_impl) {
        return;
    }

    m_impl->is_deleted = true;

    if (m_parent) {
        m_parent->remove_child(this);
    }

    m_impl->m_children.clear();
    m_impl->m_connection_manager.clear();

    m_impl.reset();
}

object::object(const object& other) : object_base(other) {
    if (!other.m_impl) {
        return;
    }

    m_impl = std::make_unique<object_impl>(*other.m_impl);
}

object& object::operator=(const object& other) {
    if (this != &other) {
        m_impl.reset();
        object_base(*this) = other;
        if (other.m_impl) {
            std::make_unique<object_impl>(*other.m_impl).swap(m_impl);
        }
    }

    return *this;
}

void object::set_parent(object* parent) {
    object* old_parent = m_parent;

    if (old_parent) {
        old_parent->remove_child(this);
        m_parent = nullptr;
    }

    if (parent) {
        parent->add_child(this);
        m_parent = parent;
    }
}

object* object::get_parent() const {
    return m_parent;
}

void object::set_name(std::string_view name) {
    ensure_impl().m_name = name;
}

std::string_view object::get_name() const {
    if (!m_impl) {
        return {};
    }

    return m_impl->m_name;
}

const child_list& object::children() const {
    return ensure_impl().m_children;
}

object* object::find_child(std::string_view name) const {
    if (!m_impl) {
        return nullptr;
    }

    return m_impl->find_child(name);
}

void object::add_child(object* child) {
    ensure_impl().add_child(child);
}

void object::remove_child(object* child) {
    if (!m_impl) {
        return;
    }

    m_impl->remove_child(child);
}

service_base* object::get_service() const {
    if (!m_service && m_parent) {
        auto* service = m_parent->get_service();
        if (service) {
            m_service = service;
        }
    }

    return m_service;
}

void object::set_service(service_base* s) {
    m_service = s;
}

connection_id_type object::add_connection(signal_type sig, mc::connection_type conn,
                                          connection_id_type id) {
    return ensure_impl().m_connection_manager.add_connection(sig, std::move(conn), id);
}

void object::disconnect(connection_id_type id) const {
    if (!m_impl) {
        return;
    }

    m_impl->m_connection_manager.remove_connection(id);
}

void object::disconnect_all(signal_type sig) {
    if (!m_impl) {
        return;
    }

    m_impl->m_connection_manager.remove_connections(&sig);
}

object_base::executor_type& object::get_executor() const {
    auto* service = get_service();
    MC_ASSERT(service, "get executor not available on object with no service");
    return service->get_executor();
}

object_impl& object::ensure_impl() const {
    if (!m_impl) {
        m_impl = std::make_unique<object_impl>();
    }

    return *m_impl;
}

} // namespace mc::core