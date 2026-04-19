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
#include <mc/engine/object.h>
#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
#include <mc/engine/abstract_object_factory.h>
#include <mc/engine/shm_object_ops.h>
#include <mc/engine/shm_runtime_provider.h>
#endif
#include <thread>

namespace mc::engine {
using object_table_ptr     = std::shared_ptr<object_table>;
using table_connection_map = std::multimap<std::string, mc::connection_type>;
using thread_list          = std::list<std::thread>;

namespace {

// 构造全局 object_table；USE_SHM=ON 时绑定 shm_runtime allocator + 注入
// extractor / reconstruct。
object_table_ptr _create_global_object_table()
{
#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
    auto&          rt = shm_runtime_provider::instance();
    engine_alloc_t alloc(rt, "object_table");
    auto           tbl = std::make_shared<object_table>(alloc);

    auto& eng = tbl->engine();
    eng.set_shm_handle_extractor(
        [](const abstract_object& o) noexcept { return o.get_shm_handle(); });
    eng.set_reconstruct(&try_reconstruct_object);
    return tbl;
#else
    return std::make_shared<object_table>();
#endif
}

}  // namespace

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

engine::engine_impl::engine_impl() : m_object_table(_create_global_object_table())
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

void engine::reset_for_test()
{
    mc::singleton<engine>::reset_for_test();
#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
    // 清掉 SHM region，下一次 lazy 创建从干净状态开始（避免上一个 case 的残留 entry
    // 经 _bind_all 后被当成新数据），保证测试间隔离。
    shm_runtime_provider::reset_for_test();
#endif
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

} // namespace mc::engine
