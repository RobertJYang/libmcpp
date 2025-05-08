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

#include <mc/core/object.h>
#include <mc/db/object.h>
#include <mc/db/table.h>
#include <mc/dbus/message.h>
#include <mc/engine/call_stack.h>
#include <mc/engine/context.h>
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
using io_context_type    = boost::asio::io_context;
using strand_type        = boost::asio::strand<boost::asio::io_context::executor_type>;
using slot_type          = std::function<mc::variant(const mc::variants&)>;
using message            = mc::dbus::message;
using object_base        = mc::db::object_base;
using connection_id_type = mc::core::connection_id_type;

class abstract_object;
class abstract_interface;
class service;
class property_base;

using property_changed_signal = mc::signal<void(const mc::variant&, const property_base&)>;

using object_call_stack = detail::call_stack<service, abstract_object>;
inline abstract_object* get_object() {
    return object_call_stack::top_value();
}

struct invoke_result : public mc::variant {
    const mc::reflect::method_type_info* method{nullptr};

    invoke_result() = default;
    invoke_result(const mc::reflect::method_type_info* m, mc::variant v)
        : mc::variant(std::move(v)), method(m) {
    }

    bool is_valid() const {
        return method != nullptr;
    }
};

struct visitor {
    virtual void handle_interface_begin(abstract_object& obj, abstract_interface& iface) = 0;
    virtual void handle_interface_end(abstract_object& obj, abstract_interface& iface)   = 0;

    struct property_meta {
        std::string_view name;
        std::string_view signature;
        uint32_t         access;
    };
    virtual void handle(abstract_object& obj, abstract_interface& iface, property_meta& info) = 0;

    struct method_meta {
        std::string_view name;
        std::string_view args_signature;
        std::string_view return_signature;
    };
    virtual void handle(abstract_object& obj, abstract_interface& iface, method_meta& info) = 0;

    struct signal_meta {
        std::string_view name;
        std::string_view args_signature;
        std::string_view return_signature;
    };

    virtual void handle(abstract_object& obj, abstract_interface& iface, signal_meta& info) = 0;
};

class property_base {
public:
    virtual std::string_view get_name() const      = 0;
    virtual std::string_view get_signature() const = 0;
    virtual uint32_t         get_access() const    = 0;
    virtual mc::variant      get_value() const     = 0;

    virtual abstract_interface* get_interface() const = 0;
    virtual abstract_object*    get_object() const    = 0;

    virtual void set_value(const mc::variant& value) = 0;

    virtual property_changed_signal& property_changed() = 0;
};

class abstract_interface {
public:
    virtual ~abstract_interface() = default;

    virtual abstract_object*  get_object() const = 0;
    virtual mc::core::object* get_owner() const  = 0;

    virtual std::string_view    get_interface_name() const                                   = 0;
    virtual mc::connection_type connect(std::string_view signal_name, slot_type slot)        = 0;
    virtual mc::variant         emit(std::string_view signal_name, const mc::variants& args) = 0;
    virtual mc::variant         get_property(std::string_view property_name)                 = 0;
    virtual std::string_view    get_property_name(const property_base* prop)                 = 0;
    virtual property_base*      get_property_base(std::string_view property_name)            = 0;
    virtual mc::dict            get_all_properties()                                         = 0;
    virtual bool set_property(std::string_view property_name, const mc::variant& value)      = 0;

    virtual invoke_result invoke(std::string_view method_name, const mc::variants& args) = 0;

    virtual void notify_property_changed(const mc::variant& value, const property_base& prop) = 0;
    virtual property_changed_signal& property_changed()                                       = 0;
};

class abstract_object : virtual public object_base {
public:
    using managed_objects = std::map<std::string_view, abstract_object*>;

    virtual ~abstract_object() = default;

    virtual void     set_service(service& s) = 0;
    virtual service* get_service() const     = 0;

    virtual mc::core::object* get_owner() const = 0;

    virtual const managed_objects& get_managed_objects() const                 = 0;
    virtual void                   add_managed_object(abstract_object* obj)    = 0;
    virtual void                   remove_managed_object(abstract_object* obj) = 0;

    virtual std::string_view get_object_name() const                = 0;
    virtual void             set_object_name(std::string_view name) = 0;
    virtual std::string_view get_object_path() const                = 0;
    virtual void             set_object_path(std::string_view path) = 0;

    virtual bool                has_interface(std::string_view interface_name) const = 0;
    virtual abstract_interface* get_interface(std::string_view interface_name)       = 0;

    virtual mc::variant    get_property(std::string_view property_name,
                                        std::string_view interface_name = {})      = 0;
    virtual property_base* get_property_base(std::string_view property_name,
                                             std::string_view interface_name = {}) = 0;
    virtual mc::dict       get_all_properties(std::string_view interface_name)     = 0;
    virtual bool           set_property(std::string_view property_name, const mc::variant& value,
                                        std::string_view interface_name = {})      = 0;
    virtual mc::connection_type connect(std::string_view signal_name, slot_type slot,
                                        std::string_view interface_name = {})      = 0;
    virtual mc::variant         emit(std::string_view signal_name, const mc::variants& args,
                                     std::string_view interface_name = {})         = 0;

    virtual void visit(visitor& v) = 0;

    virtual invoke_result invoke(std::string_view method_name, const mc::variants& args,
                                 std::string_view interface_name = {}) = 0;

    virtual void notify_property_changed(const mc::variant& value, const property_base& prop) = 0;
    virtual property_changed_signal& property_changed()                                       = 0;
};

using object_ptr = mc::im::ref_ptr<abstract_object>;
MC_FIELD_INDEX_TAG(by_path, "path");
using path_index = mc::db::ordered_non_unique<&abstract_object::get_object_path, by_path::tag>;
using path_unique_index = mc::db::ordered_unique<&abstract_object::get_object_path, by_path::tag>;
} // namespace mc::engine

namespace mc {

inline void to_variant(const mc::engine::invoke_result& obj, mc::variant& v) {
    v = mc::variant(obj);
}

inline void from_variant(const mc::variant& v, mc::engine::invoke_result& obj) {
    static_cast<mc::variant&>(obj) = v;
}

} // namespace mc

MC_REFLECT(mc::engine::abstract_object, ((get_object_path, "path")))

#endif // MC_ENGINE_BASE_H
