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
#include <cstdio>
#include <mc/engine/base.h>
#include <mc/engine/dispatcher.h>
#include <mc/engine/engine.h>
#include <mc/engine/event.h>
#include <mc/engine/internal/shm_binding.h>
#include <mc/engine/match.h>
#include <mc/engine/message.h>
#include <mc/engine/metadata.h>
#include <mc/engine/path.h>
#include <mc/engine/path_iterator.h>
#include <mc/engine/property/types.h>
#include <mc/engine/service.h>
#include <mc/engine/service_proto.h>
#include <mc/engine/utils.h>
#include <mc/exception.h>
#include <mc/log/log.h>
#include <mc/runtime/runtime_context.h>

#include <mutex>
#include <unordered_map>
#include <unordered_set>

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
#include <mc/engine/endpoint_service.h>

#include "match/shared_table.h"
#endif

#include <mc/engine/std_interface.h>

namespace mc::engine {

namespace {

#define MC_SIGNAL_KEY(NAME, LITERAL) static const mc::string NAME = mc::string::from_quark(LITERAL##_q)

MC_SIGNAL_KEY(if_object_manager, "org.freedesktop.DBus.ObjectManager");
MC_SIGNAL_KEY(if_properties, "org.freedesktop.DBus.Properties");
MC_SIGNAL_KEY(sig_interfaces_added, "InterfacesAdded");
MC_SIGNAL_KEY(sig_interfaces_removed, "InterfacesRemoved");
MC_SIGNAL_KEY(sig_properties_changed, "PropertiesChanged");
MC_SIGNAL_KEY(sig_oa, "oa{sa{sv}}");
MC_SIGNAL_KEY(sig_oas, "oas");
MC_SIGNAL_KEY(sig_sas, "sa{sv}as");

#undef MC_SIGNAL_KEY

message _make_interfaces_added(mc::string_view sender, const abstract_object& obj)
{
    mc::string parent_path = mc::string("/");
    if (auto* parent_ptr = dynamic_cast<abstract_object*>(obj.get_parent().get())) {
        parent_path = mc::string(parent_ptr->get_object_path());
    }
    message msg;
    msg.header.type           = message_type::signal;
    msg.header.sender         = mc::string(sender);
    msg.header.path           = std::move(parent_path);
    msg.header.interface_name = if_object_manager;
    msg.header.member_name    = sig_interfaces_added;
    msg.body                  = make_payload<signal_payload>(
        sig_oa, mc::variants{mc::variant(mc::string(obj.get_object_path())),
                             mc::variant(obj.get_all_properties({}, property_options::memory))});
    return msg;
}

message _make_interfaces_removed(mc::string_view sender, const abstract_object& obj)
{
    mc::variants iface_names;
    obj.get_metadata().visit_interfaces([&](const interface_metadata& iface) {
        if (iface.metadata != nullptr) {
            iface_names.push_back(mc::variant(mc::string(iface.metadata->get_class_name())));
        }
    });

    // DBus 规范要求 InterfacesAdded/Removed 信号从实现 ObjectManager 的
    // 父对象 path 发送，而非子对象自身的 path。
    mc::string parent_path = mc::string("/");
    if (auto* parent_ptr = dynamic_cast<abstract_object*>(obj.get_parent().get())) {
        parent_path = mc::string(parent_ptr->get_object_path());
    }
    message msg;
    msg.header.type           = message_type::signal;
    msg.header.sender         = mc::string(sender);
    msg.header.path           = std::move(parent_path);
    msg.header.interface_name = if_object_manager;
    msg.header.member_name    = sig_interfaces_removed;
    msg.body                  = make_payload<signal_payload>(
        sig_oas, mc::variants{mc::variant(mc::string(obj.get_object_path())), mc::variant(std::move(iface_names))});
    return msg;
}

message _make_properties_changed(mc::string_view sender, const abstract_object& obj, const property_base& prop,
                                 const mc::variant& value)
{
    const auto* iface = prop.get_interface();
    MC_ASSERT(iface != nullptr, "property base does not carry interface metadata");
    auto iface_name = iface->get_interface_name();
    auto prop_name  = prop.get_name();

    mc::dict     changed;
    mc::variants invalidated;
    if (const auto* info = iface->get_property_info(prop_name);
        info != nullptr && info->has_flags(MC_REFLECT_FLAG_INVALIDATED)) {
        invalidated.push_back(mc::variant(mc::string(prop_name)));
    } else {
        changed[mc::string(prop_name)] = value;
    }

    message msg;
    msg.header.type           = message_type::signal;
    msg.header.sender         = mc::string(sender);
    msg.header.path           = mc::string(obj.get_object_path());
    msg.header.interface_name = if_properties;
    msg.header.member_name    = sig_properties_changed;
    msg.body                  = make_payload<signal_payload>(sig_sas, mc::variants{mc::variant(mc::string(iface_name)),
                                                                  mc::variant(std::move(changed)),
                                                                  mc::variant(std::move(invalidated))});
    return msg;
}

} // namespace

using object_table_ptr = std::shared_ptr<service_object_table>;

namespace {

// property_changed → PropertiesChanged signal 的全局事件过滤器。
//
// 容错原则：DBus PropertiesChanged 信号是 property 赋值的副作用，
// 任何信号构造/发送失败都不能反过来打断 property 赋值本身。
// 典型场景：对象先 set_service 后才 set_parent；在两步之间发生属性赋值时，
// _make_properties_changed 中调用 obj.get_object_path() 会因 parent 缺失抛异常，
// 这里必须吞掉，仅记录日志。
void _property_changed_global_filter(mc::object& target, mc::event& e)
{
    auto* property_event = dynamic_cast<property_changed_event*>(&e);
    if (property_event == nullptr) {
        return;
    }

    auto* obj = property_event->property().get_object();
    if (obj == nullptr) {
        return;
    }

    auto* svc = obj->get_service();
    if (svc == nullptr) {
        return;
    }

    try {
        const auto& prop  = property_event->property();
        const auto& value = property_event->value();
        auto        msg   = _make_properties_changed(svc->name(), *obj, prop, value);
        svc->emit(msg);
    } catch (const std::exception& ex) {
        dlog("PropertiesChanged 信号发送失败，跳过: ${error}", ("error", ex.what()));
    } catch (...) {
        dlog("PropertiesChanged 信号发送失败，跳过: 未知异常");
    }
}

} // namespace

struct service_impl {
    std::mutex       m_mutex;
    service*         m_service{nullptr};
    object_table_ptr m_object_table;
    bool             m_registered{false};
    bool             m_started{false};
    service_proto*   m_proto{nullptr};

