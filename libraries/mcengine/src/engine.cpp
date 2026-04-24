/*
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

#include <mc/engine/engine.h>
#include <mc/engine/internal/shm_binding.h>
#include <mc/engine/match.h>
#include <mc/engine/object.h>
#include <mc/engine/service.h>
#include <mc/log/log.h>
#include <mc/string.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "match/local_table.h"

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
#include <mc/shm/shm_runtime.h>

#include "match/shared_table.h"
#include "shm_runtime_provider.h"
#endif

namespace mc::engine {
using object_table_ptr     = std::shared_ptr<object_table>;
using table_connection_map = std::multimap<std::string, mc::connection_type>;
using thread_list          = std::list<std::thread>;

struct engine::engine_impl {
public:
    engine_impl();
    ~engine_impl();

    void add_object(abstract_object& object);
    void remove_object(abstract_object& object);
    void update_object(abstract_object& old_object, abstract_object& new_object);

    std::mutex           m_mutex;
    mdb::database        m_database;
    object_table_ptr     m_object_table;
    table_connection_map m_connections;
};

engine::engine_impl::engine_impl() : m_object_table(shm_binding::create_global_object_table())
{
    m_database.register_table(m_object_table);
}

engine::engine_impl::~engine_impl()
{
    m_object_table->clear();
}

void engine::engine_impl::add_object(abstract_object& object)
{
    std::lock_guard lock(m_mutex);

    auto object_id = object.get_object_id();
    if (object_id > 0 && !m_object_table->find_by_object_id(object_id).is_end()) {
        return;
    }

    m_object_table->add(object_ptr(&object));
}

void engine::engine_impl::remove_object(abstract_object& object)
{
    std::lock_guard lock(m_mutex);

    m_object_table->remove(object_ptr(&object));
}

void engine::engine_impl::update_object(abstract_object& old_object, abstract_object& new_object)
{
    std::lock_guard lock(m_mutex);

    auto it = m_object_table->find<by_path>(old_object.get_object_path());
    if (it.is_end()) {
        m_object_table->add(object_ptr(&new_object));
        return;
    }

    m_object_table->update(object_ptr(const_cast<abstract_object*>(&(*it))), object_ptr(&new_object));
}

engine::engine()
{
    m_impl = std::make_shared<engine_impl>();
}

engine::~engine()
{
    m_impl.reset();
}

engine& engine::get_instance()
{
    return mc::singleton<engine>::instance_with_creator([]() {
        return new engine();
    });
}

namespace {

struct match_table_holder {
    std::mutex       mutex;
    match::table_ptr table;

    static match_table_holder& instance()
    {
        static match_table_holder h;
        return h;
    }

    match::table_ptr ensure_default()
    {
        std::lock_guard lock(mutex);
        if (!table) {
            table = _create_default_table();
        }
        return table;
    }

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
    static match::table_ptr _create_default_table()
    {
        try {
            auto& runtime = shm_runtime_provider::instance();
            return mc::make_shared<match::shared_table>(runtime, 0U, 0U);
        } catch (...) {
            return mc::make_shared<match::local_table>();
        }
    }
#else
    static match::table_ptr _create_default_table()
    {
        return mc::make_shared<match::local_table>();
    }
#endif

    void replace(match::table_ptr new_table)
    {
        std::lock_guard lock(mutex);
        table = std::move(new_table);
    }

    void reset()
    {
        std::lock_guard lock(mutex);
        table.reset();
    }
};

struct service_registry {
    std::mutex                                mutex;
    std::unordered_map<std::string, service*> by_name;

    static service_registry& instance()
    {
        static service_registry r;
        return r;
    }

    void add(service* svc)
    {
        if (!svc) {
            return;
        }
        std::lock_guard lock(mutex);
        by_name[std::string(svc->name())] = svc;
    }

    void remove(service* svc)
    {
        if (!svc) {
            return;
        }
        std::lock_guard lock(mutex);
        auto            it = by_name.find(std::string(svc->name()));
        if (it != by_name.end() && it->second == svc) {
            by_name.erase(it);
        }
    }

    service* find(mc::string_view name) const
    {
        std::lock_guard lock(const_cast<std::mutex&>(mutex));
        auto            it = by_name.find(std::string(name));
        return it == by_name.end() ? nullptr : it->second;
    }

    void reset()
    {
        std::lock_guard lock(mutex);
        by_name.clear();
    }
};

} // namespace

void engine::reset_for_test()
{
    service_registry::instance().reset();
    mc::singleton<engine>::reset_for_test();
    // match 状态与 SHM 状态需要一并复位，否则上一个 case 的订阅会顺着单例
    // 漂到下一个 case；filter 编译缓存同理。
    match_table_holder::instance().reset();
    match::filter_backend_registry::reset_for_test();
    // 清掉 SHM region，下一次 lazy 创建从干净状态开始（避免上一个 case 的残留 entry
    // 经 _bind_all 后被当成新数据），保证测试间隔离。OFF 模式下是 no-op。
    shm_binding::reset_runtime_for_test();
}

void engine::reset_after_fork()
{
    // fork 后子进程：忘掉继承的 service_registry / match_table，但不动
    // SHM endpoint 状态（那是 mcapp 的 endpoint_service 负责，engine 不感知）。
    service_registry::instance().reset();
    mc::singleton<engine>::reset_for_test();
    match_table_holder::instance().reset();
    match::filter_backend_registry::reset_for_test();
    shm_binding::reset_runtime_for_test();
}

match::table_ptr engine::get_match_table()
{
    return match_table_holder::instance().ensure_default();
}

void engine::set_match_table(match::table_ptr table)
{
    match_table_holder::instance().replace(std::move(table));
}

void engine::add_filter_backend(match::filter_backend_ptr backend)
{
    match::filter_backend_registry::instance().register_backend(std::move(backend));
}

match::filter_backend_ptr engine::find_filter_backend(std::uint32_t backend_type)
{
    return match::filter_backend_registry::instance().find(backend_type);
}

mc::db::database& engine::get_database()
{
    return m_impl->m_database;
}

bool engine::register_table(mc::db::table_ptr table)
{
    if (!table || table->get_table_name().empty()) {
        return false;
    }

    std::lock_guard lock(m_impl->m_mutex);

    auto c1 = table->on_object_added.connect([this](mdb::object_base& object) {
        m_impl->add_object(static_cast<abstract_object&>(object));
    });

    auto c2 = table->on_object_removed.connect([this](mdb::object_base& object) {
        m_impl->remove_object(static_cast<abstract_object&>(object));
    });

    auto c3 = table->on_object_updated.connect([this](mdb::object_base& old_object, mdb::object_base& new_object) {
        m_impl->update_object(static_cast<abstract_object&>(old_object), static_cast<abstract_object&>(new_object));
    });

    std::string_view table_name = table->get_table_name();
    m_impl->m_connections.emplace(table_name, c1);
    m_impl->m_connections.emplace(table_name, c2);
    m_impl->m_connections.emplace(table_name, c3);

    m_impl->m_database.register_table(table);
    return true;
}

mc::db::table_ptr engine::find_table(mc::string_view table_name)
{
    std::lock_guard lock(m_impl->m_mutex);

    return m_impl->m_database.get_table(table_name);
}

void engine::unregister_table(mc::db::table_ptr table)
{
    if (!table || table->get_table_name().empty()) {
        return;
    }

    std::lock_guard lock(m_impl->m_mutex);

    std::string table_name(table->get_table_name());
    m_impl->m_database.unregister_table(table->get_table_name());
}

object_table& engine::get_object_table()
{
    return *m_impl->m_object_table;
}

void engine::register_service(service* svc)
{
    service_registry::instance().add(svc);
}

void engine::unregister_service(service* svc)
{
    service_registry::instance().remove(svc);
}

service* engine::find_service(mc::string_view name)
{
    return service_registry::instance().find(name);
}

namespace {

void _dispatch_to_service(service* svc, const message& msg)
{
    if (svc == nullptr) {
        return;
    }
    svc->dispatch_event(msg);
}

} // namespace

void engine::route_inbound(const message& msg)
{
    auto pairs = match::get_target_match_ids(msg.header);
    if (!pairs.empty()) {
        // 合并派发路径：同一条物理消息可能命中本进程的多个 (service, match_id)。
        // 按 service 分组，每个 service 只 dispatch 一次（dispatch_event 内部会
        // 遍历属于自己的所有 id 并逐个触发 callback）。
        std::unordered_map<std::string, std::size_t> seen_services;
        for (const auto& p : pairs) {
            seen_services[std::string(p.first)]++;
        }
        for (const auto& [name, _] : seen_services) {
            _dispatch_to_service(service_registry::instance().find(name), msg);
        }
        return;
    }

    // 回退：单 id wire 或未携带 ids。广播到所有已注册 service，让 service 自己
    // 的 dispatch_event 按 m_name + fast/fallback 过滤。
    //
    // 为避免 find()/remove() 与广播并发死锁，先拷贝一份 service 列表再迭代。
    std::vector<service*> services;
    {
        auto&           reg = service_registry::instance();
        std::lock_guard lock(reg.mutex);
        services.reserve(reg.by_name.size());
        for (auto& kv : reg.by_name) {
            services.push_back(kv.second);
        }
    }
    for (auto* svc : services) {
        _dispatch_to_service(svc, msg);
    }
}

} // namespace mc::engine
