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

namespace mc::engine {

object_impl::object_impl() {
}

object_impl::~object_impl() {
}

service* object_impl::get_service() const {
    return m_service;
}

void object_impl::set_service(service& s) {
    m_service = &s;
}

void object_impl::set_parent(abstract_object* obj) {
    m_parent = obj;
}

abstract_object* object_impl::get_parent() const {
    return m_parent;
}

void object_impl::add_child(abstract_object* obj) {
    obj->set_parent(this);
    ms_children[obj->get_object_path()] = obj;
}

void object_impl::remove_child(abstract_object* obj) {
    ms_children.erase(obj->get_object_path());
}

const abstract_object::childrens_type& object_impl::get_childrens() const {
    return ms_children;
}

const std::string& object_impl::get_object_name() const {
    return m_object_name;
}

void object_impl::set_object_name(std::string_view name) {
    m_object_name = name;
}

const std::string& object_impl::get_object_path() const {
    if (m_object_path.empty()) {
        if (get_parent()) {
            m_object_path = get_parent()->get_object_path();
        }
        m_object_path += object_type::path_pattern;
    }
    return m_object_path;
}

void object_impl::set_object_path(std::string_view path) {
    m_object_path = path;
}

timer object_impl::new_timer(mc::milliseconds interval, std::function<void(error_code)> callback) {
    MC_ASSERT(m_service, "current object ${path} is not registered to service, cannot create timer",
              ("path", get_object_path()));

    timer t(m_service->get_strand());
    t.expires_after(interval);
    t.async_wait(callback);
    return t;
}

} // namespace mc::engine
