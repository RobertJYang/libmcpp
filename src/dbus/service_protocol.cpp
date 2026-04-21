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

#include <mc/dbus/service_protocol.h>

#include <mc/dbus/signal.h>
#include <mc/engine/service.h>
#include <mc/error.h>
#include <mc/exception.h>

namespace mc::dbus {

struct dbus_service_protocol::exported_object_state {
    mc::engine::abstract_object* object{nullptr};
    mc::connection_type          property_changed;
};

namespace {

constexpr std::string_view PROPERTIES_INTERFACE = "org.freedesktop.DBus.Properties";
constexpr std::string_view PROPERTIES_CHANGED   = "PropertiesChanged";

struct dispatch_state {
    const mc::engine::service_operation*                              operation{nullptr};
    mc::engine::abstract_object*                                      object{nullptr};
    std::function<void(mc::engine::abstract_object&)>                 export_object;
    std::function<void(std::string_view)>                             unexport_object;
    std::function<void(const mc::engine::update_property_operation&)> publish_property_update;
    sd_bus*                                                           bus{nullptr};
    std::string                                                       endpoint;
    std::string                                                       path;
    std::string                                                       interface_name;
    std::string                                                       method_name;
    std::string                                                       signature;
    mc::variants                                                      args;
    mc::milliseconds                                                  timeout{mc::minutes(10)};
    mc::variant                                                       result;
};

void send_reply_message(DBusConnection* raw_conn, message& reply)
{
    MC_ASSERT(raw_conn != nullptr, "dbus raw connection is null");
    MC_ASSERT(dbus_connection_send(raw_conn, reply.get_dbus_message(), nullptr), "dbus send reply failed");
    dbus_connection_flush(raw_conn);
}

void send_empty_reply(DBusConnection* raw_conn, message& request)
{
    auto reply = message::new_method_return(request);
    send_reply_message(raw_conn, reply);
}

void send_error_reply(DBusConnection* raw_conn, message& request, std::string_view error_name,
                      std::string_view error_message)
{
    auto reply = message::new_error(request, error_name, error_message);
    send_reply_message(raw_conn, reply);
}

void write_method_result(message& reply, mc::string_view signature, const mc::variant& value)
{
    if (signature.empty()) {
        return;
    }

    auto writer = reply.writer();
    writer.write_variant(signature_iterator(signature), value, 0);
}

bool prepare_invoke_request(dispatch_state& self)
{
    auto* invoke = std::get_if<mc::engine::invoke_method_operation>(&self.operation->payload);
    if (invoke == nullptr) {
        return false;
    }

    self.endpoint       = invoke->target.endpoint;
    self.path           = invoke->target.path;
    self.interface_name = invoke->target.interface_name;
    self.method_name    = invoke->method_name;
    self.signature      = invoke->signature;
    self.args           = invoke->args;
    self.timeout        = invoke->timeout;
    return true;
}

static method_call_params make_method_call_params(const dispatch_state& state)
{
    return {state.endpoint, state.path, state.interface_name, state.method_name, state.signature, state.args};
}

} // namespace

dbus_service_protocol::dbus_service_protocol(sd_bus& bus) : m_bus(&bus)
{}

mc::string_view dbus_service_protocol::name() const noexcept
{
    return "dbus";
}

void dbus_service_protocol::attach(mc::engine::service& service)
{
    m_service          = &service;
    auto& object_table = service.get_object_table();
    for (auto it = object_table.begin(); it != object_table.end(); ++it) {
        if (it->second) {
            register_object(*it->second);
        }
    }
}

void dbus_service_protocol::detach()
{
    std::vector<std::string> paths;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        paths.reserve(m_exported_objects.size());
        for (const auto& [path, _] : m_exported_objects) {
            paths.push_back(path);
        }
    }

    for (const auto& path : paths) {
        unexport_object(path);
    }
    m_service = nullptr;
}

void dbus_service_protocol::register_object(mc::engine::abstract_object& obj)
{
    mc::engine::service_operation operation{
        mc::engine::service_operation_kind::register_object,
        mc::engine::register_object_operation{std::string(obj.get_object_path()), std::string(obj.get_class_name())},
    };
    MC_UNUSED(perform_request(operation, &obj));
}

