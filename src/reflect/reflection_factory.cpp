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

#include <mc/exception.h>
#include <mc/log.h>
#include <mc/reflect/reflection_factory.h>
#include <mc/reflect/reflection_metadata.h>
#include <mc/singleton.h>
#include <mc/string.h>

using split_iterator = mc::string::split_iterator;

namespace mc::reflect {

reflection_factory::~reflection_factory() {
    m_data.with_lock([](auto& data) {
        data.m_metadata.clear();
        data.m_metadata_initializers.clear();
        data.m_type_ids.clear();
        data.m_root_module = module_node{};
    });
}

reflection_metadata_ptr reflection_factory::get_metadata(int type_id) {
    return m_data.with_lock([&](auto& data) {
        return get_metadata_inner(type_id, data);
    });
}

reflection_metadata_ptr reflection_factory::get_metadata(std::string_view type_name) {
    return m_data.with_lock([&](auto& data) -> reflection_metadata_ptr {
        auto it = data.m_type_ids.find(type_name);
        if (it == data.m_type_ids.end()) {
            return nullptr;
        }

        return get_metadata_inner(it->second, data);
    });
}

reflection_metadata_ptr reflection_factory::get_metadata_inner(int type_id, data_t& data) {
    auto it = data.m_metadata.find(type_id);
    if (it != data.m_metadata.end()) {
        auto ptr = it->second.lock();
        if (ptr) {
            return ptr;
        }

        // 如果 metadata 被销毁，则删除该项
        data.m_metadata.erase(it);
        data.m_metadata_initializers.erase(type_id);
        return nullptr;
    }

    // 延迟初始化 metadata
    auto init_it = data.m_metadata_initializers.find(type_id);
    if (init_it == data.m_metadata_initializers.end()) {
        return nullptr;
    }

    auto metadata            = init_it->second();
    data.m_metadata[type_id] = metadata;
    return metadata;
}

reflected_object_ptr reflection_factory::create_object(int type_id) {
    auto metadata = get_metadata(type_id);
    if (!metadata) {
        MC_THROW(mc::bad_type_exception, "类型不存在: ${type_id}", ("type_id", type_id));
    }
    return metadata->create_object();
}

reflected_object_ptr reflection_factory::create_object(std::string_view type_name) {
    auto type_id = get_type_id(type_name);
    if (type_id == -1) {
        MC_THROW(mc::bad_type_exception, "类型不存在: ${type_name}", ("type_name", type_name));
    }
    return create_object(type_id);
}

int reflection_factory::get_type_id(std::string_view type_name) const {
    return m_data.with_lock([&](auto& data) {
        auto it = data.m_type_ids.find(type_name);
        return it != data.m_type_ids.end() ? it->second : -1;
    });
}

std::vector<std::string_view> reflection_factory::get_registered_types() const {
    return m_data.with_lock([&](auto& data) {
        std::vector<std::string_view> types;
        for (const auto& [name, _] : data.m_type_ids) {
            types.push_back(name);
        }
        return types;
    });
}

int reflection_factory::register_type(std::string_view type_name, metadata_creator&& creator) {
    return m_data.with_lock([&](auto& data) {
        auto it = data.m_type_ids.find(type_name);
        if (it != data.m_type_ids.end()) {
            wlog("类型已注册: ${type_name}", ("type_name", type_name));
            return it->second;
        }

        int new_id                 = data.m_next_type_id++;
        data.m_type_ids[type_name] = new_id;

        // 延迟初始化 metadata
        data.m_metadata_initializers[new_id] = std::move(creator);
        return new_id;
    });
}

std::vector<std::pair<std::string, int>> reflection_factory::get_module_types(std::string_view module_path) const {
    return m_data.with_lock([&](auto& data) -> std::vector<std::pair<std::string, int>> {
        auto* node = find_module_node(module_path, const_cast<module_node&>(data.m_root_module));
        if (!node) {
            return {};
        }

        std::vector<std::pair<std::string, int>> result;
        result.reserve(node->types.size());
        for (const auto& [name, type_id] : node->types) {
            result.emplace_back(name, type_id);
        }
        return result;
    });
}

std::vector<std::string> reflection_factory::get_all_module_paths() const {
    return m_data.with_lock([&](auto& data) {
        std::vector<std::string> paths;
        collect_module_paths(data.m_root_module, "", paths);
        return paths;
    });
}

bool reflection_factory::has_module(std::string_view module_path) const {
    return m_data.with_lock([&](auto& data) {
        return find_module_node(module_path, const_cast<module_node&>(data.m_root_module)) != nullptr;
    });
}

void reflection_factory::register_to_module_tree(std::string_view full_name, int type_id) {
    m_data.with_lock([&](auto& data) {
        module_node* current_node = &data.m_root_module;
        auto         it           = split_iterator(full_name, ".:");
        while (!it.is_end()) {
            auto name = *it;
            ++it;
            if (it.is_end()) {
                current_node->types.emplace(name, type_id);
                break;
            }

            auto& submodules = current_node->submodules;
            auto  sub_it     = submodules.find(name);
            if (sub_it == submodules.end()) {
                sub_it = submodules.emplace(name, std::make_unique<module_node>()).first;
            }
            current_node = sub_it->second.get();
        }
    });
}

module_node* reflection_factory::find_module_node(std::string_view path, module_node& root) const {
    if (path.empty()) {
        return &root;
    }

    module_node* current = &root;
    for (auto it = split_iterator(path, ".:"); it != split_iterator::end(); ++it) {
        auto sub_it = current->submodules.find(*it);
        if (sub_it == current->submodules.end()) {
            return nullptr;
        }
        current = sub_it->second.get();
    }

    return current;
}

void reflection_factory::collect_module_paths(const module_node&        node,
                                              const std::string&        current_path,
                                              std::vector<std::string>& paths) const {
    // 如果当前是叶子节点(有类型的节点)，则添加当前路径
    if (!node.types.empty() && !current_path.empty()) {
        paths.push_back(current_path);
    }

    // 递归遍历子模块
    for (const auto& [name, subnode] : node.submodules) {
        std::string child_path;
        if (current_path.empty()) {
            child_path = name;
        } else {
            child_path = current_path;
            child_path += ".";
            child_path += name;
        }
        collect_module_paths(*subnode, child_path, paths);
    }
}

// 全局便利函数实现
reflected_object_ptr create_object(int type_id) {
    return reflection_factory::instance().create_object(type_id);
}

reflected_object_ptr create_object(std::string_view type_name) {
    return reflection_factory::instance().create_object(type_name);
}

} // namespace mc::reflect