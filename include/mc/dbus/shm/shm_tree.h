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
#ifndef MC_DBUS_SHM_SHM_TREE_H
#define MC_DBUS_SHM_SHM_TREE_H

#include <mc/dbus/shm/gvariant_convert.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/local_msg.h>
#include <mc/engine/base.h>
#include <mc/time.h>

namespace mc::dbus {
constexpr std::string_view OBJECT_PROPERTIES_INTERFACE = "bmc.kepler.Object.Properties";

class shm_tree {
public:
    shm_tree(std::string_view harbor_name, std::string_view service_name,
             std::string_view unique_name);

    void register_object(mc::engine::abstract_object& obj);

    void unregister_object(std::string_view path);

    std::optional<mc::variants> timeout_call(mc::milliseconds timeout,
                                             std::string_view service_name, std::string_view path,
                                             std::string_view interface, std::string_view method,
                                             std::string_view signature, const variants& args);

    static variant get_property(std::string_view service_name, std::string_view path,
                                std::string_view interface, std::string_view property);
    static void    set_property(std::string_view service_name, std::string_view path,
                                std::string_view interface, std::string_view property,
                                const variant& value);
    static void    set_property_inner(shm::shared_ptr<shm::property> prop, const variant& value);
    void           add_match(match_rule& rule, mc::dbus::match_cb_t&& cb, uint64_t id);
    void           remove_match(uint64_t id);

private:
    std::string                                         m_service_name;
    std::string                                         m_unique_name;
    shm::object_tree*                                   m_tree;
    std::unordered_map<uint64_t, std::function<void()>> m_shm_slots;
};

struct shm_obj_visitor : mc::engine::visitor {
    shm_obj_visitor(shm::object& shm_obj)
        : m_shm_ins(shm::shared_memory::get_instance()), m_shm_obj(shm_obj) {
    }

    void handle_interface_begin(const mc::engine::abstract_object&    obj,
                                const mc::engine::abstract_interface& iface) override {
        m_shm_intf = &m_shm_obj.register_interface(m_shm_ins, false, iface.get_interface_name());
    }

    void handle_interface_end(const mc::engine::abstract_object&    obj,
                              const mc::engine::abstract_interface& iface) override {
    }

    void handle(const mc::engine::abstract_object&        obj,
                const mc::engine::abstract_interface&     iface,
                const mc::engine::visitor::property_meta& info) override {
        shm::shared_ptr<shm::property> shm_prop = m_shm_intf->add_p(m_shm_ins, info.name, info.signature);
        shm_prop->set_read_privilege(info.read_privilege);
        shm_prop->set_write_privilege(info.write_privilege);
        shm_prop->set_flags(info.flags);
        if (iface.get_interface_name() == OBJECT_PROPERTIES_INTERFACE) {
            auto value = mc::engine::common_properties_interface::get_instance().get(info.name);
            shm_tree::set_property_inner(shm_prop, value);
        } else {
            auto value = iface.get_property(info.name, mc::engine::property_options::memory);
            shm_tree::set_property_inner(shm_prop, value);
        }
    }

    void handle(const mc::engine::abstract_object&      obj,
                const mc::engine::abstract_interface&   iface,
                const mc::engine::visitor::method_meta& info) override {
        shm::method& shm_method =
            m_shm_intf->add_m(m_shm_ins, info.name, info.args_signature, info.return_signature);
        shm_method.set_privilege(info.privilege);
        shm_method.set_flags(info.flags);
    }

    void handle(const mc::engine::abstract_object&      obj,
                const mc::engine::abstract_interface&   iface,
                const mc::engine::visitor::signal_meta& info) override {
        shm::signal& shm_signal = m_shm_intf->add_s(m_shm_ins, info.name, info.args_signature);
        shm_signal.set_flags(info.flags);
    }

    shm::shared_memory& m_shm_ins;
    shm::object&        m_shm_obj;
    shm::interface*     m_shm_intf;
};
} // namespace mc::dbus

#endif // MC_DBUS_SHM_SHM_TREE_H