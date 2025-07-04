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
#include <mc/engine/property.h>

namespace mc::dbus {

constexpr uint64_t SD_BUS_VTABLE_PROPERTY_EMITS_INVALIDATION = 1 << 6;
static DBus::Match::Context s_ctx;

struct interface_added_visitor : mc::engine::visitor {
    interface_added_visitor(const std::string_view& path)
        : m_signal(mc::dbus::message::new_signal(path, "org.freedesktop.DBus.ObjectManager",
                                                 "InterfacesAdded")),
          m_writer(m_signal.writer()), m_sig_it("oa{sa{sv}}") {
        mc::variant v(path);
        m_writer.write_variant(m_sig_it, v, 0);
        m_sig_it.next();
    }

    void handle_interface_begin(const mc::engine::abstract_object&    obj,
                                const mc::engine::abstract_interface& iface) override {
        m_properties[std::string(iface.get_interface_name())] = mc::mutable_dict();
    }

    void handle_interface_end(const mc::engine::abstract_object&    obj,
                              const mc::engine::abstract_interface& iface) override {
    }

    void handle(const mc::engine::abstract_object& obj, const mc::engine::abstract_interface& iface,
                const mc::engine::visitor::property_meta& info) override {
        auto& properties      = m_properties[std::string(iface.get_interface_name())];
        properties[info.name] = iface.get_property(info.name, mc::engine::property_options::memory);
    }

    void handle(const mc::engine::abstract_object& obj, const mc::engine::abstract_interface& iface,
                const mc::engine::visitor::method_meta& info) override {
    }

    void handle(const mc::engine::abstract_object& obj, const mc::engine::abstract_interface& iface,
                const mc::engine::visitor::signal_meta& info) override {
    }

    message& signal() {
        mc::mutable_dict properties;
        for (auto& [key, value] : m_properties) {
            properties[key] = value;
        }
        m_writer.write_variant(m_sig_it, properties, 0);
        return m_signal;
    }

    message                                           m_signal;
    message_writer                                    m_writer;
    std::unordered_map<std::string, mc::mutable_dict> m_properties;
    signature_iterator                                m_sig_it;
};

struct interface_removed_visitor : mc::engine::visitor {
    interface_removed_visitor(const std::string_view& path)
        : m_signal(mc::dbus::message::new_signal(path, "org.freedesktop.DBus.ObjectManager",
                                                 "InterfacesRemoved")),
          m_writer(m_signal.writer()), m_sig_it("oas") {
        mc::variant v(path);
        m_writer.write_variant(m_sig_it, v, 0);
        m_sig_it.next();
    }

    void handle_interface_begin(const mc::engine::abstract_object&    obj,
                                const mc::engine::abstract_interface& iface) override {
        m_interfaces.push_back(std::string(iface.get_interface_name()));
    }

    void handle_interface_end(const mc::engine::abstract_object&    obj,
                              const mc::engine::abstract_interface& iface) override {
    }

    void handle(const mc::engine::abstract_object& obj, const mc::engine::abstract_interface& iface,
                const mc::engine::visitor::property_meta& info) override {
    }

    void handle(const mc::engine::abstract_object& obj, const mc::engine::abstract_interface& iface,
                const mc::engine::visitor::method_meta& info) override {
    }

    void handle(const mc::engine::abstract_object& obj, const mc::engine::abstract_interface& iface,
                const mc::engine::visitor::signal_meta& info) override {
    }

    message& signal() {
        m_writer.write_variant(m_sig_it, m_interfaces, 0);
        return m_signal;
    }

    message                  m_signal;
    message_writer           m_writer;
    std::vector<std::string> m_interfaces;
    signature_iterator       m_sig_it;
};

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
    char* raw_data   = nullptr;
    int   len        = 0;
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
    auto iface     = prop.get_interface()->get_interface_name();
    auto prop_name = prop.get_name();
    auto signal = mc::dbus::message::new_signal(obj.get_object_path(), "org.freedesktop.DBus.Properties",
                                                "PropertiesChanged");
    signature_iterator it("sa{sv}as");
    auto               writer = signal.writer();
    mc::variant        v(iface);
    writer.write_variant(it, v, 0);
    it.next();
    mc::mutable_dict props_with_value;
    mc::variants props_without_value;
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
    interface_added_visitor visitor(obj.get_object_path());
    obj.visit(visitor);
    conn.send(std::move(visitor.signal()));
}

void emit_interfaces_removed(connection& conn, const engine::abstract_object& obj) {
    interface_removed_visitor visitor(obj.get_object_path());
    obj.visit(visitor);
    conn.send(std::move(visitor.signal()));
}

} // namespace mc::dbus
