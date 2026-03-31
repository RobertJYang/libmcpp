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
#include <mc/engine/metadata.h>
#include <mc/engine/service.h>
#include <mc/engine/signal_info.h>
#include <mc/engine/utils.h>
#include <mc/memory.h>
#include <mc/module.h>
#include <mc/reflect.h>
#include <mc/reflect/signature_helper.h>
#include <mc/result.h>
#include <mc/time.h>
#include <mc/traits.h>
#include <mc/variant.h>

#include <string_view>
#include <tuple>
#include <type_traits>

namespace mc::engine {
namespace mdb            = mc::db;
using method_type_info   = mc::reflect::method_type_info;
using property_type_info = mc::reflect::property_type_info;

using message            = mc::dbus::message;
using core_object        = mc::core::object;
using connection_id_type = mc::core::connection_id_type;

class abstract_object;
class abstract_interface;
class property_base;

using property_changed_signal = mc::signal<void(const mc::variant&, const property_base&)>;

using object_call_stack = detail::call_stack<service, abstract_object>;

using invoke_result = mc::variant;

using object_identifier_t = std::tuple<uint8_t, std::string, std::string, std::string>;

#define MC_REFLECT_FLAG_PROPERTY_TPL 0x01 // 接口的属性是 property<T> 类型的反射标记
#define MC_REFLECT_FLAG_INTERFACE    0x02 // 对象的属性是一个 interface 类型的反射标记

// 属性同步来源信息
struct property_sync_info {
    std::string                                                                 source;
    std::vector<std::tuple<std::string, std::string, std::string, std::string>> properties;
};

using property_sync_info_ptr = std::shared_ptr<property_sync_info>;

class MC_API property_base {
public:
    virtual std::string_view get_name() const      = 0;
    virtual std::string_view get_signature() const = 0;
    virtual uint32_t         get_access() const    = 0;
    virtual uint64_t         get_flags() const     = 0;

    virtual abstract_interface* get_interface() const                        = 0;
    virtual void                set_interface(abstract_interface* interface) = 0;
    virtual abstract_object*    get_object() const                           = 0;

    virtual mc::variant get_value(int options = 0) const = 0;

    void set_value(const mc::variant& value)
    {
        set_variant(value);
    }

    virtual property_changed_signal& property_changed() = 0;

    virtual mc::variant get_override_value() const                   = 0;
    virtual void        set_override_value(const mc::variant& value) = 0;

protected:
    virtual void set_variant(const mc::variant& value) = 0;
};

class MC_API abstract_object : public mc::core::object {
public:
    MC_REFLECTABLE("mc.engine.abstract_object");

    using managed_objects = std::map<std::string_view, abstract_object*>;
    using mc::core::object::connect;

    abstract_object(core_object* parent = nullptr) : mc::core::object(parent)
    {}

    virtual ~abstract_object() = default;

    virtual void     set_service(service* s) = 0;
    virtual service* get_service() const     = 0;

    virtual abstract_object*       get_owner() const                 = 0;
    virtual void                   set_owner(abstract_object* owner) = 0;
    virtual const managed_objects& get_managed_objects() const       = 0;

    virtual std::string_view    get_object_name() const                                      = 0;
    virtual void                set_object_name(std::string_view name)                       = 0;
    virtual std::string_view    get_object_path() const                                      = 0;
    virtual void                set_object_path(std::string_view path)                       = 0;
    virtual std::string_view    get_position() const                                         = 0;
    virtual void                set_position(std::string_view position)                      = 0;
    virtual std::string_view    get_class_name() const                                       = 0;
    virtual object_identifier_t get_object_identifier() const                                = 0;
    virtual void                set_object_identifier(const object_identifier_t& identifier) = 0;

    virtual bool                has_interface(std::string_view interface_name) const = 0;
    virtual abstract_interface* get_interface(std::string_view interface_name) const = 0;

    virtual mc::variant get_property(std::string_view property_name, std::string_view interface_name = {},
                                     int options = 0) const                                                      = 0;
    virtual bool        has_property(std::string_view property_name, std::string_view interface_name = {}) const = 0;
    virtual mc::dict    get_all_properties(std::string_view interface_name = {}, int options = 0) const          = 0;
    virtual bool        set_property(std::string_view property_name, const mc::variant& value,
                                     std::string_view interface_name = {})                                       = 0;

    virtual void                   set_property_ref_info(std::string_view property_name, const std::string& info,
                                                         std::string_view interface_name = {})        = 0;
    virtual std::string            get_property_ref_info(std::string_view property_name,
                                                         std::string_view interface_name = {}) const  = 0;
    virtual void                   set_property_sync_info(std::string_view property_name, property_sync_info_ptr info,
                                                          std::string_view interface_name = {})       = 0;
    virtual property_sync_info_ptr get_property_sync_info(std::string_view property_name,
                                                          std::string_view interface_name = {}) const = 0;

