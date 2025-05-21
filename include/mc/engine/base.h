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
#include <mc/db/table.h>
#include <mc/dbus/message.h>
#include <mc/engine/call_stack.h>
#include <mc/engine/context.h>
#include <mc/engine/macro.h>
#include <mc/engine/service.h>
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
namespace mdb = mc::db;

using io_context_type    = boost::asio::io_context;
using strand_type        = boost::asio::strand<boost::asio::io_context::executor_type>;
using slot_type          = std::function<mc::variant(const mc::variants&)>;
using message            = mc::dbus::message;
using core_object        = mc::core::object;
using connection_id_type = mc::core::connection_id_type;

class abstract_object;
class abstract_interface;
class property_base;

using property_changed_signal = mc::signal<void(const mc::variant&, const property_base&)>;

using object_call_stack = detail::call_stack<service, abstract_object>;

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
    virtual void handle_interface_begin(const abstract_object&    obj,
                                        const abstract_interface& iface) = 0;
    virtual void handle_interface_end(const abstract_object&    obj,
                                      const abstract_interface& iface)   = 0;

    struct property_meta {
        std::string_view name;
        std::string_view signature;
        int              read_privilege;
        int              write_privilege;
        int              flags;
    };
    virtual void handle(const abstract_object& obj, const abstract_interface& iface,
                        const property_meta& info) = 0;

    struct method_meta {
        std::string_view name;
        std::string_view args_signature;
        std::string_view return_signature;
        int              privilege;
        int              flags;
    };
    virtual void handle(const abstract_object& obj, const abstract_interface& iface,
                        const method_meta& info) = 0;

    struct signal_meta {
        std::string_view name;
        std::string_view args_signature;
        std::string_view return_signature;
        int              flags;
    };

    virtual void handle(const abstract_object& obj, const abstract_interface& iface,
                        const signal_meta& info) = 0;
};

class property_base {
public:
    virtual std::string_view get_name() const      = 0;
    virtual std::string_view get_signature() const = 0;
    virtual uint32_t         get_access() const    = 0;

    virtual abstract_interface* get_interface() const = 0;
    virtual abstract_object*    get_object() const    = 0;

    virtual mc::variant get_value() const = 0;
    void                set_value(const mc::variant& value) {
        set_variant(value);
    }

    virtual property_changed_signal& property_changed() = 0;

protected:
    virtual void set_variant(const mc::variant& value) = 0;
};

class abstract_object : public mc::core::object {
public:
    using managed_objects = std::map<std::string_view, abstract_object*>;
    using mc::core::object::connect;

    abstract_object(core_object* parent) : mc::core::object(parent) {
    }

    virtual ~abstract_object() = default;

    abstract_object* get_parent() const override;

    void     set_service(service& s);
    service* get_service() const override;

    virtual const managed_objects& get_managed_objects() const                 = 0;
    virtual void                   add_managed_object(abstract_object* obj)    = 0;
    virtual void                   remove_managed_object(abstract_object* obj) = 0;

    virtual std::string_view get_object_name() const                 = 0;
    virtual void             set_object_name(std::string_view name)  = 0;
    virtual std::string_view get_object_path() const                 = 0;
    virtual void             set_object_path(std::string_view path)  = 0;
    virtual std::string_view get_position() const                    = 0;
    virtual void             set_position(std::string_view position) = 0;
    virtual std::string_view get_class_name() const                  = 0;

    virtual bool                has_interface(std::string_view interface_name) const = 0;
    virtual abstract_interface* get_interface(std::string_view interface_name)       = 0;

    virtual mc::variant    get_property(std::string_view property_name,
                                        std::string_view interface_name = {})      = 0;
    virtual property_base* get_property_base(std::string_view property_name,
                                             std::string_view interface_name = {}) = 0;
    virtual bool           has_property(std::string_view property_name,
                                        std::string_view interface_name = {})      = 0;
    virtual mc::dict       get_all_properties(std::string_view interface_name)     = 0;
    virtual bool           set_property(std::string_view property_name, const mc::variant& value,
                                        std::string_view interface_name = {})      = 0;
    virtual mc::connection_type connect(std::string_view signal_name, slot_type slot,
                                        std::string_view interface_name = {})      = 0;
    virtual mc::variant         emit(std::string_view signal_name, const mc::variants& args,
                                     std::string_view interface_name = {})         = 0;

