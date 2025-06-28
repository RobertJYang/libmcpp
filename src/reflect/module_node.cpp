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

namespace mc::reflect {

reflection_metadata_ptr module_node::get_metadata(std::string_view type_name) const {
    auto it = types.find(type_name);
    if (it == types.end()) {
        return nullptr;
    }

    auto metadata = it->second->metadata.lock();
    if (metadata) {
        return metadata;
    }

    return nullptr;
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

std::vector<std::pair<std::string, type_id_type>>
module_node::get_module_types(factory_id_type factory_id) const {
    std::vector<std::pair<std::string, type_id_type>> result;

    // 先添加本模块的类型
    for (const auto& [name, info] : types) {
        result.emplace_back(name, encode_type_id(factory_id, info->type_id));
    }

    // 再添加子模块的类型
    return result;
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
    auto name = *type_it;
    ++type_it;
    if (type_it.is_end()) {
        sub_factory = factory;
        return;
    }

    module_node* sub_node = nullptr;
    auto         sub_it   = submodules.find(name);
    if (sub_it == submodules.end()) {
        sub_node = new_module_node(name);
    } else {
        sub_node = sub_it->second.get();
    }
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

} // namespace mc::reflect