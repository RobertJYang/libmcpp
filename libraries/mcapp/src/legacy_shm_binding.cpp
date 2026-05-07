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

#include "legacy_shm_binding.h"

#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM

#include <mc/dbus/connection.h>
#include <mc/dbus/match.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/engine/base.h>
#include <mc/engine/dispatcher.h>
#include <mc/engine/engine.h>
#include <mc/engine/match.h>
#include <mc/engine/message.h>
#include <mc/engine/payload.h>
#include <mc/engine/service.h>
#include <mc/log/log.h>
#include <mc/object_base.h>

#include <cstdint>
#include <chrono>
#include <optional>
#include <thread>
#include <unistd.h>
#include <utility>

namespace mc::app::legacy_shm {

namespace {

constexpr const char* LEGACY_SHM_LOCK_PATH         = "/dev/shm/init_shm.lock";
constexpr auto        LEGACY_SHM_WAIT_RETRY_MS     = std::chrono::milliseconds(200);
constexpr auto        LEGACY_SHM_WAIT_LOG_INTERVAL = std::chrono::seconds(5);

void _wait_for_legacy_shm_lock()
{
    auto last_log = std::chrono::steady_clock::time_point{};
    while (::access(LEGACY_SHM_LOCK_PATH, F_OK) != 0) {
        auto now = std::chrono::steady_clock::now();
        if (last_log.time_since_epoch().count() == 0
            || now - last_log >= LEGACY_SHM_WAIT_LOG_INTERVAL) {
            ilog("legacy_shm install 等待 SHM 初始化锁: ${path}",
                 ("path", LEGACY_SHM_LOCK_PATH));
            last_log = now;
        }
        std::this_thread::sleep_for(LEGACY_SHM_WAIT_RETRY_MS);
    }
}

// 把 harbor 投递进来的 (path, interface, method, args) 调用转换为 mcengine
// method_call message，走 mc::engine::dispatch 派发到目标对象，再从
// method_return / error payload 中解出返回值。method_type_info 单独再查一次，
// 用于 harbor 端按返回签名 marshal。
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

    // dispatcher 已经在内部调过 invoke + 查过 method_info，但只通过 payload.signature
    // 暴露返回签名；harbor 这一层需要 method_type_info* 给本地反编排使用，所以这里
    // 重新查一次（少量额外开销）。对象不存在时 method_info 为 nullptr，与旧实现行为一致。
    const mc::reflect::method_type_info* method_info   = nullptr;
    auto&                                table         = svc.get_object_table();
    auto                                 path_key      = mc::string_view(path.data(), path.size());
    auto                                 interface_key = mc::string_view(interface_name.data(), interface_name.size());
    auto                                 method_key    = mc::string_view(method_name.data(), method_name.size());
    auto                                 it            = table.find<mc::engine::by_path>(path_key);
    if (!it.is_end()) {
        const auto& meta = (*it).get_metadata();
        auto        info = meta.get_method_info(method_key, interface_key);
        method_info      = info.item;
    }

    if (response.header.type == mc::engine::message_type::method_return) {
        if (auto* payload = response.try_as<mc::engine::method_return_payload>(); payload != nullptr) {
            return {method_info, payload->value};
        }
        return {method_info, mc::variant{}};
    }

