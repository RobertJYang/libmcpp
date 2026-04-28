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
#include <mc/engine/message.h>
#include <mc/engine/payload.h>
#include <mc/engine/service.h>
#include <mc/log/log.h>
#include <mc/object_base.h>

#include <utility>

namespace mc::app::legacy_shm {

namespace {

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
    const mc::reflect::method_type_info* method_info = nullptr;
    auto&                                table       = svc.get_object_table();
    auto path_key      = mc::string_view(path.data(), path.size());
    auto interface_key = mc::string_view(interface_name.data(), interface_name.size());
    auto method_key    = mc::string_view(method_name.data(), method_name.size());
    auto it            = table.find<mc::engine::by_path>(path_key);
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

    mc::dbus::method_handler_t handler =
        [this](std::string_view path, std::string_view interface, std::string_view method,
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

    m_added_conn = mc::scoped_connection(object_table.on_object_added.connect([this](mc::object_base& obj) {
        on_object_added_signal(obj);
    }));
    m_removed_conn = mc::scoped_connection(object_table.on_object_removed.connect([this](mc::object_base& obj) {
        on_object_removed_signal(obj);
    }));

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

} // namespace mc::app::legacy_shm

#endif // MCDBUS_USE_OLD_SHM