    virtual void visit(visitor& v) const = 0;

    virtual bool          has_method(std::string_view method_name,
                                     std::string_view interface_name = {}) const = 0;
    virtual invoke_result invoke(std::string_view method_name, const mc::variants& args,
                                 std::string_view interface_name = {})           = 0;

    virtual void notify_property_changed(const mc::variant& value, const property_base& prop) = 0;
    virtual property_changed_signal& property_changed()                                       = 0;
};

class abstract_interface : public mc::core::object {
public:
    using mc::core::object::connect;

    virtual ~abstract_interface() = default;

    virtual abstract_object* get_parent() const override;
    virtual abstract_object* get_owner() const = 0;

    virtual std::string_view    get_interface_name() const                                   = 0;
    virtual mc::connection_type connect(std::string_view signal_name, slot_type slot)        = 0;
    virtual mc::variant         emit(std::string_view signal_name, const mc::variants& args) = 0;
    virtual mc::variant         get_property(std::string_view property_name) const           = 0;
    virtual std::string_view    get_property_name(const property_base* prop)                 = 0;
    virtual property_base*      get_property_base(std::string_view property_name)            = 0;
    virtual bool                has_property(std::string_view property_name)                 = 0;
    virtual mc::dict            get_all_properties()                                         = 0;
    virtual bool set_property(std::string_view property_name, const mc::variant& value)      = 0;

    virtual bool          has_method(std::string_view method_name) const                 = 0;
    virtual invoke_result invoke(std::string_view method_name, const mc::variants& args) = 0;

    virtual void notify_property_changed(const mc::variant& value, const property_base& prop) = 0;
    virtual property_changed_signal& property_changed()                                       = 0;

    virtual void visit(visitor& v) const = 0;
};

using object_ptr = mc::im::ref_ptr<abstract_object>;

} // namespace mc::engine

MC_REFLECT(mc::engine::abstract_object,                           // 配置计算属性（只读）
           (MC_COMPUTED_PROPERTY("path", get_object_path))        // path
           (MC_COMPUTED_PROPERTY("class_name", get_class_name))   // class_name
           (MC_COMPUTED_PROPERTY("object_name", get_object_name)) // object_name
           (MC_COMPUTED_PROPERTY("position", get_position))       // position
           (MC_COMPUTED_PROPERTY("object_id", get_object_id))     // object_id
)

namespace mc::engine {
MC_FIELD_INDEX_TAG(by_path, "path");
MC_FIELD_INDEX_TAG(by_class_name, "class_name");
MC_FIELD_INDEX_TAG(by_object_name, "object_name");

using path_non_unique_index =
    mc::db::ordered_non_unique<&abstract_object::get_object_path, by_path::tag>;
using path_unique_index = mc::db::ordered_unique<&abstract_object::get_object_path, by_path::tag>;
using class_name_index =
    mc::db::ordered_non_unique<&abstract_object::get_class_name, &abstract_object::get_position,
                               by_class_name::tag>;
using object_name_index =
    mc::db::ordered_unique<&abstract_object::get_object_name, by_object_name::tag>;

// 每个服务自己的对象表
using service_object_table_impl =
    mdb::table<abstract_object,
               mdb::indexed_by<path_unique_index, class_name_index, object_name_index>>;

using object_table_impl =
    mdb::table<abstract_object,
               mdb::indexed_by<path_non_unique_index, class_name_index, object_name_index>>;

// 服务对象表（每个服务一个）
class service_object_table : public service_object_table_impl {
public:
    service_object_table(const std::string& service_name)
        : service_object_table_impl(service_name + ".object_table") {
    }
};

// 全局对象表，与服务对象表的差别是 path 是非唯一索引
class object_table : public object_table_impl {
public:
    object_table() : object_table_impl("object_table") {
    }
};

} // namespace mc::engine

namespace mc {

inline void to_variant(const mc::engine::invoke_result& obj, mc::variant& v) {
    v = mc::variant(obj);
}

inline void from_variant(const mc::variant& v, mc::engine::invoke_result& obj) {
    static_cast<mc::variant&>(obj) = v;
}

} // namespace mc

#endif // MC_ENGINE_BASE_H