    virtual mc::connection_type connect(std::string_view signal_name, slot_type slot,
                                        std::string_view interface_name = {}) = 0;
    virtual mc::variant         emit(std::string_view signal_name, const mc::variants& args,
                                     std::string_view interface_name = {})    = 0;

    virtual bool          has_method(std::string_view method_name, std::string_view interface_name = {}) const = 0;
    virtual invoke_result invoke(std::string_view method_name, const mc::variants& args = {},
                                 std::string_view interface_name = {})                                         = 0;
    virtual result<mc::variant> async_invoke(std::string_view method_name, const mc::variants& args = {},
                                             std::string_view interface_name = {})                             = 0;

    virtual void                     notify_property_changed(const mc::variant& value, const property_base& prop) = 0;
    virtual property_changed_signal& property_changed()                                                           = 0;
    virtual void notify_property_update_shm(const mc::variant& value, const property_base& prop)                  = 0;
    virtual property_changed_signal& property_update_shm()                                                        = 0;

    mc::shared_ptr<abstract_object> shared_from_this()
    {
        return mc::shared_ptr<abstract_object>(this);
    }

    virtual const object_metadata& get_metadata() const = 0;

    // 获取当前调用的上下文对象
    context& get_context();

protected:
    friend class object_impl;
    virtual void add_managed_object(abstract_object* obj)    = 0;
    virtual void remove_managed_object(abstract_object* obj) = 0;
};

class MC_API abstract_interface : public mc::core::object {
public:
    using mc::core::object::connect;

    virtual ~abstract_interface() = default;

    virtual abstract_object* get_owner() const = 0;

    service* get_service() const;

    virtual std::string_view get_interface_name() const = 0;

    // 属性相关接口
    virtual mc::variant get_property(std::string_view property_name, int options = 0) const    = 0;
    virtual mc::dict    get_all_properties(int options = 0) const                              = 0;
    virtual bool        set_property(std::string_view property_name, const mc::variant& value) = 0;

    // 方法相关接口
    virtual invoke_result       invoke(std::string_view method_name, const mc::variants& args = {})       = 0;
    virtual result<mc::variant> async_invoke(std::string_view method_name, const mc::variants& args = {}) = 0;

    // 信号相关接口
    virtual mc::variant              emit(std::string_view signal_name, const mc::variants& args)                 = 0;
    virtual mc::connection_type      connect(std::string_view signal_name, slot_type slot)                        = 0;
    virtual void                     notify_property_changed(const mc::variant& value, const property_base& prop) = 0;
    virtual property_changed_signal& property_changed()                                                           = 0;

    // 类型反射相关接口
    virtual const metadata_list&      get_metadata() const                                    = 0;
    virtual const signal_type_info*   get_signal_info(std::string_view signal_name) const     = 0;
    virtual const method_type_info*   get_method_info(std::string_view method_name) const     = 0;
    virtual const property_type_info* get_property_info(std::string_view property_name) const = 0;
    virtual const property_type_info* get_property_info(const void* prop_addr) const          = 0;

    bool has_property(std::string_view property_name) const
    {
        return get_property_info(property_name) != nullptr;
    }
    bool has_method(std::string_view method_name) const
    {
        return get_method_info(method_name) != nullptr;
    }
    bool has_signal(std::string_view signal_name) const
    {
        return get_signal_info(signal_name) != nullptr;
    }

    // 获取当前调用的上下文对象
    context& get_context();
};

using object_ptr = mc::shared_ptr<abstract_object>;

} // namespace mc::engine

namespace mc::engine {
MC_FIELD_INDEX_TAG(by_path, "path");
MC_FIELD_INDEX_TAG(by_class_name, "class_name");
MC_FIELD_INDEX_TAG(by_object_name, "object_name");

using path_non_unique_index = mc::db::ordered_non_unique<&abstract_object::get_object_path, by_path::tag>;
using path_unique_index     = mc::db::ordered_unique<&abstract_object::get_object_path, by_path::tag>;
using class_name_index =
    mc::db::ordered_non_unique<&abstract_object::get_class_name, &abstract_object::get_position, by_class_name::tag>;
using object_name_index = mc::db::ordered_unique<&abstract_object::get_object_name, by_object_name::tag>;

// 每个服务自己的对象表
using service_object_table_impl =
    mdb::table<abstract_object, mdb::indexed_by<path_unique_index, class_name_index, object_name_index>>;

using object_table_impl =
    mdb::table<abstract_object, mdb::indexed_by<path_non_unique_index, class_name_index, object_name_index>>;

// 服务对象表（每个服务一个）
class service_object_table : public service_object_table_impl {
public:
    service_object_table(const std::string& service_name) : service_object_table_impl(service_name + ".object_table")
    {}
};

// 全局对象表，与服务对象表的差别是 path 是非唯一索引
class object_table : public object_table_impl {
public:
    object_table() : object_table_impl("object_table")
    {}
};

} // namespace mc::engine

#endif // MC_ENGINE_BASE_H
