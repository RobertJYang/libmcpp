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
#include <mc/dbus/path_iterator.h>
#include <mc/dbus/validator.h>
#include <mc/engine.h>
#include <mc/exception.h>
#include <mc/log.h>

namespace mdb = mc::db;

namespace mc::engine {

using object_table_ptr = std::shared_ptr<service_object_table>;

struct service_interface : public mc::engine::interface<service_interface> {
    MC_INTERFACE("bmc.kepler.maca")

    std::string m_service_name;
    std::string m_service_path;
};

struct service_object : public mc::engine::object<service_object> {
    MC_OBJECT(service_object, "ServiceObject", "/bmc/kepler/maca", (service_interface))

    void init(service* s) {
        m_interface.m_service_path = path_pattern;
        m_interface.m_service_path += "/";
        m_interface.m_service_path += s->name();

        set_object_name(s->name());
        set_object_path(m_object_path);
    }

    const std::string& get_name() const {
        return m_interface.m_service_name;
    }

    service_interface m_interface;
};

struct service_impl {
    service_impl();

    bool init(service* s);
    bool start();
    void stop();

    DBusHandlerResult on_path_message(mc::dbus::message& msg, abstract_object& obj);
    DBusHandlerResult on_filter_message(mc::dbus::message& msg);

    DBusHandlerResult on_method_call(abstract_object& obj, mc::dbus::message& msg);

    void register_object(abstract_object& obj);
    void unregister_object(std::string_view path);

    void adjust_object_parent(abstract_object& obj);

    std::mutex                      m_mutex;
    service*                        m_service;
    dbus::connection_ptr            m_connection;
    object_table_ptr                m_object_table;
    mc::im::ref_ptr<service_object> m_service_object;
};
} // namespace mc::engine

MC_REFLECT(mc::engine::service_interface, ((m_service_name, "name")))
MC_REFLECT(mc::engine::service_object, ((m_interface, "interface")))

using service_table =
    mdb::table<mc::engine::service_object,
               mdb::indexed_by<mdb::ordered_unique<&mc::engine::service_object::get_name>>>;

namespace mc::engine {

service_impl::service_impl() {
}

bool service_impl::init(service* s) {
    m_service        = s;
    m_service_object = mc::core::make_ref<service_object>();
    m_service_object->init(s);
    return true;
}

bool service_impl::start() {
    std::lock_guard lock(m_mutex);

    auto connection = mc::dbus::connection::open_session_bus(m_service->get_strand());
    if (!connection || !connection->start()) {
        elog("start service failed: cannot open dbus session");
        return false;
    }

    if (!connection->request_name(m_service->name())) {
        elog("start service failed: cannot request dbus name");
        return false;
    }

    connection->on_filter_message.connect([this](mc::dbus::message& msg) {
        return on_filter_message(msg);
    });

    m_connection   = connection;
    m_object_table = std::make_shared<service_object_table>(m_service->name());
    mc::engine::get_engine().register_table(m_object_table);
    register_object(*m_service_object);
    return true;
}

void service_impl::stop() {
    std::lock_guard lock(m_mutex);

    if (m_connection) {
        m_connection->disconnect();
        m_connection.reset();
    }

    auto& engine   = mc::engine::engine::get_instance();
    auto  services = engine.get_table<service_table>("services");
    services->remove(m_service_object);
    m_object_table->clear();
    engine.unregister_table(m_object_table);
}

void service_impl::register_object(abstract_object& obj) {
    m_object_table->add(mc::im::ref_ptr<abstract_object>(&obj));
    obj.set_service(*m_service);
    m_connection->register_path(obj.get_object_path(), [this, &obj](auto& msg) {
        return on_path_message(msg, obj);
    });
}

void service_impl::unregister_object(std::string_view path) {
}

DBusHandlerResult service_impl::on_filter_message(mc::dbus::message& msg) {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult service_impl::on_path_message(mc::dbus::message& msg, abstract_object& obj) {
    if (msg.is_method_call()) {
        return on_method_call(obj, msg);
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult service_impl::on_method_call(abstract_object& object, mc::dbus::message& msg) {
    context ctx(*m_service, object);
    ctx.set_call_info(detail::dbus_call{msg});
    auto& info = std::get<detail::dbus_call>(ctx.get_call_info());

    context_stack::context call_ctx(m_service, ctx);
    try {
        auto interface_name = msg.get_interface();
        auto method_name    = msg.get_member();
        auto args           = msg.read_args();

        auto result = object.invoke(method_name, args, interface_name);
        if (!result.is_valid()) {
            info.response =
                mc::dbus::message::new_error(msg, errors::unknown_method.name, "method not found");
        } else {
            info.response = mc::dbus::message::new_method_return(msg);
            mc::dbus::signature_iterator it(result.method->get_result_signature());
            if (!it.at_end()) {
                info.response.writer().write_variant(it, result, 0);
            }
        }
    } catch (const mc::method_call_exception& e) {
        // 用户主动抛出的调用错误只记录 debug 日志
        dlog("method call failed: ${error}", ("error", e.what()));

        auto& err     = ctx.get_error();
        info.response = mc::dbus::message::new_error(msg, err.name, err.to_string());
    } catch (const std::exception& e) {
        // 未知错误记录 error 日志
        elog("unknow method call failed: ${error}", ("error", e.what()));
        // TODO:: 目前为了调试方便将 e.what() 作为错误内容访问，后续应该隐藏程序内部信息避免安全隐患
        info.response = mc::dbus::message::new_error(msg, errors::failed.name, e.what());
    }

    m_connection->send(std::move(info.response));
    return DBUS_HANDLER_RESULT_HANDLED;
}

service::service(std::string_view name) : mc::core::service_base(std::string(name)) {
    m_impl = std::make_unique<service_impl>();
}

service::~service() {
}

bool service::init(dict args) {
    MC_UNUSED(args);

    if (!dbus::validator::is_valid_bus_name(this->name())) {
        elog("initialize service failed: invalid service name ${name}", ("name", this->name()));
        return false;
    }

    if (!m_impl->init(this)) {
        elog("initialize service failed: initialization failed");
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
        elog("start service failed: ${error}", ("error", e.what()));
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

void service::register_object(abstract_object& obj) {
    auto path = obj.get_object_path();
    MC_ASSERT(!path.empty(), "object path is empty");
    MC_ASSERT(mc::dbus::validator::is_valid_path(path), "invalid object path ${path}",
              ("path", path));

    MC_ASSERT(m_impl->m_object_table, "service is not started, cannot register object");
    m_impl->register_object(obj);
}

void service::unregister_object(std::string_view path) {
    m_impl->unregister_object(path);
}

service_object_table& service::get_object_table() const {
    return *m_impl->m_object_table;
}

} // namespace mc::engine
