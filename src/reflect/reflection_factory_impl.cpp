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
#include "reflect/include/reflection_factory_impl.h"

namespace mc::reflect {
static constexpr std::string_view global_name = "global";

static std::string normalize_type_name(std::string_view type_name) {
    std::vector<std::string> parts;
    for (auto it = split_iterator(type_name, delims); !it.is_end(); ++it) {
        parts.push_back(std::string(*it));
    }

    return mc::string::join(parts, ".");
}

static std::string make_full_type_name(std::string_view factory_name, std::string_view type_name) {
    if (factory_name.empty()) {
        return std::string(type_name);
    }
    return mc::string::concat(factory_name, ".", type_name);
}

reflection_factory::impl::impl(std::string_view factory_name, bool is_global)
    : m_factory_name(factory_name) {
    auto& data         = m_data.unsafe_get_data();
    data.m_root_module = std::make_unique<module_node>();
    if (!is_global) {
        data.m_factory_id = s_factory_id.fetch_add(1);
    }
}

reflection_factory::impl::~impl() {
    m_data.with_lock([](auto& data) {
        data.m_type_infos_by_id.clear();
        data.m_type_infos.clear();
        data.m_factories.clear();
        data.m_factories_by_id.clear();
        data.m_root_module.reset();
    });
}

reflection_metadata_ptr reflection_factory::impl::get_metadata_by_id(
    type_id_type global_id, const data_t& data) {
    auto [factory_id, type_id] = decode_type_id(global_id);
    if (factory_id == data.m_factory_id) {
        // 类型在本模块中
        return get_local_metadata_by_id(type_id, data);
    }

    // 类型在其他模块中
    auto factory = get_factory_by_id(factory_id, data);
    if (!factory) {
        return nullptr;
    }

    return factory->get_metadata(global_id);
}

reflection_metadata_ptr reflection_factory::impl::get_local_metadata_by_id(
    local_type_id_type type_id, const data_t& data) {
    auto it = data.m_type_infos_by_id.find(type_id);
    if (it == data.m_type_infos_by_id.end()) {
        // 类型不存在
        return nullptr;
    }

    // 如果 metadata 存在且有效则返回
    auto* info = it->second;
    if (auto ptr = info->metadata.lock()) {
        return ptr;
    }

    return nullptr;
}

reflection_metadata_ptr reflection_factory::impl::get_metadata_by_name(
    std::string_view type_name, const data_t& data) {
    // 如果类型在本模块中
    auto ptr = get_local_metadata_by_name(type_name, data);
    if (ptr) {
        return ptr;
    }

    // 如果类型不在本模块中，则从模块树中查找
    split_iterator type_it(type_name, delims);
    return data.m_root_module->get_metadata(type_it);
}

reflection_metadata_ptr reflection_factory::impl::get_local_metadata_by_name(
    std::string_view type_name, const data_t& data) {
    auto it = data.m_type_infos.find(type_name);
    if (it == data.m_type_infos.end()) {
        return nullptr;
    }

    auto* info = it->second.get();
    if (!info->metadata && info->creator) {
        // 延迟初始化 metadata
        info->metadata = info->creator();
        info->creator  = nullptr;
    }

    return info->metadata.lock();
}

factory_ptr reflection_factory::impl::get_factory_by_name(
    std::string_view factory_name, const data_t& data) {
    auto it = data.m_factories.find(factory_name);
    if (it == data.m_factories.end()) {
        return nullptr;
    }

    return it->second->factory.lock();
}

factory_ptr reflection_factory::impl::get_factory_by_id(
    factory_id_type factory_id, const data_t& data) {
    auto it = data.m_factories_by_id.find(factory_id);
    if (it == data.m_factories_by_id.end()) {
        return nullptr;
    }

    return it->second->factory.lock();
}

type_id_type reflection_factory::impl::register_type(
    std::string_view type_name, metadata_creator&& creator) {
    return m_data.with_lock([&](auto& data) {
        // 构造规范化的类型名
        // 注意：规范化后的类型名的字符串视图被作为 key 存储在 map 中，
        // 所以索引规范化名称必须 move 到 map 中，否则会导致字符串视图失效
        std::string name = normalize_type_name(type_name);

        auto it = data.m_type_infos.find(name);
        if (it != data.m_type_infos.end()) {
            return INVALID_TYPE_ID;
        }

        int  new_id    = ++data.m_next_type_id;
        auto info      = std::make_unique<type_info>(new_id, std::move(name), std::move(creator));
        auto name_view = std::string_view(info->type_name);
        auto ret       = data.m_type_infos.emplace(
            std::piecewise_construct, std::forward_as_tuple(name_view),
            std::forward_as_tuple(std::move(info)));
        data.m_type_infos_by_id[new_id] = ret.first->second.get();

        // 注册到模块树中
        split_iterator type_it(name_view, delims);
        data.m_root_module->register_type(type_it, ret.first->second.get());

        return encode_type_id(data.m_factory_id, new_id);
    });
}

bool reflection_factory::impl::unregister_type(std::string_view type_name) {
    return m_data.with_lock([&](auto& data) {
        std::string name = normalize_type_name(type_name);

        auto it = data.m_type_infos.find(name);
        if (it == data.m_type_infos.end()) {
            return false;
        }

        // 一定要先从模块树中移除，因为 module_node 用了 type_info 的 name 视图作为索引
        split_iterator type_it(name, delims);
        data.m_root_module->unregister_type(type_it);

        // 从类型信息中移除
        auto type_id = it->second->type_id;
        data.m_type_infos_by_id.erase(type_id);
        data.m_type_infos.erase(it);

        return true;
    });
}

factory_id_type reflection_factory::impl::register_factory(std::string_view factory_name, factory_ptr factory) {
    return m_data.with_lock([&](auto& data) {
        // 构造规范化的类型名
        // 注意：规范化后的类型名的字符串视图被作为 key 存储在 map 中，
        // 所以索引规范化名称必须 move 到 map 中，否则会导致字符串视图失效
        std::string name       = normalize_type_name(factory_name);
        auto        factory_id = factory->m_impl->m_data.rlock()->m_factory_id;

        auto it = data.m_factories.find(name);
        if (it != data.m_factories.end()) {
            return INVALID_FACTORY_ID;
        }

        auto it_by_id = data.m_factories_by_id.find(factory_id);
        if (it_by_id != data.m_factories_by_id.end()) {
            return INVALID_FACTORY_ID;
        }

        auto info      = std::make_unique<factory_info>(factory_id, std::move(name), std::move(factory));
        auto name_view = std::string_view(info->factory_name);

        auto ret = data.m_factories.emplace(
            std::piecewise_construct, std::forward_as_tuple(name_view),
            std::forward_as_tuple(std::move(info)));
        data.m_factories_by_id[factory_id] = ret.first->second.get();

        // 注册到模块树中
        split_iterator type_it(name_view, delims);
        data.m_root_module->register_factory(type_it, ret.first->second.get());

        return factory_id;
    });
}

bool reflection_factory::impl::unregister_factory(std::string_view factory_name) {
    return m_data.with_lock([&](auto& data) {
        auto it = data.m_factories.find(factory_name);
        if (it == data.m_factories.end()) {
            return false;
        }

        data.m_factories.erase(it);

        // 从模块树中移除
        split_iterator type_it(factory_name, delims);
        data.m_root_module->unregister_factory(type_it);

        return true;
    });
}

void reflection_factory::impl::get_registered_types(
    const data_t& data, std::vector<std::string>& types, const std::string& prefix) const {
    for (const auto& [name, _] : data.m_type_infos) {
        types.push_back(make_full_type_name(prefix, name));
    }

    foreach_factory(data, [&](factory_ptr factory) {
        factory->m_impl->m_data.with_rlock([&](auto& sub_data) {
            auto sub_prefix = make_full_type_name(prefix, factory->get_factory_name());
            get_registered_types(sub_data, types, sub_prefix);
        });
    });
}

void reflection_factory::impl::foreach_factory(
    const data_t& data, std::function<void(factory_ptr)> callback) const {
    for (auto it = data.m_factories.begin(); it != data.m_factories.end();) {
        if (auto factory = it->second->factory.lock()) {
            callback(factory);
            ++it;
        }
    }
}

module_node* reflection_factory::impl::find_module_node(std::string_view path, module_node& root) const {
    if (path.empty()) {
        return &root;
    }

    module_node* current = &root;
    for (auto it = split_iterator(path, delims); it != split_iterator::end(); ++it) {
        auto sub_it = current->submodules.find(*it);
        if (sub_it == current->submodules.end()) {
            return nullptr;
        }
        current = sub_it->second.get();
    }

    return current;
}

bool reflection_factory::impl::is_leaf_node(const module_node& node) const {
    // 如果节点有类型，则是叶子节点
    return !node.types.empty();
}

void reflection_factory::impl::collect_module_paths(
    const data_t&             data,
    const module_node&        node,
    const std::string&        current_path,
    std::vector<std::string>& paths) const {
    // 如果当前是叶子节点，则添加当前路径
    if (is_leaf_node(node)) {
        paths.push_back(current_path);
    }

    // 递归遍历子模块
    for (const auto& [name, subnode] : node.submodules) {
        std::string child_path = current_path.empty()
                                     ? std::string(name)
                                     : mc::string::concat(current_path, ".", name);
        collect_module_paths(data, *subnode, child_path, paths);
    }

    // 收集子模块的模块路径
    foreach_factory(data, [&](factory_ptr factory) {
        factory->m_impl->m_data.with_rlock([&](auto& sub_data) {
            factory->m_impl->collect_module_paths(
                sub_data, *sub_data.m_root_module, current_path, paths);
        });
    });
}

std::string_view reflection_factory::impl::get_pretty_name() const {
    return m_factory_name.empty() ? global_name : m_factory_name;
}

} // namespace mc::reflect