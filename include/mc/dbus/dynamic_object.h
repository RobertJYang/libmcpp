/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_DBUS_DYNAMIC_OBJECT_H
#define MC_DBUS_DYNAMIC_OBJECT_H

#include <map>
#include <mc/core/object.h>
#include <mc/dict.h>
#include <mc/engine/object.h>
#include <mc/memory.h>
#include <mc/reflect/metadata_info.h>
#include <mc/sync/small_mutex.h>
#include <mc/variant.h>
#include <string>
#include <string_view>
#include <tuple>

namespace mc::dbus {

enum class access_property_rsp_code : int {
    success                        = 0,
    set_sync_property_err          = -1,
    set_ref_property_err           = -2,
    property_convert_type_err      = -3,
    set_property_unknown_err       = -4,
    property_not_exist_err         = -5,
    access_ref_object_property_err = -6,
    interface_not_exist_err        = -7,
    object_not_exist_err           = -8,
    get_ref_property_err           = -9,
    expression_parameter_err       = -10,
    property_invalid_err           = -11
};

struct MC_API dynamic_method {
    std::string name;
    std::string i_args;
    std::string o_args;
    int32_t     cb_ref;
    uint8_t     flags;
};

struct MC_API dynamic_property {
    std::string                  name;
    std::string                  signature;
    mc::variant                  value;
    bool                         readonly;
    uint8_t                      flags;
    shm::weak_ptr<shm::property> shm_prop;
};

struct MC_API dynamic_signal {
    std::string name;
    std::string signature;
    uint8_t     flags;
};

class MC_API dynamic_interface : public mc::core::object {
public:
    dynamic_interface(std::string_view name);

    void             add_property(std::string_view name, dynamic_property prop);
    void             add_method(std::string_view name, dynamic_method m);
    void             add_signal(std::string_view name, dynamic_signal s);
    bool             set_property(std::string property_name, const mc::variant& value);
    mc::variant      get_property(std::string property_name) const;
    bool             has_property(std::string property_name) const;
    std::string_view get_name() const;
    mc::dict         get_all_properties() const;
    void             update_shm_prop(std::string_view property_name, const mc::variant& value);

    std::map<std::string, dynamic_property>& get_properties();

private:
    std::map<std::string, dynamic_method>   m_methods;
    std::string                             m_interface_name;
    std::map<std::string, dynamic_property> m_properties;
    std::map<std::string, dynamic_signal>   m_signals;
    mutable mc::sync::spin_mutex            m_property_access_mutex;
};

class MC_API dynamic_object : public mc::engine::object_impl {
public:
    dynamic_object(mc::core::object* parent = nullptr);
    using mc::engine::object_impl::get_interface;

    mc::variant get_property(std::string_view property_name, std::string_view interface_name,
                             int options = 0) const override;
    bool        set_property(std::string_view property_name, const mc::variant& value,
                             std::string_view interface_name) override;
    mc::dict    get_all_properties(std::string_view interface_name = {}, int options = 0) const override;
    bool        has_property(std::string_view property_name, std::string_view interface_name) const override;
    std::tuple<int, mc::variant> try_get_property(std::string_view property_name,
                                                  std::string_view interface_name) const;
    int try_set_property(std::string_view property_name, const mc::variant& value, std::string_view interface_name);

    void                              add_interface(mc::shared_ptr<dynamic_interface>);
    mc::shared_ptr<dynamic_interface> get_interface(std::string intf_name) const;

    std::string_view                   get_class_name() const override;
    std::string_view                   get_path_pattern() const override;
    const mc::engine::object_metadata& get_metadata() const override;

    void update_shm_prop(std::string_view property_name, const mc::variant& value, std::string_view interface_name);

    std::map<std::string, mc::shared_ptr<dynamic_interface>>& get_interfaces();

private:
    std::map<std::string, mc::shared_ptr<dynamic_interface>> m_interfaces;
    mutable std::unique_ptr<mc::engine::object_metadata>     m_metadata;
    mutable std::unique_ptr<mc::reflect::struct_metadata>    m_reflect_metadata;
};

} // namespace mc::dbus

#endif // MC_DBUS_DYNAMIC_OBJECT_H