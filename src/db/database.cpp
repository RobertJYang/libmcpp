#include <mc/db/database.h>

namespace mc {
namespace db {

database::database() {
}

database::~database() {
}

void database::register_table(table_ptr table) {
    MC_ASSERT(!table->get_table_name().empty(), "Table name is empty");

    table->set_table_id(++m_next_table_id);
    m_tables[table->get_table_name()]  = table;
    m_table_ids[table->get_table_id()] = table;
}

void database::unregister_table(std::string_view table_name) {
    auto it = m_tables.find(table_name);
    if (it == m_tables.end()) {
        return;
    }

    m_table_ids.erase(it->second->get_table_id());
    m_tables.erase(it);
}

bool database::is_table_registered(std::string_view table_name) const {
    return m_tables.find(table_name) != m_tables.end();
}

table_ptr database::get_table(std::string_view table_name) const {
    auto it = m_tables.find(table_name);
    if (it == m_tables.end()) {
        return nullptr;
    }
    return it->second;
}

const object_base* database::add(std::string_view table_name, const mc::dict& var,
                                 transaction* txn) {
    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return nullptr;
    }
    return table->second->add_object(var, txn);
}

bool database::remove(std::string_view table_name, const query_builder& builder, transaction* txn) {
    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return false;
    }
    return table->second->remove_object(builder, txn);
}

bool database::empty(std::string_view table_name) const {
    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return false;
    }
    return table->second->empty();
}

size_t database::size(std::string_view table_name) const {
    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return 0;
    }
    return table->second->size();
}

void database::clear(std::string_view table_name) {
    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return;
    }
    table->second->clear();
}

bool database::update(std::string_view table_name, const query_builder& builder,
                      const mc::dict& values, transaction* txn) {
    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return false;
    }
    return table->second->update_object(builder, values, txn);
}

bool database::update(std::string_view table_name, const query_builder& builder,
                      const std::map<std::string, variant>& values, transaction* txn) {
    auto table = m_tables.find(table_name);
    if (table == m_tables.end()) {
        return false;
    }
    return table->second->update_object(builder, values, txn);
}

} // namespace db
} // namespace mc
