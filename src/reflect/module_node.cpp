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
#include "reflect/include/module_node.h"
#include "reflect/include/reflection_factory_impl.h"
#include <mc/reflect/reflection_factory.h>

namespace mc::reflect {

static void append_path(std::string& path, std::string_view name) {
    if (!path.empty()) {
        mc::string::append(path, ".", name);
    } else {
        path = name;
    }
}

reflection_metadata_ptr module_node::get_metadata(std::string_view type_name) const {
    auto it = types.find(type_name);
    if (it != types.end()) {
        auto metadata = it->second->metadata.lock();
        if (metadata) {
            return metadata;
        }
    }

    if (!sub_factory) {
        return nullptr;
    }

    auto factory = sub_factory->factory.lock();
    if (!factory) {
        return nullptr;
    }

    return factory->get_metadata(type_name);
}

reflection_metadata_ptr module_node::get_metadata(split_iterator& type_it) const {
    auto name = *type_it;
    ++type_it;
    if (type_it.is_end()) {
        // 最后一节是类型名
        return get_metadata(name);
    }

    // 中间路径是模块名
    auto tail_name = type_it.tail(); // 先将尾名保存下来，避免 type_it 被修改
    auto sub_it    = submodules.find(name);
    if (sub_it != submodules.end()) {
        // 在当前工厂的子模块中查找
        auto ptr = sub_it->second->get_metadata(type_it);
        if (ptr) {
            return ptr;
        }
    }

    // 如果当前工厂的子模块不存在，则尝试从其他工厂中查找
    if (!sub_factory) {
        return nullptr;
    }

    auto factory = sub_factory->factory.lock();
    if (!factory) {
        return nullptr;
    }

    auto ptr = factory->get_metadata(tail_name);
    if (ptr) {
        return ptr;
    }

    return nullptr;
}

void module_node::collect_module_paths(std::string& path, std::vector<std::string>& paths) const {
    auto old_size = path.size();
    if (!name.empty()) {
        // 添加上子模块路径（全局模块名不需要搜集）
        append_path(path, name);
        if (!types.empty() || (sub_factory && sub_factory->factory.lock())) {
            // 如果当前节点有类型或者有子模块，说明当前节点是一个模块，需要搜集
            paths.push_back(path);
        }
    }

    for (const auto& [subname, subnode] : submodules) {
        subnode->collect_module_paths(path, paths);
    }

    collect_sub_factory_module_paths(path, paths);
    path.resize(old_size);
}

void module_node::collect_sub_factory_module_paths(std::string& path, std::vector<std::string>& paths) const {
    if (!sub_factory) {
        return;
    }

    auto factory = sub_factory->factory.lock();
    if (!factory) {
        return;
    }

    factory->m_impl->m_data.with_rlock([&](auto& data) {
        data.m_root_module->collect_module_paths(path, paths);
    });
}

void module_node::collect_module_types(std::string& path, std::vector<std::string>& all) const {
    for (const auto& [name, info] : types) {
        all.push_back(make_full_path(path, name));
    }

    for (const auto& [subname, subnode] : submodules) {
        auto old_size = path.size();
        append_path(path, subname);
        subnode->collect_module_types(path, all);
        path.resize(old_size);
    }

    if (!sub_factory) {
        return;
    }

    auto factory = sub_factory->factory.lock();
    if (!factory) {
        return;
    }

    factory->m_impl->m_data.with_rlock([&](auto& data) {
        data.m_root_module->collect_module_types(path, all);
    });
}

void module_node::register_type(split_iterator& type_it, type_info* info) {
    auto name = *type_it;
    ++type_it;
    if (type_it.is_end()) {
        // 最后一节是类型名
        types.emplace(name, info);
        return;
    }

    // 中间路径是模块名
    module_node* sub_node = nullptr;
    auto         sub_it   = submodules.find(name);
    if (sub_it == submodules.end()) {
        sub_node = new_module_node(name);
    } else {
        sub_node = sub_it->second.get();
    }
    sub_node->register_type(type_it, info);
}

bool module_node::unregister_type(split_iterator& type_it) {
    auto name = *type_it;
    ++type_it;
    if (type_it.is_end()) {
        auto it = types.find(name);
        if (it != types.end()) {
            types.erase(it);
            return true;
        }
        return false;
    }

    auto sub_it = submodules.find(name);
    if (sub_it == submodules.end()) {
        return false;
    }

    auto sub_node = sub_it->second.get();
    if (sub_node->unregister_type(type_it) && sub_node->is_empty()) {
        // 移除空子模块
        submodules.erase(sub_it);
        return true;
    }

    return false;
}

void module_node::register_factory(split_iterator& type_it, factory_info* factory) {
    if (type_it.is_end()) {
        sub_factory = factory;
        return;
    }

    auto         name     = *type_it;
    module_node* sub_node = nullptr;
    auto         sub_it   = submodules.find(name);
    if (sub_it == submodules.end()) {
        sub_node = new_module_node(name);
    } else {
        sub_node = sub_it->second.get();
    }
    ++type_it;
    sub_node->register_factory(type_it, factory);
}

bool module_node::unregister_factory(split_iterator& type_it) {
    auto name = *type_it;
    ++type_it;
    if (type_it.is_end()) {
        sub_factory = nullptr;
        return true;
    }

    auto sub_it = submodules.find(name);
    if (sub_it == submodules.end()) {
        return false;
    }

    auto sub_node = sub_it->second.get();
    if (sub_node->unregister_factory(type_it) && sub_node->is_empty()) {
        // 移除空子模块
        submodules.erase(sub_it);
        return true;
    }

    return false;
}

module_node* module_node::new_module_node(std::string_view name) {
    auto node = std::make_unique<module_node>(name);

    std::string_view name_view = node->name;
    module_node*     ptr       = node.get();
    submodules.emplace(name_view, std::move(node));
    return ptr;
}

const module_node* module_node::find_module_node(split_iterator& type_it) const {
    if (type_it.is_end()) {
        return this;
    }

    auto name = *type_it;
    ++type_it;
    auto tail_name = type_it.tail(); // 先保存尾名，避免 type_it 被修改

    auto sub_it = submodules.find(name);
    if (sub_it != submodules.end()) {
        auto node = sub_it->second->find_module_node(type_it);
        if (node) {
            return node;
        }
    }

    if (!sub_factory) {
        return nullptr;
    }

    auto factory = sub_factory->factory.lock();
    if (!factory) {
        return nullptr;
    }

    return factory->m_impl->m_data.with_rlock([tail_name](auto& data) {
        split_iterator type_it(tail_name, delims);
        return data.m_root_module->find_module_node(type_it);
    });
}

factory_ptr module_node::find_factory(split_iterator& type_it) const {
    if (type_it.is_end()) {
        return sub_factory ? sub_factory->factory.lock() : nullptr;
    }

    auto name      = *type_it;
    auto tail_name = type_it.tail(); // 先保存尾名，避免 type_it 被修改
    auto sub_it    = submodules.find(name);
    if (sub_it != submodules.end()) {
        ++type_it;
        auto factory = sub_it->second->find_factory(type_it);
        if (factory) {
            return factory;
        }
    }

    if (!sub_factory) {
        return nullptr;
    }

    auto factory = sub_factory->factory.lock();
    if (!factory) {
        return nullptr;
    }

    return factory->m_impl->m_data.with_rlock([tail_name](auto& data) {
        split_iterator type_it(tail_name, delims);
        return data.m_root_module->find_factory(type_it);
    });
}

} // namespace mc::reflect