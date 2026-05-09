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

#include "service_backend.h"

#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM

#include <mc/dbus/connection.h>
#include <mc/dbus/match.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/dbus/signal.h>
#include <mc/engine/base.h>
#include <mc/engine/dispatcher.h>
#include <mc/engine/engine.h>
#include <mc/engine/match.h>
#include <mc/engine/message.h>
#include <mc/engine/payload.h>
#include <mc/engine/service.h>
#include <mc/engine/std_interface.h>
#include <mc/log/log.h>

#include <chrono>
#include <optional>
#include <thread>
#include <unistd.h>
#include <utility>

namespace mc::app {

namespace {

constexpr const char* SERVICE_BACKEND_SHM_LOCK_PATH         = "/dev/shm/init_shm.lock";
constexpr auto        SERVICE_BACKEND_SHM_WAIT_RETRY_MS     = std::chrono::milliseconds(200);
constexpr auto        SERVICE_BACKEND_SHM_WAIT_LOG_INTERVAL = std::chrono::seconds(5);

void _wait_for_backend_shm_lock()
{
    auto last_log = std::chrono::steady_clock::time_point{};
    while (::access(SERVICE_BACKEND_SHM_LOCK_PATH, F_OK) != 0 && !::shm::shared_memory::is_exist()) {
        auto now = std::chrono::steady_clock::now();
        if (last_log.time_since_epoch().count() == 0 || now - last_log >= SERVICE_BACKEND_SHM_WAIT_LOG_INTERVAL) {
            ilog("service_backend 等待旧 SHM 初始化锁: ${path}", ("path", SERVICE_BACKEND_SHM_LOCK_PATH));
            last_log = now;
        }
        std::this_thread::sleep_for(SERVICE_BACKEND_SHM_WAIT_RETRY_MS);
    }
}

const mc::reflect::method_type_info* _resolve_method_info(mc::engine::service& svc, std::string_view path,
                                                          std::string_view interface_name, std::string_view method_name)
{
    if (auto* standard_method = mc::engine::standard_interfaces::get_method_info(interface_name, method_name);
        standard_method != nullptr) {
        return standard_method;
    }

    auto& table         = svc.get_object_table();
    auto  path_key      = mc::string_view(path.data(), path.size());
    auto  interface_key = mc::string_view(interface_name.data(), interface_name.size());
    auto  method_key    = mc::string_view(method_name.data(), method_name.size());
    auto  it            = table.find<mc::engine::by_path>(path_key);
    if (!it.is_end()) {
        const auto& meta = (*it).get_metadata();
        auto        info = meta.get_method_info(method_key, interface_key);
        if (info.item != nullptr) {
            return info.item;
        }
    }
    return nullptr;
}

mc::dbus::invoke_result _dispatch_via_engine(mc::engine::service& svc, std::string_view path,
                                             std::string_view interface_name, std::string_view method_name,
                                             const mc::variants& args)
{
    mc::engine::message request;
    request.header.type           = mc::engine::message_type::method_call;
    request.header.destination    = mc::string(svc.name());
    request.header.path           = mc::string(path);
    request.header.interface_name = mc::string(interface_name);
    request.header.member_name    = mc::string(method_name);
    request.body                  = mc::engine::make_payload<mc::engine::method_call_payload>(mc::string_view{}, args);

    mc::engine::message response = mc::engine::dispatch(svc, request);

    auto* method_info = _resolve_method_info(svc, path, interface_name, method_name);

    if (response.header.type == mc::engine::message_type::method_return) {
        if (auto* payload = response.try_as<mc::engine::method_return_payload>(); payload != nullptr) {
            return {method_info, payload->value};
        }
        return {method_info, mc::variant{}};
    }

    if (response.header.type == mc::engine::message_type::error) {
        if (auto* err = response.try_as<mc::engine::error_payload>(); err != nullptr) {
            wlog("service_backend dispatch error: ${name}: ${msg}",
                 ("name", mc::string(err->name))("msg", mc::string(err->message)));
        }
    }
    return {nullptr, mc::variant{}};
}

void _shm_register_object(mc::dbus::shm_tree& tree, mc::engine::abstract_object& obj)
{
    mc::dbus::shm_global_lock_exec([&tree, &obj]() {
        tree.register_object(obj);
    });
}

void _shm_unregister_object(mc::dbus::shm_tree& tree, std::string_view path)
{
    mc::dbus::shm_global_lock_exec([&tree, path]() {
        tree.unregister_object(path);
    });
}

bool _is_signal_rule(const mc::engine::match_rule& rule)
{
    return rule.type.empty() || rule.type == "signal";
}

mc::string _path_namespace_from_pattern(mc::string_view path)
{
    auto pos = path.find('*');
    if (pos == mc::string_view::npos) {
        return mc::string(path);
    }

    mc::string prefix(path.substr(0, pos));
    while (prefix.size() > 1U && prefix.back() == '/') {
        prefix.pop_back();
    }
    if (prefix.empty()) {
        return mc::string("/");
    }
    return prefix;
}

std::optional<mc::dbus::match_rule> _make_dbus_match_rule(const mc::engine::match_rule& rule)
{
    if (!_is_signal_rule(rule)) {
        return std::nullopt;
    }

    auto dbus_rule = mc::dbus::match_rule::new_signal(rule.member_name, rule.interface_name);
    if (!rule.sender.empty()) {
        dbus_rule.with_sender(rule.sender);
    }
    if (!rule.destination.empty()) {
        dbus_rule.with_destination(rule.destination);
    }
    if (!rule.path.empty()) {
        auto path = _path_namespace_from_pattern(rule.path);
        if (path == rule.path) {
            dbus_rule.with_path(rule.path);
        } else {
            dbus_rule.with_path_namespace(path);
        }
    }
    return dbus_rule;
}

mc::variants _read_signal_args(mc::dbus::message& wire_msg)
{
    try {
        return wire_msg.read_args();
    } catch (const std::exception& ex) {
        elog("service_backend read signal args failed: ${error}", ("error", ex.what()));
        return {};
    }
}

mc::engine::message _make_engine_signal(mc::dbus::message& wire_msg)
{
    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::signal;
    msg.header.destination    = mc::string(wire_msg.get_destination());
    msg.header.sender         = mc::string(wire_msg.get_sender());
    msg.header.path           = mc::string(wire_msg.get_path());
    msg.header.interface_name = mc::string(wire_msg.get_interface());
    msg.header.member_name    = mc::string(wire_msg.get_member());
    msg.header.serial         = wire_msg.get_serial();
    msg.body = mc::engine::make_payload<mc::engine::signal_payload>(mc::string(wire_msg.get_signature()),
                                                                    _read_signal_args(wire_msg));
    return msg;
}

std::size_t _count_signature_fields(mc::string_view signature) noexcept
{
    std::size_t count = 0;
    for (mc::dbus::signature_iterator it(signature); !it.at_end(); it.next()) {
        ++count;
    }
    return count;
}

bool _write_wire_args(mc::dbus::message& msg, const mc::variants& args, mc::string_view signature)
{
    if (_count_signature_fields(signature) != args.size()) {
        elog("service_backend write signal args failed: signature=${sig} args=${args}",
             ("sig", signature)("args", args.size()));
        return false;
    }

    auto writer = msg.writer();
    auto sig_it = mc::dbus::signature_iterator(signature);
    for (const auto& arg : args) {
        if (sig_it.at_end()) {
            return false;
        }
        writer.write_variant(sig_it, arg, 0);
        sig_it.next();
    }
    return sig_it.at_end();
}

bool _make_wire_signal(const mc::engine::message& msg, mc::dbus::message& wire, bool probe)
{
    if (msg.header.path.empty() || msg.header.interface_name.empty() || msg.header.member_name.empty()) {
        return false;
    }

    wire = mc::dbus::message::new_signal(msg.header.path, msg.header.interface_name, msg.header.member_name);
    if (!msg.header.destination.empty()) {
        wire.set_destination(msg.header.destination);
    }
    if (probe && !msg.header.sender.empty()) {
        wire.set_sender(msg.header.sender);
    }

    if (const auto* sig_payload = msg.try_as<mc::engine::signal_payload>(); sig_payload != nullptr) {
        return _write_wire_args(wire, sig_payload->args, sig_payload->signature);
    }
    return true;
}

} // namespace

service_backend::service_backend(mc::engine::service& svc, mc::dbus::connection& conn)
    : m_service(svc), m_connection(conn)
{}

service_backend::~service_backend()
{
    uninstall();
}

bool service_backend::install()
{
    if (m_installed) {
        return true;
    }

    _wait_for_backend_shm_lock();

    auto svc_name = m_service.name();
    auto uniq     = m_connection.get_unique_name();
    if (svc_name.empty() || uniq.empty()) {
        wlog("service_backend install 跳过：service_name 或 unique_name 为空");
        return false;
    }
    m_service_name.assign(svc_name.data(), svc_name.size());
    m_unique_name.assign(uniq.data(), uniq.size());

    auto& harbor = mc::dbus::harbor::get_instance();
    harbor.set_harbor_name_if_empty(std::string("harbor.") + m_service_name);
    harbor.register_unique_name(m_unique_name, m_service_name);

    mc::dbus::method_handler_t handler = [this](std::string_view path, std::string_view interface,
                                                std::string_view    method,
                                                const mc::variants& args) -> mc::dbus::invoke_result {
        return _dispatch_via_engine(m_service, path, interface, method, args);
    };
    harbor.register_method_handler(m_service_name, m_unique_name, std::move(handler));

    m_shm_tree = std::make_unique<mc::dbus::shm_tree>(harbor.get_harbor_name(), m_service_name, m_unique_name);
    harbor.start();

    auto& object_table = m_service.get_object_table();
    for (auto it = object_table.begin(0U); it != object_table.end(0U); ++it) {
        if (it.is_end()) {
            break;
        }
        const auto& obj_ptr = (*it).second;
        if (obj_ptr) {
            register_to_shm(*obj_ptr);
        }
    }

    m_service.set_backend(this);
    m_installed = true;
    dlog("service_backend install completed: service=${service}, unique=${unique}",
         ("service", m_service_name)("unique", m_unique_name));
    return true;
}

void service_backend::uninstall()
{
    if (!m_installed) {
        return;
    }

    m_service.set_backend(nullptr);

    std::unordered_map<mc::engine::match_id, uint64_t> shm_match_ids;
    {
        std::lock_guard lock(m_match_mutex);
        shm_match_ids.swap(m_shm_match_ids);
    }
    for (const auto& item : shm_match_ids) {
        try {
            if (m_shm_tree) {
                m_shm_tree->remove_match(item.second);
            }
            m_connection.remove_match(item.second);
        } catch (const std::exception& ex) {
            elog("service_backend remove shm match failed: ${error}", ("error", ex.what()));
        }
    }

    if (!m_service_name.empty()) {
        mc::dbus::harbor::get_instance().unregister_service(m_service_name);
    }

    m_shm_tree.reset();
    m_installed = false;
}

bool service_backend::emit(const mc::engine::message& msg)
{
    if (!m_installed || !m_shm_tree || msg.header.type != mc::engine::message_type::signal) {
        return true;
    }

    try {
        mc::dbus::message probe;
        if (!_make_wire_signal(msg, probe, true)) {
            return true;
        }
        if (!mc::dbus::shm_tree::test_shm_match(probe.get_dbus_message())) {
            return true;
        }

        mc::dbus::message wire;
        if (!_make_wire_signal(msg, wire, false)) {
            return true;
        }
        mc::dbus::send_signal(m_connection, wire);
        return false;
    } catch (const std::exception& ex) {
        elog("service_backend emit signal failed: ${error}", ("error", ex.what()));
        return true;
    }
}

void service_backend::on_object_added(mc::engine::abstract_object& obj)
{
    register_to_shm(obj);
}

void service_backend::on_object_removed(mc::engine::abstract_object& obj)
{
    unregister_from_shm(obj.get_object_path());
}

void service_backend::on_match_added(mc::engine::match_id id, const mc::engine::match_rule& rule)
{
    if (!m_shm_tree) {
        wlog("service_backend skip match add: shm_tree not ready, id=${id}", ("id", id));
        return;
    }

    auto dbus_rule = _make_dbus_match_rule(rule);
    if (!dbus_rule.has_value()) {
        return;
    }

    try {
        mc::dbus::match_cb_t callback = [this, id](mc::dbus::message& wire_msg) {
            dispatch_signal_to_match(id, wire_msg);
        };

        auto legacy_id = m_shm_tree->add_match(dbus_rule.value(), mc::dbus::match_cb_t(callback));

        auto cloned_rule = dbus_rule->clone();
        m_connection.add_match(cloned_rule, std::move(callback), legacy_id);

        std::lock_guard lock(m_match_mutex);
        m_shm_match_ids[id] = legacy_id;
    } catch (const std::exception& ex) {
        elog("service_backend add shm match failed: ${error}", ("error", ex.what()));
    }
}

void service_backend::on_match_removed(mc::engine::match_id id)
{
    unregister_match_from_shm(id);
}

void service_backend::register_to_shm(mc::engine::abstract_object& obj)
{
    if (!m_shm_tree) {
        return;
    }
    _shm_register_object(*m_shm_tree, obj);
}

void service_backend::unregister_from_shm(mc::string_view path)
{
    if (!m_shm_tree) {
        return;
    }
    _shm_unregister_object(*m_shm_tree, std::string_view(path.data(), path.size()));
}

void service_backend::unregister_match_from_shm(mc::engine::match_id id)
{
    uint64_t legacy_id = 0;
    {
        std::lock_guard lock(m_match_mutex);
        auto            it = m_shm_match_ids.find(id);
        if (it == m_shm_match_ids.end()) {
            return;
        }
        legacy_id = it->second;
        m_shm_match_ids.erase(it);
    }

    try {
        if (m_shm_tree) {
            m_shm_tree->remove_match(legacy_id);
        }
        m_connection.remove_match(legacy_id);
    } catch (const std::exception& ex) {
        elog("service_backend remove shm match failed: ${error}", ("error", ex.what()));
    }
}

void service_backend::dispatch_signal_to_match(mc::engine::match_id id, mc::dbus::message& wire_msg)
{
    try {
        auto msg   = _make_engine_signal(wire_msg);
        auto table = mc::engine::engine::get_match_table();
        if (!table) {
            wlog("service_backend dispatch skipped: match_table not ready");
            return;
        }

        auto targets      = table->find_targets(msg);
        auto service_name = mc::string(m_service.name());
        for (const auto& target : targets) {
            if (target.id != id || target.service_name != service_name) {
                continue;
            }
            mc::engine::match::set_target_match_ids(msg.header, {{service_name, id}});
            m_service.dispatch_event(msg);
            return;
        }
    } catch (const std::exception& ex) {
        elog("service_backend dispatch signal failed: ${error}", ("error", ex.what()));
    }
}

} // namespace mc::app

#endif // MCDBUS_USE_OLD_SHM
