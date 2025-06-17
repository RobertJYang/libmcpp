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

#include <mc/engine/object.h>
#include <mc/engine/path.h>
#include <mc/engine/utils.h>

namespace mc::engine {

object_impl::object_impl(core_object* parent) : abstract_object(parent) {
}

object_impl::~object_impl() {
    m_object_path.clear();
    m_position.clear();
    m_managed_objects.clear();
    m_property_changed_signal.reset();
}

const abstract_object::managed_objects& object_impl::get_managed_objects() const {
    return m_managed_objects;
}

void object_impl::add_managed_object(abstract_object* obj) {
    if (!obj || obj == this) {
        return;
    }

    auto old_owner = obj->get_owner();
    if (old_owner == this) {
        return;
    }

    if (old_owner != nullptr) {
        old_owner->remove_managed_object(obj);
    }

    // 检查当前的子节点中，是否有节点应该成为新对象的子节点
    auto path = obj->get_object_path();
    for (auto it = m_managed_objects.begin(); it != m_managed_objects.end();) {
        auto c_path = it->first;
        auto c_obj  = static_cast<object_impl*>(it->second);
        if (c_obj == obj) {
            it = m_managed_objects.erase(it);
            continue;
        }

        if (detail::path_starts_with(c_path, path)) {
            it = m_managed_objects.erase(it);
            c_obj->set_owner(obj);
        } else {
            ++it;
        }
    }

    m_managed_objects[path] = obj;
}

void object_impl::remove_managed_object(abstract_object* obj) {
    if (obj->get_owner() != this) {
        return;
    }

    // 将 obj 从管理列表中移除
    auto it = m_managed_objects.find(obj->get_object_path());
    if (it != m_managed_objects.end()) {
        m_managed_objects.erase(it);
    }
}

void object_impl::notify_property_changed(const mc::variant& value, const property_base& prop) {
    if (m_property_changed_signal) {
        (*m_property_changed_signal)(value, prop);
    }
}

property_changed_signal& object_impl::property_changed() {
    if (m_property_changed_signal == nullptr) {
        m_property_changed_signal = std::make_unique<property_changed_signal>();
    }

    return *m_property_changed_signal;
}

abstract_object* object_impl::get_owner() const {
    return m_owner;
}

void object_impl::set_owner(abstract_object* new_owner) {
    if (m_owner == new_owner) {
        return;
    }

    // 将当前对象从旧的 owner 中移除
    if (m_owner != nullptr) {
        m_owner->remove_managed_object(this);
        if (!new_owner) {
            // 如果没有新的 owner，那需要将当前对象的子对象添加到其 owner 的管理列表中
            auto& child_objects = get_managed_objects();
            while (!child_objects.empty()) {
                child_objects.begin()->second->set_owner(m_owner);
            }
            m_owner = nullptr;
            return;
        }
    }

    // 将当前对象添加到新的 owner 中
    if (new_owner != nullptr) {
        new_owner->add_managed_object(this);
    }
    m_owner = new_owner;
}

std::string_view object_impl::get_object_name() const {
    // 由于 get_name() 现在返回 std::string，我们需要缓存结果
    static thread_local std::string cached_name;
    cached_name = this->get_name();
    return cached_name;
}

void object_impl::set_object_name(std::string_view name) {
    this->set_name(name);
}

void object_impl::set_object_path(std::string_view path) {
    // 去除尾部多余的 '/'
    while (path.size() > 1 && path.back() == '/') {
        path = path.substr(0, path.size() - 1);
    }

    if (!path.empty() && !mc::engine::path::is_valid(path)) {
        MC_THROW(mc::invalid_arg_exception, "invalid object path ${path}", ("path", path));
    }

    // 如果已经注册到服务中则不能修改路径
    MC_ASSERT_THROW(m_object_path.empty() || !get_service(), mc::invalid_arg_exception,
                    "object ”${path}“ is registered, please unregister first",
                    ("path", m_object_path));

    m_object_path = path;
}

std::string_view object_impl::get_position() const {
    return m_position;
}

void object_impl::set_position(std::string_view position) {
    m_position = position;
}

void object_impl::set_service(service* s) {
    m_service = s;
}

service* object_impl::get_service() const {
    return m_service;
}

} // namespace mc::engine
