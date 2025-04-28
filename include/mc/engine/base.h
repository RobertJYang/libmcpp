/**
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

#ifndef MC_ENGINE_BASE_H
#define MC_ENGINE_BASE_H

#include <mc/db/object.h>
#include <mc/db/table.h>
#include <mc/dbus/message.h>
#include <mc/engine/call_info.h>
#include <mc/engine/call_stack.h>
#include <mc/engine/error.h>
#include <mc/engine/macro.h>
#include <mc/engine/utils.h>
#include <mc/im/ref_ptr.h>
#include <mc/reflect.h>
#include <mc/signal_slot.h>
#include <mc/traits.h>
#include <mc/variant.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace mc::engine {
using io_context_type = boost::asio::io_context;
using strand_type     = boost::asio::strand<boost::asio::io_context::executor_type>;
using slot_type       = std::function<mc::variant(const mc::variants&)>;
using message         = mc::dbus::message;

class object_base;
class service;

using dbus_call_stack   = detail::call_stack<service, dbus_call_info>;
using object_call_stack = detail::call_stack<service, object_base>;

inline dbus_call_info* top_dbus_call_info() {
    return dbus_call_stack::top();
}

inline object_base* top_object() {
    return object_call_stack::top();
}

struct interface_base {
    virtual ~interface_base() = default;

    virtual void         set_object(object_base* obj) = 0;
    virtual object_base* get_object() const           = 0;

    virtual std::string_view    get_interface_name() const                                     = 0;
    virtual mc::connection_type connect(std::string_view signal_name, slot_type slot)          = 0;
    virtual mc::variant         emit(std::string_view signal_name, const mc::variants& args)   = 0;
    virtual mc::variant         invoke(std::string_view method_name, const mc::variants& args) = 0;
    virtual mc::variant         get_property(std::string_view property_name)                   = 0;
    virtual mc::dict            get_all_properties()                                           = 0;
    virtual bool set_property(std::string_view property_name, const mc::variant& value)        = 0;
};

class object_base {
public:
    using childrens_type = std::map<std::string_view, object_base*>;

    virtual ~object_base() = default;

    virtual void ref()   = 0;
    virtual void unref() = 0;

    virtual void                  set_service(service& s)        = 0;
    virtual service*              get_service() const            = 0;
    virtual void                  set_parent(object_base* obj)   = 0;
    virtual object_base*          get_parent() const             = 0;
    virtual void                  add_child(object_base* obj)    = 0;
    virtual void                  remove_child(object_base* obj) = 0;
    virtual const childrens_type& get_childrens() const          = 0;
    virtual void                  introspect(std::string& xml)   = 0;

    virtual const std::string& get_object_name() const                = 0;
    virtual void               set_object_name(std::string_view name) = 0;
    virtual const std::string& get_object_path() const                = 0;
    virtual void               set_object_path(std::string_view path) = 0;

    virtual bool            has_interface(std::string_view interface_name) const = 0;
    virtual interface_base* get_interface(std::string_view interface_name)       = 0;

    virtual mc::variant get_property(std::string_view property_name,
                                     std::string_view interface_name = {})  = 0;
    virtual mc::dict    get_all_properties(std::string_view interface_name) = 0;
    virtual bool        set_property(std::string_view property_name, const mc::variant& value,
                                     std::string_view interface_name = {})  = 0;

    virtual mc::variant invoke(std::string_view method_name, const mc::variants& args,
                               std::string_view interface_name = {}) = 0;

    virtual mc::connection_type connect(std::string_view signal_name, slot_type slot,
                                        std::string_view interface_name = {}) = 0;
    virtual mc::variant         emit(std::string_view signal_name, const mc::variants& args,
                                     std::string_view interface_name = {})    = 0;
};

struct object_wrap : public mc::db::object<object_wrap> {
    object_wrap(mc::engine::object_base* object) : m_object(object) {
        m_object->ref();
    }

    ~object_wrap() override {
        free();
    }

    void free() {
        if (m_object) {
            m_object->unref();
            m_object = nullptr;
        }
    }

    object_wrap(const object_wrap& other)
        : mc::db::object<object_wrap>(), m_object(other.m_object) {
        if (m_object) {
            m_object->ref();
        }
    }

    object_wrap& operator=(const object_wrap& other) {
        if (this != &other && m_object != other.m_object) {
            free();
            m_object = other.m_object;
            if (m_object) {
                m_object->ref();
            }
        }

        return *this;
    }

    object_wrap(object_wrap&& other) noexcept {
        m_object       = other.m_object;
        other.m_object = nullptr;
    }

    object_wrap& operator=(object_wrap&& other) noexcept {
        if (this != &other) {
            free();

            m_object       = other.m_object;
            other.m_object = nullptr;
        }

        return *this;
    }

    const std::string& get_path() const {
        return m_object->get_object_path();
    }

    operator mc::engine::object_base&() const {
        return *m_object;
    }

    operator mc::engine::object_base*() const {
        return m_object;
    }

    mc::engine::object_base& operator*() const {
        return *m_object;
    }

    mc::engine::object_base* operator->() const {
        return m_object;
    }

    mc::engine::object_base* m_object;
};

using object_ptr = mc::im::ref_ptr<object_base>;
struct by_path : mc::db::tag_base {};
struct by_object_name : mc::db::tag_base {};
using path_index = mc::db::ordered_non_unique<
    mc::db::member<object_wrap, const std::string&, &object_wrap::get_path>, by_path>;

} // namespace mc::engine

#endif // MC_ENGINE_BASE_H
