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
#ifndef MC_REFLECT_REFLECTION_FACTORY_IMPL_H
#define MC_REFLECT_REFLECTION_FACTORY_IMPL_H

#include "reflect/include/module_node.h"

namespace mc::reflect {
using split_iterator = mc::string::split_iterator;

class reflection_factory::impl {
    using type_info_map    = std::unordered_map<std::string_view, std::unique_ptr<type_info>>;
    using type_info_id_map = std::unordered_map<local_type_id_type, type_info*>;
    using factories_map    = std::unordered_map<std::string_view, std::unique_ptr<factory_info>>;
    using factories_id_map = std::unordered_map<factory_id_type, factory_info*>;

    struct data_t {
        type_info_map    m_type_infos;
        type_info_id_map m_type_infos_by_id;

        mutable factories_map m_factories;
        factories_id_map      m_factories_by_id;
        factory_wptr          m_parent;

        local_type_id_type           m_next_type_id{0};
        factory_id_type              m_factory_id{0};
        std::unique_ptr<module_node> m_root_module;
    };
    using data_box = mc::mutex_box<data_t, std::shared_mutex>;

public:
    std::string m_factory_name;
    std::string m_namespace_type_name;
    data_box    m_data;

    explicit impl(std::string_view factory_name, std::string_view factory_type_name, bool is_global);
    ~impl();

    type_id_type register_type(
        std::string_view type_name, type_id_type type_id, metadata_creator&& creator);
    type_id_type unregister_type(std::string_view type_name);

    std::pair<factory_id_type, factory_ptr> register_factory(
        std::string_view module_name, factory_id_type factory_id, factory_ptr factory);
    factory_id_type unregister_factory(std::string_view factory_name);

    reflection_metadata_ptr get_metadata_by_id(type_id_type global_id, const data_t& data);
    reflection_metadata_ptr get_local_metadata_by_id(local_type_id_type type_id, const data_t& data);

    reflection_metadata_ptr get_metadata_by_name(std::string_view type_name, const data_t& data);
    reflection_metadata_ptr get_local_metadata_by_name(std::string_view type_name, const data_t& data);

    factory_ptr get_factory_by_name(std::string_view module_name, const data_t& data);
    factory_ptr get_factory_by_id(factory_id_type factory_id, const data_t& data);
    void        collect_factory_names(
               std::string_view path, std::vector<std::string>& names, const data_t& data) const;

    void get_registered_types(const data_t& data, std::vector<std::string>& types) const;

    const module_node* find_module_node(std::string_view path, const data_t& data) const;
    void               collect_module_paths(std::vector<std::string>& paths, const data_t& data) const;

    const std::string& get_pretty_name() const;
    const std::string& get_namespace_type_name() const;
};

} // namespace mc::reflect

#endif // MC_REFLECT_REFLECTION_FACTORY_IMPL_H