    std::mutex                          m_match_mutex;
    std::unordered_set<match::match_id> m_owned_matches;
    shm_binding::service_state*         m_shm_state{nullptr};

    service_impl() : m_shm_state(shm_binding::create_service_state())
    {}
    ~service_impl()
    {
        shm_binding::destroy_service_state(m_shm_state);
    }

    void             ensure_registered();
    void             unregister_from_engine();
    void             register_object(abstract_object& obj);
    void             unregister_object(mc::string_view path);
    abstract_object* find_owner(mc::string_view path) const;
    void             _restore_owner_relations();
};

void service_impl::ensure_registered()
{
    if (m_registered) {
        return;
    }

    mc::runtime::get_runtime_context().install_global_filter(property_changed_event_id,
                                                             _property_changed_global_filter);

    if (!m_object_table) {
        m_object_table = shm_binding::create_service_object_table(m_service->name());
    }
    mc::engine::engine::get_instance().register_table(m_object_table);
    mc::engine::engine::register_service(m_service);
    m_registered = true;

    shm_binding::attach_and_recover(m_shm_state, m_service->name(), *m_object_table);
    _restore_owner_relations();
    (void)m_service->gc_isolated();
}

void service_impl::unregister_from_engine()
{
    if (!m_registered || !m_object_table) {
        return;
    }

    m_object_table->clear();
    shm_binding::detach(m_shm_state);
    mc::engine::engine::unregister_service(m_service);
    mc::engine::engine::get_instance().unregister_table(m_object_table);
    m_registered = false;
    std::unordered_set<match::match_id> drained;
    {
        std::lock_guard lock(m_match_mutex);
        drained.swap(m_owned_matches);
    }
    if (!drained.empty()) {
        auto table = mc::engine::engine::get_match_table();
        if (table) {
            for (auto id : drained) {
                m_service->on_match_removed(id);
                table->unsubscribe(id);
            }
        }
    }
}

void service_impl::_restore_owner_relations()
{
    for (auto it = m_object_table->begin(0U); it != m_object_table->end(0U); ++it) {
        const auto& obj_ptr = (*it).second;
        if (!obj_ptr) {
            continue;
        }
        obj_ptr->set_service(m_service);
        auto* owner = find_owner(obj_ptr->get_object_path());
        if (owner != nullptr && owner != obj_ptr.get()) {
            obj_ptr->set_owner(owner);
        }
    }
}

void service_impl::register_object(abstract_object& obj)
{
    auto shared_obj = obj.shared_from_this();
    MC_ASSERT(shared_obj, "Object must be managed by shared_ptr");

    shm_binding::ensure_object_handle(obj, m_shm_state);

    m_object_table->add(shared_obj);
    obj.set_service(m_service);

    auto* owner = find_owner(obj.get_object_path());
    if (owner) {
        obj.set_owner(owner);
    }

    shm_binding::notify_after_register();
}

abstract_object* service_impl::find_owner(mc::string_view path) const
{
    if (path.empty() || path[0] != '/') {
        return nullptr;
    }

    if (!m_object_table) {
        return nullptr;
    }

    mc::engine::path current_path{std::string(path)};
    auto             parent_path = current_path.parent();
    while (parent_path != "/") {
        auto it = m_object_table->find<by_path>(parent_path.str());
        if (!it.is_end()) {
            return const_cast<abstract_object*>(&(*it));
        }
        parent_path = parent_path.parent();
    }

    auto root_it = m_object_table->find<by_path>("/");
    if (!root_it.is_end()) {
        return const_cast<abstract_object*>(&(*root_it));
    }

    return nullptr;
}

void service_impl::unregister_object(mc::string_view path)
{
    auto it = m_object_table->find<by_path>(path);
    if (it.is_end()) {
        return;
    }

    auto& obj = const_cast<abstract_object&>(*it);
    for (auto& child : obj.get_children()) {
        auto engine_child = mc::dynamic_pointer_cast<mc::engine::abstract_object>(child);
        if (engine_child) {
            unregister_object(engine_child->get_object_path());
        }
    }
    m_object_table->remove(mc::shared_ptr<abstract_object>(&obj));

    obj.set_owner(nullptr);
    obj.set_service(nullptr);

    shm_binding::release_object_handle(obj);
}

service::service(mc::string_view name, mc::object* parent)
    : mc::object(parent), m_name(name), m_impl(std::make_unique<service_impl>())
{
    m_impl->m_service = this;
    set_name(std::string(m_name));
}

service::~service()
{
    stop();
}

mc::string_view service::name() const
{
    return m_name;
}

bool service::init(dict args)
{
    m_impl->ensure_registered();
    return on_init(std::move(args));
}

bool service::start()
{
    m_impl->ensure_registered();

    {
        std::lock_guard lock(m_impl->m_mutex);
        if (m_impl->m_started) {
            return true;
        }
        m_impl->m_started = true;
    }
    if (on_start()) {
        return true;
    }

    {
        std::lock_guard lock(m_impl->m_mutex);
        m_impl->m_started = false;
    }
    m_impl->unregister_from_engine();
    return false;
}

bool service::stop()
{
    if (m_impl) {
        {
            std::lock_guard lock(m_impl->m_mutex);
            if (!m_impl->m_started) {
                m_impl->unregister_from_engine();
                return true;
            }
        }
        if (!on_stop()) {
            return false;
        }
        {
            std::lock_guard lock(m_impl->m_mutex);
            m_impl->m_started = false;
        }
        m_impl->unregister_from_engine();
    }
    return true;
}

void service::cleanup()
{
    on_cleanup();
}

bool service::is_healthy() const
{
    return true;
}

void service::on_dump(mc::dict context, mc::string filepath)
{
    MC_UNUSED(context);
    MC_UNUSED(filepath);
}

void service::on_detach_debug_console(mc::dict context)
{
    MC_UNUSED(context);
}

int32_t service::on_reboot_prepare(mc::dict context)
{
    MC_UNUSED(context);
    return 0;
}

int32_t service::on_reboot_process(mc::dict context)
{
    MC_UNUSED(context);
    return 0;
}

int32_t service::on_reboot_action(mc::dict context)
{
    MC_UNUSED(context);
    return 0;
}

void service::on_reboot_cancel(mc::dict context)
{
    MC_UNUSED(context);
}

bool service::on_init(dict args)
{
    MC_UNUSED(args);
    return true;
}

bool service::on_start()
{
    return true;
}

bool service::on_stop()
{
    return true;
}

void service::on_cleanup()
{}

namespace {

struct local_endpoint_id {
    std::uint16_t endpoint_id{0};
    std::uint32_t instance_id{0};
};

local_endpoint_id _resolve_local_endpoint(const match::table_ptr& table)
{
#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
    if (auto* shared = dynamic_cast<match::shared_table*>(table.get())) {
        return {shared->local_endpoint_id(), shared->local_instance_id()};
    }
#else
    (void)table;
#endif
    return {0U, 0U};
}

struct endpoint_key {
    std::uint16_t endpoint_id;
    std::uint32_t instance_id;
    bool          operator==(const endpoint_key& o) const noexcept
    {
        return endpoint_id == o.endpoint_id && instance_id == o.instance_id;
    }
};

struct endpoint_key_hash {
    std::size_t operator()(const endpoint_key& k) const noexcept
    {
        return (static_cast<std::size_t>(k.instance_id) << 16) ^ static_cast<std::size_t>(k.endpoint_id);
    }
};

using target_pairs = std::vector<std::pair<mc::string, match::match_id>>;

std::unordered_map<endpoint_key, target_pairs, endpoint_key_hash>
_group_targets_by_endpoint(const std::vector<match::target>& targets)
{
    std::unordered_map<endpoint_key, target_pairs, endpoint_key_hash> groups;
    for (const auto& t : targets) {
        groups[endpoint_key{t.endpoint_id, t.instance_id}].emplace_back(t.service_name, t.id);
    }
    return groups;
}

message _attach_match_ids(const message& msg, const target_pairs& pairs)
{
    message cloned = msg;
    match::set_target_match_ids(cloned.header, pairs);
    return cloned;
}

void _push_to_proto_tree(service_proto& root, const message& msg)
{
    try {
        mc::proto::proto_request req;
        auto&                    ctx = req.ensure_context<service_proto::message_context>(&root);
        ctx.msg                      = msg;
        (void)root.push(req);
    } catch (...) {
    }
}

} // namespace

void service::emit(const message& msg) const
{
    message m = msg;
    if (m.header.sender.empty()) {
        m.header.sender = m_name;
    }

    auto table = engine::get_match_table();
    if (!table) {
        return;
    }

    auto targets = table->find_targets(m);
    if (targets.empty()) {
        return;
    }

    auto groups   = _group_targets_by_endpoint(targets);
    auto local_ep = _resolve_local_endpoint(table);

    service_proto* proto = nullptr;
    {
        std::lock_guard lock(m_impl->m_mutex);
        proto = m_impl->m_proto;
    }

    for (const auto& [key, pairs] : groups) {
        bool is_local = (key.endpoint_id == 0U) ||
                        (key.endpoint_id == local_ep.endpoint_id && key.instance_id == local_ep.instance_id);
        if (is_local) {
            engine::route_inbound(_attach_match_ids(m, pairs));
            continue;
        }
        auto merged = _attach_match_ids(m, pairs);
        bool sent   = false;
#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
        if (auto* endpoint = engine::get_endpoint_service()) {
            sent = endpoint->send_to_endpoint(key.endpoint_id, key.instance_id, merged);
        }
#endif
        if (!sent && proto != nullptr) {
            _push_to_proto_tree(*proto, merged);
        }
    }
}

void service::register_object(abstract_object& obj)
{
    m_impl->ensure_registered();

    if (!obj.has_valid_id()) {
        obj.set_object_id(m_impl->m_object_table->generate_available_id());
    }

    auto path = obj.get_object_path();
    MC_ASSERT(!path.empty(), "object path is empty");
    MC_ASSERT(mc::engine::path::is_valid(path), "invalid object path ${path}", ("path", path));

    m_impl->register_object(obj);

    bool started = false;
    {
        std::lock_guard lock(m_impl->m_mutex);
        started = m_impl->m_started;
    }
    // 未启动时不发 InterfacesAdded。
    if (!started) {
        return;
    }

    emit(_make_interfaces_added(m_name, obj));
}

void service::unregister_object(mc::string_view path)
{
    if (!m_impl->m_object_table) {
        return;
    }

    // 先构造再注销：需要从 table 里读 metadata。
    if (auto it = m_impl->m_object_table->find<by_path>(path); !it.is_end()) {
        const auto& obj = *it;
        emit(_make_interfaces_removed(m_name, obj));
    }

    m_impl->unregister_object(path);
}

service_object_table& service::get_object_table() const
{
    const_cast<service*>(this)->m_impl->ensure_registered();
    return *m_impl->m_object_table;
}

std::size_t service::gc_isolated()
{
    if (!m_impl || !m_impl->m_object_table) {
        return 0U;
    }
    return shm_binding::gc_isolated(m_impl->m_shm_state, *m_impl->m_object_table);
}

namespace {} // namespace

void service::set_proto(service_proto* proto)
{
    service_proto* previous = nullptr;
    {
        std::lock_guard lock(m_impl->m_mutex);
        if (m_impl->m_proto == proto) {
            return;
        }
        previous        = m_impl->m_proto;
        m_impl->m_proto = proto;
    }

    if (previous != nullptr) {
        previous->clear_inbound_handler();
    }

    if (proto == nullptr) {
        return;
    }

    proto->set_inbound_handler([this](message request) -> message {
        return mc::engine::dispatch(*this, request);
    });
}

service_proto* service::get_proto() const
{
    std::lock_guard lock(m_impl->m_mutex);
    return m_impl->m_proto;
}

match::match_id service::add_match(match::match_rule rule, match::filter_spec spec,
                                   match::match_callback callback) const
{
    MC_ASSERT_THROW(callback, mc::invalid_arg_exception, "service::add_match 需要非空 callback");

    auto table = engine::get_match_table();
    MC_ASSERT_THROW(table, mc::invalid_arg_exception, "engine match table 未初始化");

    auto rule_snapshot = rule;
    auto id            = table->subscribe(m_name, std::move(rule), std::move(spec), std::move(callback));

    {
        std::lock_guard lock(m_impl->m_match_mutex);
        m_impl->m_owned_matches.insert(id);
    }
    on_match_added(id, rule_snapshot);
    return id;
}

void service::remove_match(match::match_id id) const
{
    if (id == 0) {
        return;
    }

    bool owned = false;
    {
        std::lock_guard lock(m_impl->m_match_mutex);
        owned = m_impl->m_owned_matches.erase(id) > 0;
    }
    if (!owned) {
        return;
    }
    on_match_removed(id);
    if (auto table = engine::get_match_table()) {
        table->unsubscribe(id);
    }
}

void service::dispatch_event(const message& msg) const
{
    auto table = engine::get_match_table();
    if (!table) {
        return;
    }

    auto pairs = match::get_target_match_ids(msg.header);
    if (!pairs.empty()) {
        for (const auto& p : pairs) {
            if (p.first != m_name) {
                continue;
            }
            auto cb = table->lookup_callback(m_name, p.second);
            if (!cb) {
                continue;
            }
            try {
                cb(msg);
            } catch (...) {
            }
        }
        return;
    }

    // Fallback：自发自收路径，走 find_targets 按名过滤。
    auto targets = table->find_targets(msg);
    if (targets.empty()) {
        return;
    }
    for (const auto& tgt : targets) {
        if (tgt.service_name != m_name) {
            continue;
        }
        auto cb = table->lookup_callback(tgt.service_name, tgt.id);
        if (!cb) {
            continue;
        }
        try {
            cb(msg);
        } catch (...) {
        }
    }
}

mc::string service::resolve_object_path(mc::string_view path_pattern, const abstract_object& obj)
{
    mc::string resolved_path;
    if (!mc::engine::engine::resolve_path_template(mc::string_view(path_pattern), obj, resolved_path)) {
        resolved_path = mc::string(path_pattern);
    }

    mc::string path = resolved_path;
    mc::string::trim_inplace(path);
    if (!path.empty() && path.front() == '/') {
        return path;
    }

    auto parent = mc::static_pointer_cast<abstract_object>(obj.get_parent());
    MC_ASSERT_THROW(parent, mc::invalid_arg_exception, "object parent is nullptr or not abstract_object");
    auto parent_path = parent->get_object_path();

    mc::string tmp;
    tmp.reserve(parent_path.size() + 1 + path.size());
    tmp = mc::string(parent_path);
    tmp += "/";
    tmp += path;
    return tmp;
}

message service::send_with_reply(message request, mc::milliseconds timeout) const
{
    return async_send_with_reply(std::move(request), timeout).get();
}

mc::future<message> service::async_send_with_reply(message request, mc::milliseconds timeout) const
{
    // 同进程：查全局 object_table 找目标对象
    if (request.header.type == message_type::method_call && !request.header.destination.empty()) {
        auto* target_svc = engine::find_service(request.header.destination);
        if (target_svc != nullptr) {
            return mc::resolve(mc::engine::dispatch(*target_svc, request));
        }
    }

    // 跨进程：通过 protocol 栈
    auto* proto = get_proto();
    if (proto == nullptr) {
        MC_THROW(mc::method_call_exception, "无法发送消息: 无 protocol 且目标服务不在本进程 (${destination})",
                 ("destination", request.header.destination));
    }

    if (request.header.sender.empty()) {
        request.header.sender = m_name;
    }
    return proto->async_send_with_reply(std::move(request), timeout);
}

void service::send(message request) const
{
    auto* proto = get_proto();
    if (proto == nullptr) {
        return;
    }

    if (request.header.sender.empty()) {
        request.header.sender = m_name;
    }
    proto->send(std::move(request));
}

mc::variant service::call(mc::string_view path, mc::string_view service_name, mc::string_view interface_name,
                          mc::string_view method_name, const mc::variants& args, mc::string_view signature,
                          mc::milliseconds timeout) const
{
    message request;
    request.header.type           = message_type::method_call;
    request.header.destination    = mc::string(service_name);
    request.header.sender         = m_name;
    request.header.path           = mc::string(path);
    request.header.interface_name = mc::string(interface_name);
    request.header.member_name    = mc::string(method_name);
    request.body                  = make_payload<method_call_payload>(signature, args);

    auto response = send_with_reply(std::move(request), timeout);

    if (response.header.type == message_type::error) {
        if (auto* payload = response.try_as<error_payload>(); payload != nullptr) {
            MC_THROW(mc::method_call_exception, "远端调用失败: ${name}: ${message}",
                     ("name", payload->name)("message", payload->message));
        }
        MC_THROW(mc::method_call_exception, "远端调用失败: ${name}", ("name", response.header.error_name));
    }

    if (response.header.type != message_type::method_return) {
        MC_THROW(mc::method_call_exception, "远端调用没有返回 method_return");
    }

    auto* payload = response.try_as<method_return_payload>();
    if (payload == nullptr) {
        MC_THROW(mc::method_call_exception, "远端 method_return 缺少 payload");
    }
    return payload->value;
}

} // namespace mc::engine
