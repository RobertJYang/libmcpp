/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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

namespace mc::db {
static std::atomic<object_id_type> m_next_id{1}; ///< 下一个可用的对象ID

object_id_type table_base::generate_id()
{
    return m_next_id.fetch_add(1, std::memory_order_relaxed);
}

database::database()
{
}

database::~database()
{
}

void database::register_table(table_ptr table)
{
    MC_ASSERT(!table->get_table_name().empty(), "Table name is empty");

    std::lock_guard lock(m_mutex);
    table->set_table_id(++m_next_table_id);
    m_tables[table->get_table_name()]  = table;
    m_table_ids[table->get_table_id()] = table;
}

void database::unregister_table(std::string_view table_name)
{
    std::lock_guard lock(m_mutex);

    auto it = m_tables.find(table_name);
    if (it == m_tables.end()) {
        return;
    }

    m_table_ids.erase(it->second->get_table_id());
    m_tables.erase(it);
}

bool database::is_table_registered(std::string_view table_name) const
{
    std::lock_guard lock(m_mutex);

    return m_tables.find(table_name) != m_tables.end();
}

table_ptr database::get_table(std::string_view table_name) const
{
    std::lock_guard lock(m_mutex);

    auto it = m_tables.find(table_name);
    if (it == m_tables.end()) {
        return nullptr;
    }
    return it->second;
}

object_ptr database::add(std::string_view table_name, const mc::dict& var, transaction* txn)
{
    std::lock_guard lock(m_mutex);

    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return nullptr;
    }
    return table->second->add_object(var, txn);
}

bool database::remove(std::string_view table_name, const query_builder& builder, transaction* txn)
{
    std::lock_guard lock(m_mutex);

    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return false;
    }
    return table->second->remove_object(builder, txn);
}

bool database::empty(std::string_view table_name) const
{
    std::lock_guard lock(m_mutex);

    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return false;
    }
    return table->second->empty();
}

size_t database::size(std::string_view table_name) const
{
    std::lock_guard lock(m_mutex);

    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return 0;
    }
    return table->second->size();
}

void database::clear(std::string_view table_name)
{
    std::lock_guard lock(m_mutex);

    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return;
    }
    table->second->clear();
}

bool database::update(std::string_view table_name, const query_builder& builder,
                      const mc::dict& values, transaction* txn)
{
    std::lock_guard lock(m_mutex);

    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return false;
    }
    return table->second->update_object(builder, values, txn);
}

bool database::update(std::string_view table_name, const query_builder& builder,
                      const std::map<std::string, variant>& values, transaction* txn)
{
    std::lock_guard lock(m_mutex);

    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return false;
    }

    return table->second->update_object(builder, values, txn);
}

} // namespace mc::db
