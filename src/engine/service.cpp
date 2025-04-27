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
#include <mc/db/database.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/message.h>
#include <mc/dbus/validator.h>
#include <mc/engine/base.h>
#include <mc/engine/engine.h>
#include <mc/engine/interface.h>
#include <mc/engine/macro.h>
#include <mc/engine/object.h>
#include <mc/engine/service.h>
#include <mc/exception.h>
#include <mc/log.h>

namespace mdb = mc::db;

namespace mc::engine {
using object_tree     = mdb::table<object_wrap, mdb::indexed_by<path_index>>;
using object_tree_ptr = std::shared_ptr<object_tree>;

struct service_interface : public mc::engine::interface<service_interface> {
    MC_INTERFACE("org.openubmc.service")

    std::string m_service_name;
    std::string m_service_path;
};

struct service_object : public mc::engine::object<service_object> {
    MC_OBJECT("/org/openubmc/service", (service_interface))

    void init(service* s) {
        m_interface.m_service_path = path_pattern;
        m_interface.m_service_path += "/";
        m_interface.m_service_path += s->name();

        m_interface.m_service_name = s->name();

        set_service_name(m_service_name);
        set_object_name(m_service_name);
        set_object_path(m_object_path);
    }

    const std::string& get_name() const {
        return m_interface.m_service_name;
    }

    service_interface m_interface;
};

struct service_impl {
    bool init(service* s);
    bool start();
    void stop();

    DBusHandlerResult on_path_message(mc::dbus::message& msg);
    DBusHandlerResult on_filter_message(mc::dbus::message& msg);

    DBusHandlerResult on_method_call(object_base& obj, mc::dbus::message& msg);

    std::mutex                      m_mutex;
    service*                        m_service;
    dbus::connection_ptr            m_connection;
    object_tree_ptr                 m_object_tree;
    mc::im::ref_ptr<service_object> m_service_object;
};
} // namespace mc::engine

MC_REFLECT(mc::engine::service_interface, ((m_service_name, "name")))
MC_REFLECT(mc::engine::service_object, ((m_interface, "interface")))

using service_table = mdb::table<
    mc::engine::service_object,
    mdb::indexed_by<mdb::ordered_unique<mdb::member<mc::engine::service_object, const std::string&,
                                                    &mc::engine::service_object::get_name>>>>;

namespace mc::engine {
bool service_impl::init(service* s) {
    m_service        = s;
    m_service_object = mc::im::make_ref<service_object>();
    m_service_object->init(s);
    return true;
}

bool service_impl::start() {
    std::lock_guard lock(m_mutex);

    auto& engine     = mc::engine::engine::get_instance();
    auto  connection = mc::dbus::connection::open_session_bus(engine.get_io_context());
    if (!connection || !connection->start()) {
        elog("初始化服务失败: 无法打开DBus会话");
        return false;
    }

    if (!connection->request_name(m_service->name())) {
        elog("初始化服务失败: 无法请求DBus名称");
        return false;
    }

    connection->on_filter_message.connect([this](mc::dbus::message& msg) {
        return on_filter_message(msg);
    });

    m_connection  = connection;
    m_object_tree = std::make_shared<object_tree>("object_tree");
    m_object_tree->add(object_wrap::create(m_service_object.get()));
    m_connection->register_path(m_service_object->get_object_path(), [this](auto& msg) {
        return on_path_message(msg);
    });
    return true;
}

void service_impl::stop() {
    std::lock_guard lock(m_mutex);

    if (m_connection) {
        m_connection->disconnect();
        m_connection.reset();
    }
}

DBusHandlerResult service_impl::on_filter_message(mc::dbus::message& msg) {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult service_impl::on_path_message(mc::dbus::message& msg) {
    std::string path(msg.get_path());

    auto it = m_object_tree->get<by_path>().find(path);
    if (it.is_end()) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (msg.is_method_call()) {
        return on_method_call(*it->m_object, msg);
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult service_impl::on_method_call(object_base& object, mc::dbus::message& msg) {
    auto method    = msg.get_member();
    auto interface = msg.get_interface();
    auto args      = msg.read_args();

    auto result = object.invoke(method, args, interface);
    if (result.is_null()) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    auto reply = mc::dbus::message::new_method_return(msg);
    reply.writer() << result;
    m_connection->send(std::move(reply));

    return DBUS_HANDLER_RESULT_HANDLED;
}

service::service(std::string_view name) : mc::service_base<service>(std::string(name)) {
    m_impl = std::make_unique<service_impl>();
}

service::~service() {
}

bool service::init(dict args) {
    MC_UNUSED(args);

    if (!dbus::validator::is_valid_bus_name(m_name)) {
        elog("初始化服务失败: 无效的服务名 ${name}", ("name", m_name));
        return false;
    }

    if (!m_impl->init(this)) {
        elog("初始化服务失败: 初始化失败");
        return false;
    }

    auto& engine   = mc::engine::engine::get_instance();
    auto  services = engine.get_table<service_table>("services");
    services->add(m_impl->m_service_object);
    return true;
}

bool service::start() {
    try {
        return m_impl->start();
    } catch (const std::exception& e) {
        elog("初始化服务失败: ${error}", ("error", e.what()));
        return false;
    }
}

bool service::stop() {
    m_impl->stop();
    return true;
}

void service::cleanup() {
}

bool service::is_healthy() const {
    return true;
}

} // namespace mc::engine
