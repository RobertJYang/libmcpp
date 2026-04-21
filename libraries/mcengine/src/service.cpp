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
#include <mc/engine/engine_proto.h>
#include <mc/engine/internal/shm_binding.h>
#include <mc/engine/path.h>
#include <mc/engine/path_iterator.h>
#include <mc/engine/service.h>
#include <mc/engine/utils.h>
#include <mc/exception.h>

namespace mc::engine {

using object_table_ptr = std::shared_ptr<service_object_table>;

struct service_impl {
    std::mutex           m_mutex;
    service*             m_service{nullptr};
    object_table_ptr     m_object_table;
    bool                 m_registered{false};
    bool                 m_started{false};
    service_protocol_ptr m_protocol;
    mc::proto::protocol* m_proto{nullptr};
    // 始终持有；OFF 模式下 create_service_state 返回 nullptr，所有 binding 调用
    // 走 inline no-op。
    shm_binding::service_state* m_shm_state{nullptr};

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
    // recover 后按 path 反查 owner，把 heap 端 owner 关系建上；触发的 set_owner
    // 会通过 shm_binding::sync_owner 顺带把 SHM parent/children 一并刷新。
    // 独立一遍是为了避免 owner-children 顺序敏感。
    void _restore_owner_relations();
};

void service_impl::ensure_registered()
{
    if (m_registered) {
        return;
    }

    if (!m_object_table) {
        m_object_table = shm_binding::create_service_object_table(m_service->name());
    }
    mc::engine::engine::get_instance().register_table(m_object_table);
    m_registered = true;

    shm_binding::attach_and_recover(m_shm_state, m_service->name(), *m_object_table);
    _restore_owner_relations();
    // 上一次崩溃 / 升级遗留下来的 isolated 对象在这里立刻清掉，避免它们持续
    // 占用 user_arena 直到业务侧手动调 gc。
    (void)m_service->gc_isolated();
}

void service_impl::unregister_from_engine()
{
    if (!m_registered || !m_object_table) {
        return;
    }

    // 显式 stop / unregister 走"主动 detach"语义：清掉 SHM 数据并把 shm_service 标
    // detached + pid=0。crash 路径（进程没机会走到这里）下次 attach 时则按 takeover
    // 语义自动接管旧 shm_object。
    m_object_table->clear();
    shm_binding::detach(m_shm_state);
    mc::engine::engine::get_instance().unregister_table(m_object_table);
    m_registered = false;
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
        // set_owner 会通过 shm_binding::sync_owner 把 SHM 的 parent /
        // new_owner.children 一起更新，跨 path 的 ownership 由此自动持久化。
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

    // set_owner(nullptr) 触发 shm_binding::sync_owner，从 SHM 旧 parent.children
    // 中移除本体并把 sh->parent 置 null；随后 release_object_handle 销毁 sh。
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

    service_protocol_ptr protocol;
    {
        std::lock_guard lock(m_impl->m_mutex);
        if (m_impl->m_started) {
            return true;
        }
        m_impl->m_started = true;
        protocol          = m_impl->m_protocol;
    }
    if (protocol) {
        protocol->attach(*this);
    }
    if (on_start()) {
        return true;
    }

    if (protocol) {
        protocol->detach();
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
    service_protocol_ptr protocol;
    if (m_impl) {
        {
            std::lock_guard lock(m_impl->m_mutex);
            if (!m_impl->m_started) {
                m_impl->unregister_from_engine();
                return true;
            }
            protocol = m_impl->m_protocol;
        }
        if (!on_stop()) {
            return false;
        }
        {
            std::lock_guard lock(m_impl->m_mutex);
            m_impl->m_started = false;
        }
        if (protocol) {
            protocol->detach();
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

void service::register_object(abstract_object& obj)
{
    m_impl->ensure_registered();

    if (!obj.has_valid_id()) {
        obj.set_object_id(m_impl->m_object_table->generate_id());
    }

    auto path = obj.get_object_path();
    MC_ASSERT(!path.empty(), "object path is empty");
    MC_ASSERT(mc::engine::path::is_valid(path), "invalid object path ${path}", ("path", path));

    m_impl->register_object(obj);

    service_protocol_ptr protocol;
    {
        std::lock_guard lock(m_impl->m_mutex);
        if (!m_impl->m_started) {
            return;
        }
        protocol = m_impl->m_protocol;
    }
    if (protocol) {
        protocol->register_object(obj);
    }
}

void service::unregister_object(mc::string_view path)
{
    if (!m_impl->m_object_table) {
        return;
    }

    service_protocol_ptr protocol;
    {
        std::lock_guard lock(m_impl->m_mutex);
        if (m_impl->m_started) {
            protocol = m_impl->m_protocol;
        }
    }
    if (protocol) {
        protocol->unregister_object(path);
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

void service::set_protocol(service_protocol_ptr protocol)
{
    service_protocol_ptr previous;
    service_protocol_ptr current;
    bool                 started = false;
    {
        std::lock_guard lock(m_impl->m_mutex);
        if (m_impl->m_protocol == protocol) {
            return;
        }
        previous           = std::move(m_impl->m_protocol);
        m_impl->m_protocol = std::move(protocol);
        current            = m_impl->m_protocol;
        started            = m_impl->m_started;
    }

    if (previous && started) {
        previous->detach();
    }
    if (started && current) {
        current->attach(*this);
    }
}

service_protocol_ptr service::get_protocol() const
{
    std::lock_guard lock(m_impl->m_mutex);
    return m_impl->m_protocol;
}

namespace {

engine_proto* _resolve_engine_proto(mc::proto::protocol* proto)
{
    auto* engine = dynamic_cast<engine_proto*>(proto);
    MC_ASSERT_THROW(engine != nullptr, mc::invalid_arg_exception,
                    "service::set_proto requires mc::engine::engine_proto as root");
    return engine;
}

} // namespace

void service::set_proto(mc::proto::protocol* proto)
{
    mc::proto::protocol* previous = nullptr;
    mc::proto::protocol* current  = nullptr;
    {
        std::lock_guard lock(m_impl->m_mutex);
        if (m_impl->m_proto == proto) {
            return;
        }
        previous        = std::move(m_impl->m_proto);
        m_impl->m_proto = std::move(proto);
        current         = m_impl->m_proto;
    }

    if (auto* prev_engine = dynamic_cast<engine_proto*>(previous); prev_engine != nullptr) {
        prev_engine->clear_inbound_handler();
    }

    if (!current) {
        return;
    }

    auto* engine = _resolve_engine_proto(current);
    engine->set_inbound_handler([this](message request) -> message {
        return mc::engine::dispatch(*this, request);
    });
}

mc::proto::protocol* service::get_proto() const
{
    std::lock_guard lock(m_impl->m_mutex);
    return m_impl->m_proto;
}

mc::variant service::request(const service_operation& operation) const
{
    auto protocol = get_protocol();
    MC_ASSERT_THROW(protocol, mc::invalid_arg_exception, "service protocol is not configured");
    return protocol->request(operation);
}

mc::result<mc::variant> service::async_request(const service_operation& operation) const
{
    auto protocol = get_protocol();
    MC_ASSERT_THROW(protocol, mc::invalid_arg_exception, "service protocol is not configured");
    return protocol->async_request(operation);
}

mc::variant service::timeout_call(mc::milliseconds timeout, mc::string_view endpoint, mc::string_view path,
                                  mc::string_view interface_name, mc::string_view method_name,
                                  mc::string_view signature, mc::variants args, mc::dict context) const
{
    service_operation operation{
        service_operation_kind::invoke_method,
        invoke_method_operation{
            service_operation_target{mc::string(endpoint), mc::string(path), mc::string(interface_name)},
            mc::string(method_name),
            mc::string(signature),
            std::move(args),
            std::move(context),
            timeout,
        },
    };
    return request(operation);
}

mc::result<mc::variant> service::async_timeout_call(mc::milliseconds timeout, mc::string_view endpoint,
                                                    mc::string_view path, mc::string_view interface_name,
                                                    mc::string_view method_name, mc::string_view signature,
                                                    mc::variants args, mc::dict context) const
{
    service_operation operation{
        service_operation_kind::invoke_method,
        invoke_method_operation{
            service_operation_target{mc::string(endpoint), mc::string(path), mc::string(interface_name)},
            mc::string(method_name),
            mc::string(signature),
            std::move(args),
            std::move(context),
            timeout,
        },
    };
    return async_request(operation);
}

uint64_t service::add_match(mc::dbus::match_rule& rule, std::function<void(mc::dbus::message&)>&& cb) const
{
    auto protocol = get_protocol();
    MC_ASSERT_THROW(protocol, mc::invalid_arg_exception, "service protocol is not configured");
    return protocol->add_match(rule, std::move(cb));
}

void service::remove_match(uint64_t id) const
{
    auto protocol = get_protocol();
    MC_ASSERT_THROW(protocol, mc::invalid_arg_exception, "service protocol is not configured");
    protocol->remove_match(id);
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

} // namespace mc::engine
