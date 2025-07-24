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

#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/signal.h>
#include <mc/engine/metadata.h>
#include <mc/engine/property.h>

namespace mc::dbus {

constexpr uint64_t          SD_BUS_VTABLE_PROPERTY_EMITS_INVALIDATION = 1 << 6;
static DBus::Match::Context s_ctx;

static void send_properties_changed(connection& conn, message& signal) {
    std::unordered_map<std::string, std::string> destinations;
    shm_lock_call([&]() {
        auto& shm = shm::shared_memory::get_instance().get_base();
        s_ctx.set_req(signal.get_dbus_message());
        shm._matchs.run(s_ctx, [&](DBus::Match::Context& _, shm::object_tree* tree) {
            std::string unique_name(tree->unique_name());
            if (destinations.find(unique_name) == destinations.end()) {
                destinations[unique_name] = std::string(tree->wellknow_name());
            }
        });
    });
    if (signal.get_serial() == 0) {
        signal.set_serial(conn.get_next_serial());
    }
    char* raw_data = nullptr;
    int   len      = 0;
    if (!dbus_message_marshal(signal.get_dbus_message(), &raw_data, &len)) {
        elog("failed to marshal properties changed signal");
        return;
    }
    auto& harbor = harbor::get_instance();
    for (auto& [unique_name, wellknow_name] : destinations) {
        auto msg_queue = harbor.get_destination_msg_queue(wellknow_name);
        if (msg_queue == nullptr) {
            signal.set_serial(0);
            signal.set_destination(unique_name);
            conn.send(std::move(signal));
            continue;
        }
        if (!msg_queue->push_back(std::string(raw_data, len), MSG_QUEUE_PUSH_TIMEOUT)) {
            elog("failed to push properties changed signal to msg queue");
        }
    }
    free(raw_data);
}

void emit_properties_changed(connection& conn, engine::abstract_object& obj,
                             const engine::property_base& prop, const variant& value) {
    auto               iface     = prop.get_interface()->get_interface_name();
    auto               prop_name = prop.get_name();
    auto               signal    = mc::dbus::message::new_signal(obj.get_object_path(), "org.freedesktop.DBus.Properties",
                                                                 "PropertiesChanged");
    signature_iterator it("sa{sv}as");
    auto               writer = signal.writer();
    mc::variant        v(iface);
    writer.write_variant(it, v, 0);
    it.next();
    mc::mutable_dict props_with_value;
    mc::variants     props_without_value;
    if ((prop.get_flags() & SD_BUS_VTABLE_PROPERTY_EMITS_INVALIDATION) == 0) {
        props_with_value[prop_name] = value;
    } else {
        props_without_value.push_back(prop_name);
    }
    writer.write_variant(it, props_with_value, 0);
    it.next();
    writer.write_variant(it, props_without_value, 0);
    send_properties_changed(conn, signal);
}

void emit_interfaces_added(connection& conn, const engine::abstract_object& obj) {
    auto signal = mc::dbus::message::new_signal(
        obj.get_object_path(), "org.freedesktop.DBus.ObjectManager", "InterfacesAdded");
    auto writer = signal.writer();
    writer << obj.get_object_path();
    writer.write_variant("oa{sa{sv}}", obj.get_all_properties({}, mc::engine::property_options::memory), 0);
    conn.send(std::move(signal));
}

void emit_interfaces_removed(connection& conn, const engine::abstract_object& obj) {
    auto signal = mc::dbus::message::new_signal(
        obj.get_object_path(), "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved");
    auto writer = signal.writer();
    writer << obj.get_object_path();

    std::vector<std::string_view> interfaces;
    obj.get_metadata().visit_interfaces([&](const mc::engine::interface_metadata& iface) {
        interfaces.push_back(iface.metadata->get_class_name());
    });
    writer.write_variant("oas", interfaces, 0);
    conn.send(std::move(signal));
}

} // namespace mc::dbus
