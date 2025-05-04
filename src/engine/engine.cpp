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
#include <mc/engine/service.h>
#include <thread>
namespace mdb = mc::db;

namespace mc::engine {
using object_tree          = mdb::table<abstract_object, mdb::indexed_by<path_index>>;
using object_tree_ptr      = std::shared_ptr<object_tree>;
using table_connection_map = std::multimap<std::string, mc::connection_type>;
using thread_pool          = std::list<std::thread>;
using work_guard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

struct engine::engine_impl {
public:
    engine_impl();
    ~engine_impl();

    void add_object(abstract_object* object);
    void remove_object(abstract_object* object);
    void update_object(abstract_object* old_object, abstract_object* new_object);

    std::mutex                  m_mutex;
    mdb::database               m_database;
    object_tree_ptr             m_object_tree;
    boost::asio::io_context     m_io_context;
    table_connection_map        m_connections;
    thread_pool                 m_threads;
    std::mutex                  m_threads_mutex;
    std::unique_ptr<work_guard> m_work;
};

engine::engine_impl::engine_impl() : m_object_tree(std::make_shared<object_tree>("object_tree")) {
    m_database.register_table(std::dynamic_pointer_cast<mdb::table_base>(m_object_tree));
    m_work = std::make_unique<work_guard>(m_io_context.get_executor());
}

engine::engine_impl::~engine_impl() {
    m_object_tree->clear();
}

void engine::engine_impl::add_object(abstract_object* object) {
    std::lock_guard lock(m_mutex);

    m_object_tree->add(mc::im::cast<abstract_object>(object));
}

void engine::engine_impl::remove_object(abstract_object* object) {
    std::lock_guard lock(m_mutex);

    m_object_tree->get<by_path>().remove(object->get_object_path());
}

void engine::engine_impl::update_object(abstract_object* old_object, abstract_object* new_object) {
    std::lock_guard lock(m_mutex);

    auto& idx = m_object_tree->get<by_path>();
    auto  it  = idx.find(old_object->get_object_path());
    if (it == idx.end()) {
        idx.remove(old_object->get_object_path());
        m_object_tree->add(mc::im::cast<abstract_object>(new_object));
        return;
    }

    idx.update(*it, mc::im::cast<abstract_object>(new_object));
}

engine::engine() {
    m_impl = std::make_shared<engine_impl>();
}

engine::~engine() {
    stop();
    m_impl.reset();
}

engine& engine::get_instance() {
    return mc::singleton<engine>::instance_with_creator([]() {
        return new engine();
    });
}

void engine::start(std::size_t thread_num) {
    std::lock_guard lock(m_impl->m_mutex);

    if (thread_num == 0) {
        thread_num = std::thread::hardware_concurrency();
    }

    for (std::size_t i = 0; i < thread_num; ++i) {
        m_impl->m_threads.emplace_back([this]() {
            m_impl->m_io_context.run();
        });
    }
}

void engine::join() {
    auto impl = m_impl;

    std::unique_lock lock(impl->m_threads_mutex);
    for (auto& t : impl->m_threads) {
        t.join();
    }
    impl->m_threads.clear();
}

void engine::stop() {
    {
        std::lock_guard lock(m_impl->m_mutex);

        m_impl->m_work.reset();
        m_impl->m_io_context.stop();
    }

    join();
}

mc::db::database& engine::get_database() {
    return m_impl->m_database;
}

io_context_type& engine::get_io_context() {
    return m_impl->m_io_context;
}

bool engine::register_table(mc::db::table_ptr table) {
    if (!table || table->get_table_name().empty()) {
        return false;
    }

    std::lock_guard lock(m_impl->m_mutex);

    auto c1 = table->on_object_added.connect([this](mdb::object_base* object) {
        m_impl->add_object(dynamic_cast<abstract_object*>(object));
    });

    auto c2 = table->on_object_removed.connect([this](mdb::object_base* object) {
        m_impl->remove_object(dynamic_cast<abstract_object*>(object));
    });

    auto c3 = table->on_object_updated.connect(
        [this](mdb::object_base* old_object, mdb::object_base* new_object) {
            m_impl->update_object(dynamic_cast<abstract_object*>(old_object),
                                  dynamic_cast<abstract_object*>(new_object));
        });

    std::string_view table_name = table->get_table_name();
    m_impl->m_connections.emplace(table_name, c1);
    m_impl->m_connections.emplace(table_name, c2);
    m_impl->m_connections.emplace(table_name, c3);

    m_impl->m_database.register_table(table);
    return true;
}

mc::db::table_ptr engine::find_table(std::string_view table_name) {
    std::lock_guard lock(m_impl->m_mutex);

    return m_impl->m_database.get_table(table_name);
}

void engine::unregister_table(mc::db::table_ptr table) {
    if (!table || table->get_table_name().empty()) {
        return;
    }

    std::lock_guard lock(m_impl->m_mutex);

    std::string table_name(table->get_table_name());
    m_impl->m_database.unregister_table(table->get_table_name());
}

} // namespace mc::engine
