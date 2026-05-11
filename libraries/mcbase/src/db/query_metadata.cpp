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

#include <mc/db/query/metadata.h>

namespace mc::db::query {

void table_index_metadata_base::add_index(const index_metadata& metadata)
{
    auto& md = m_indices.emplace_back(std::make_unique<index_metadata>(metadata));

    for (const auto& field_name : md->field_names) {
        m_field_to_indices[field_name].push_back(m_indices.size() - 1);
    }
}

bool table_index_metadata_base::has_non_unique_index(mc::string_view field_name) const
{
    auto it = m_field_to_indices.find(mc::string(field_name));
    if (it == m_field_to_indices.end()) {
        return false;
    }

    for (size_t index_id : it->second) {
        if (!m_indices[index_id]->is_unique) {
            return true;
        }
    }
    return false;
}

int table_index_metadata_base::find_best_index_id(mc::string_view field_name, bool prefer_unique) const
{
    auto it = m_field_to_indices.find(mc::string(field_name));
    if (it == m_field_to_indices.end()) {
        return -1;
    }

    if (prefer_unique) {
        for (size_t index_id : it->second) {
            if (m_indices[index_id]->is_unique && m_indices[index_id]->index_id > 0) {
                return static_cast<int>(m_indices[index_id]->index_id);
            }
        }
    }

    for (size_t index_id : it->second) {
        if (m_indices[index_id]->index_id > 0) {
            return static_cast<int>(m_indices[index_id]->index_id);
        }
    }

    return -1;
}

const table_index_metadata_base::index_metadata_list& table_index_metadata_base::get_all_indices() const
{
    return m_indices;
}

const index_metadata* table_index_metadata_base::get_index_by_id(size_t index_id) const
{
    if (index_id < m_indices.size()) {
        return m_indices[index_id].get();
    }
    return nullptr;
}

} // namespace mc::db::query