void dbus_service_protocol::unregister_object(mc::string_view path)
{
    mc::engine::service_operation operation{
        mc::engine::service_operation_kind::unregister_object,
        mc::engine::unregister_object_operation{std::string(path)},
    };
    MC_UNUSED(perform_request(operation, nullptr));
}

mc::variant dbus_service_protocol::request(const mc::engine::service_operation& operation)
{
    return perform_request(operation, nullptr);
}

mc::variant dbus_service_protocol::perform_request(const mc::engine::service_operation& operation,
                                                   mc::engine::abstract_object*         obj)
{
    dispatch_state entry;
    entry.operation        = &operation;
    entry.object           = obj;
    entry.bus              = m_bus;
    entry.export_object    = [this](mc::engine::abstract_object& target) {
        export_object(target);
    };
    entry.unexport_object = [this](std::string_view path) {
        unexport_object(path);
    };
    entry.publish_property_update = [this](const mc::engine::update_property_operation& update_property) {
        publish_property_update(update_property);
    };

    if (entry.operation == nullptr) {
        MC_THROW(mc::invalid_arg_exception, "dbus service protocol request failed: service operation is null");
    }
    if (entry.bus == nullptr) {
        MC_THROW(mc::invalid_op_exception, "dbus service protocol request failed: dbus bus is null");
    }

    switch (entry.operation->kind) {
        case mc::engine::service_operation_kind::register_object:
            MC_ASSERT_THROW(entry.object != nullptr, mc::invalid_arg_exception, "register_object target is null");
            entry.export_object(*entry.object);
            return entry.result;
        case mc::engine::service_operation_kind::unregister_object: {
            auto* unregister_object = std::get_if<mc::engine::unregister_object_operation>(&entry.operation->payload);
            MC_ASSERT_THROW(unregister_object != nullptr, mc::invalid_arg_exception,
                            "unregister_object payload is invalid");
            entry.unexport_object(unregister_object->path);
            return entry.result;
        }
        case mc::engine::service_operation_kind::update_property: {
            auto* update_property = std::get_if<mc::engine::update_property_operation>(&entry.operation->payload);
            MC_ASSERT_THROW(update_property != nullptr, mc::invalid_arg_exception, "update_property payload is invalid");
            entry.publish_property_update(*update_property);
            return entry.result;
        }
        case mc::engine::service_operation_kind::invoke_method:
            MC_ASSERT_THROW(prepare_invoke_request(entry), mc::invalid_arg_exception,
                            "invoke_method payload is invalid");
            if (auto result = entry.bus->shm_timeout_call(entry.timeout, make_method_call_params(entry));
                result.has_value()) {
                entry.result = mc::variant(result.value());
            } else {
                entry.result = mc::variant(entry.bus->timeout_call(entry.timeout, make_method_call_params(entry)));
            }
            return entry.result;
    }

    return entry.result;
}

mc::result<mc::variant> dbus_service_protocol::async_request(const mc::engine::service_operation& operation)
{
    try {
        return mc::make_result(perform_request(operation, nullptr));
    } catch (const mc::exception& ex) {
        return mc::make_result<mc::variant>(ex);
    } catch (const std::exception& ex) {
        return mc::make_result<mc::variant>(mc::make_error("mc.dbus.service_protocol.error", ex.what()));
    }
}

uint64_t dbus_service_protocol::add_match(mc::dbus::match_rule& rule, std::function<void(mc::dbus::message&)>&& cb)
{
    MC_ASSERT_THROW(m_bus != nullptr, mc::invalid_op_exception, "dbus service protocol bus is not initialized");
    return m_bus->add_match(rule, std::move(cb));
}

void dbus_service_protocol::remove_match(uint64_t id)
{
    MC_ASSERT_THROW(m_bus != nullptr, mc::invalid_op_exception, "dbus service protocol bus is not initialized");
    m_bus->remove_match(id);
}