    if (response.header.type == mc::engine::message_type::error) {
        if (auto* err = response.try_as<mc::engine::error_payload>(); err != nullptr) {
            wlog("legacy_shm dispatch error: ${name}: ${msg}",
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
        elog("legacy_shm read signal args failed: ${error}", ("error", ex.what()));
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

} // namespace

binding::binding(mc::engine::service& svc, mc::dbus::connection& conn) : m_service(svc), m_connection(conn)
{}

binding::~binding()
{
    uninstall();
}

bool binding::install()
{
    if (m_installed) {
        return true;
    }

    _wait_for_legacy_shm_lock();

    auto svc_name = m_service.name();
    auto uniq     = m_connection.get_unique_name();
    if (svc_name.empty() || uniq.empty()) {
        wlog("legacy_shm install 跳过：service_name 或 unique_name 为空");
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

    // harbor::start 内部按 m_is_running 判定幂等，多 service 重复调用安全。
    harbor.start();

    auto& object_table = m_service.get_object_table();

    // 已经在表中的对象先一次性同步到 shm_tree（install 时机一般在 service::start
    // 之前，table 为空；这里仅作兜底）。raw_iterator 解引用得到 pair<key, stored_ref>，
    // stored 是 shared_ptr<abstract_object>。
    for (auto it = object_table.begin(0U); it != object_table.end(0U); ++it) {
        if (it.is_end()) {
            break;
        }
        const auto& obj_ptr = (*it).second;
        if (obj_ptr) {
            register_to_shm(*obj_ptr);
        }
    }

    m_added_conn       = mc::scoped_connection(object_table.on_object_added.connect([this](mc::object_base& obj) {
        on_object_added_signal(obj);
    }));
    m_removed_conn     = mc::scoped_connection(object_table.on_object_removed.connect([this](mc::object_base& obj) {
        on_object_removed_signal(obj);
    }));
    m_match_added_conn = mc::scoped_connection(
        m_service.on_match_added.connect([this](mc::engine::match_id id, const mc::engine::match_rule& rule) {
        on_match_added_signal(id, rule);
    }));
    m_match_removed_conn = mc::scoped_connection(m_service.on_match_removed.connect([this](mc::engine::match_id id) {
        on_match_removed_signal(id);
    }));

    dlog("legacy_shm install completed: service=${service}, unique=${unique}, add_slots=${add_slots}, "
         "remove_slots=${remove_slots}, add_conn=${add_conn}, remove_conn=${remove_conn}",
         ("service", m_service_name)("unique", m_unique_name)("add_slots", m_service.on_match_added.slot_count())(
             "remove_slots", m_service.on_match_removed.slot_count())("add_conn", m_match_added_conn.connected())(
             "remove_conn", m_match_removed_conn.connected()));
    m_installed = true;
    return true;
}

void binding::uninstall()
{
    if (!m_installed) {
        return;
    }

    m_added_conn.disconnect();
    m_removed_conn.disconnect();
    m_match_added_conn.disconnect();
    m_match_removed_conn.disconnect();

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
            elog("legacy_shm remove shm match failed: ${error}", ("error", ex.what()));
        }
    }

    if (!m_service_name.empty()) {
        mc::dbus::harbor::get_instance().unregister_service(m_service_name);
    }

    // shm_tree 是 SHM 中跨进程 object_tree 的本进程 wrapper：wrapper 释放不影响
    // SHM 中实际数据，仅意味着本 service 不再持有索引访问点。语义与旧实现一致。
    m_shm_tree.reset();

    m_installed = false;
}

void binding::on_object_added_signal(mc::object_base& obj)
{
    register_to_shm(static_cast<mc::engine::abstract_object&>(obj));
}

void binding::on_object_removed_signal(mc::object_base& obj)
{
    auto& engine_obj = static_cast<mc::engine::abstract_object&>(obj);
    unregister_from_shm(engine_obj.get_object_path());
}

void binding::on_match_added_signal(mc::engine::match_id id, const mc::engine::match_rule& rule)
{
    if (!m_shm_tree) {
        wlog("legacy_shm skip match add: shm_tree not ready, id=${id}", ("id", id));
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

        // 在旧 shm_tree/harbor 注册一份，同时在当前 service connection 上也执行
        // add_match（不仅是 add_rule），确保当前进程能够直接从 DBus 收到信号。
        auto cloned_rule = dbus_rule->clone();
        m_connection.add_match(cloned_rule, std::move(callback), legacy_id);

        std::lock_guard lock(m_match_mutex);
        m_shm_match_ids[id] = legacy_id;
    } catch (const std::exception& ex) {
        elog("legacy_shm add shm match failed: ${error}", ("error", ex.what()));
    }
}

void binding::on_match_removed_signal(mc::engine::match_id id)
{
    unregister_match_from_shm(id);
}

void binding::register_to_shm(mc::engine::abstract_object& obj)
{
    if (!m_shm_tree) {
        return;
    }
    _shm_register_object(*m_shm_tree, obj);
}

void binding::unregister_from_shm(mc::string_view path)
{
    if (!m_shm_tree) {
        return;
    }
    _shm_unregister_object(*m_shm_tree, std::string_view(path.data(), path.size()));
}

void binding::unregister_match_from_shm(mc::engine::match_id id)
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
        elog("legacy_shm remove shm match failed: ${error}", ("error", ex.what()));
    }
}

void binding::dispatch_signal_to_match(mc::engine::match_id id, mc::dbus::message& wire_msg)
{
    try {
        auto msg   = _make_engine_signal(wire_msg);
        auto table = mc::engine::engine::get_match_table();
        if (!table) {
            wlog("legacy_shm dispatch skipped: match_table not ready");
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
        elog("legacy_shm dispatch signal failed: ${error}", ("error", ex.what()));
    }
}

} // namespace mc::app::legacy_shm

#endif // MCDBUS_USE_OLD_SHM