void dbus_service_protocol::export_object(mc::engine::abstract_object& obj)
{
    const std::string path(obj.get_object_path());
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_exported_objects.find(path) != m_exported_objects.end()) {
            return;
        }
    }

    auto state    = std::make_shared<exported_object_state>();
    state->object = &obj;
    state->property_changed =
        obj.property_changed().connect([this, &obj](const mc::variant& value, const mc::engine::property_base& prop) {
        if (m_bus == nullptr) {
            return;
        }
        emit_properties_changed(m_bus->get_connection(), obj, prop, value);
    });

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_exported_objects.emplace(path, state);
    }

    m_bus->get_connection().register_path(path, [this, path](message& msg) {
        return handle_exported_object_message(path, msg);
    });
    emit_interfaces_added(m_bus->get_connection(), obj);
}

void dbus_service_protocol::unexport_object(mc::string_view path)
{
    std::shared_ptr<exported_object_state> state;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto                        it = m_exported_objects.find(std::string(path));
        if (it == m_exported_objects.end()) {
            return;
        }
        state = std::move(it->second);
        m_exported_objects.erase(it);
    }

    if (state && state->object != nullptr) {
        emit_interfaces_removed(m_bus->get_connection(), *state->object);
    }
    if (state) {
        state->property_changed.disconnect();
    }
    m_bus->get_connection().unregister_path(path);
}

void dbus_service_protocol::publish_property_update(const mc::engine::update_property_operation& operation)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_exported_objects.find(operation.path) == m_exported_objects.end()) {
        return;
    }

    auto signal = message::new_signal(operation.path, PROPERTIES_INTERFACE, PROPERTIES_CHANGED);
    auto writer = signal.writer();
    writer << operation.interface_name;
    writer.write_container("a{sv}", [&](message_writer& dict_writer, auto) {
        dict_writer << operation.property_name << operation.value;
    });
    writer.write_container("as", [&](message_writer&, auto) {
    });
    send_signal(m_bus->get_connection(), signal);
}

DBusHandlerResult dbus_service_protocol::handle_exported_object_message(mc::string_view path, message& msg)
{
    if (!msg.is_method_call()) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    std::shared_ptr<exported_object_state> state;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto                        it = m_exported_objects.find(std::string(path));
        if (it == m_exported_objects.end()) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        state = it->second;
    }

    DBusConnection* raw_conn = m_bus->get_connection().get_connection();
    try {
        const auto interface_name = std::string(msg.get_interface());
        const auto member_name    = std::string(msg.get_member());
        const auto args           = msg.read_args();

        if (interface_name == PROPERTIES_INTERFACE) {
            if (member_name == "Get") {
                auto reply  = message::new_method_return(msg);
                auto writer = reply.writer();
                writer << state->object->get_property(args[1].as_string(), args[0].as_string());
                send_reply_message(raw_conn, reply);
                return DBUS_HANDLER_RESULT_HANDLED;
            }

            if (member_name == "GetAll") {
                auto reply  = message::new_method_return(msg);
                auto writer = reply.writer();
                writer << state->object->get_all_properties(args[0].as_string());
                send_reply_message(raw_conn, reply);
                return DBUS_HANDLER_RESULT_HANDLED;
            }

            if (member_name == "Set") {
                if (!state->object->set_property(args[1].as_string(), args[2], args[0].as_string())) {
                    MC_THROW(mc::not_found_exception, "property not found: ${interface}.${property}",
                             ("interface", args[0].as_string())("property", args[1].as_string()));
                }
                send_empty_reply(raw_conn, msg);
                return DBUS_HANDLER_RESULT_HANDLED;
            }
        }

        auto method_info = state->object->get_metadata().get_method_info(member_name, interface_name);
        if (method_info.item == nullptr) {
            MC_THROW(mc::not_found_exception, "method not found: ${interface}.${method}",
                     ("interface", interface_name)("method", member_name));
        }

        auto reply  = message::new_method_return(msg);
        auto result = state->object->invoke(member_name, args, interface_name);
        write_method_result(reply, method_info.item->get_result_signature(), result);
        send_reply_message(raw_conn, reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    } catch (const mc::exception& ex) {
        send_error_reply(raw_conn, msg, ex.name(), ex.what());
        return DBUS_HANDLER_RESULT_HANDLED;
    } catch (const std::exception& ex) {
        send_error_reply(raw_conn, msg, "mc.dbus.service_protocol.error", ex.what());
        return DBUS_HANDLER_RESULT_HANDLED;
    }
}

} // namespace mc::dbus
