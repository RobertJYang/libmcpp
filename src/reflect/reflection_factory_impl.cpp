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
static std::string global_name = "global";

static std::string normalize_type_name(std::string_view type_name) {
    std::vector<std::string> parts;
    for (auto it = split_iterator(type_name, delims); !it.is_end(); ++it) {
        parts.push_back(std::string(*it));
    }

    return mc::string::join(parts, ".");
}

// 由于我们支持 factory 单例销毁，这里需要一个全局变量来记录之前注册的类型分配过的 factory_id
using used_factory_ids_map = std::unordered_map<std::string_view, factory_id_type>;
static mc::mutex_box<used_factory_ids_map, std::mutex> s_used_factory_ids;
constexpr size_t                                       s_max_used_factory_ids = 100000;

static factory_id_type make_factory_id(std::string_view factory_name) {
    return s_used_factory_ids.with_lock([factory_name](auto& map) {
        auto it = map.find(factory_name);
        if (it != map.end()) {
            return it->second;
        }
        auto id = s_factory_id.fetch_add(1);
        if (map.size() <= s_max_used_factory_ids) {
            // 这个映射关系没有释放机制，所以限定最大数量避免恶意操作造成内存泄露。
            // 因为反射工厂的构造机制确保 factory_name 是一个静态编译期常量，无法运行时扩充，所以
            // 这个方式还是比较安全的
            map.emplace(factory_name, id);
        }
        return id;
    });
}

reflection_factory::impl::impl(
    std::string_view factory_name, std::string_view namespace_type_name, bool is_global)
    : m_factory_name(normalize_type_name(factory_name)), m_namespace_type_name(namespace_type_name) {
    auto& data         = m_data.unsafe_get_data();
    data.m_root_module = std::make_unique<module_node>();
    if (!is_global) {
        data.m_factory_id = make_factory_id(factory_name);
    }
}

reflection_factory::impl::~impl() {
    m_data.with_lock_ptr([&](auto lock_ptr) {
        auto& data = *lock_ptr;
        data.m_type_infos_by_id.clear();
        data.m_type_infos.clear();
        data.m_factories.clear();
        data.m_factories_by_id.clear();
        data.m_root_module.reset();

        if (auto parent = data.m_parent.lock()) {
            auto unlocker = lock_ptr.scoped_unlock();
            parent->unregister_factory(m_factory_name);
        }
        data.m_parent.reset();
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
    std::string_view module_name, const data_t& data) {
    // 快速路径：找到立即返回
    auto it = data.m_factories.find(module_name);
    if (it != data.m_factories.end()) {
        return it->second->factory.lock();
    }

    // 如果找不到，则从模块树中查找
    split_iterator type_it(module_name, delims);
    return data.m_root_module->find_factory(type_it);
}

factory_ptr reflection_factory::impl::get_factory_by_id(
    factory_id_type factory_id, const data_t& data) {
    auto it = data.m_factories_by_id.find(factory_id);
    if (it == data.m_factories_by_id.end()) {
        return nullptr;
    }

    return it->second->factory.lock();
}

void reflection_factory::impl::collect_factory_names(
    std::string_view path, std::vector<std::string>& names, const data_t& data) const {
    for (auto& [_, factory_info] : data.m_factories) {
        auto factory = factory_info->factory.lock();
        if (!factory) {
            continue;
        }

        names.emplace_back(make_full_path(path, factory_info->module_name));
        factory->m_impl->m_data.with_rlock([&](auto& data) {
            factory->m_impl->collect_factory_names(names.back(), names, data);
        });
    }
}

type_id_type reflection_factory::impl::register_type(
    std::string_view type_name, type_id_type type_id, metadata_creator&& creator) {
    return m_data.with_lock([&](auto& data) {
        // 构造规范化的类型名
        // 注意：规范化后的类型名的字符串视图被作为 key 存储在 map 中，
        // 所以索引规范化名称必须 move 到 map 中，否则会导致字符串视图失效
        std::string name = normalize_type_name(type_name);

        auto it = data.m_type_infos.find(name);
        if (it != data.m_type_infos.end()) {
            return INVALID_TYPE_ID;
        }

        int  new_id    = type_id == INVALID_TYPE_ID ? ++data.m_next_type_id : type_id;
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

type_id_type reflection_factory::impl::unregister_type(std::string_view type_name) {
    return m_data.with_lock([&](auto& data) {
        std::string name = normalize_type_name(type_name);

        auto it = data.m_type_infos.find(name);
        if (it == data.m_type_infos.end()) {
            return INVALID_TYPE_ID;
        }

        // 一定要先从模块树中移除，因为 module_node 用了 type_info 的 name 视图作为索引
        split_iterator type_it(name, delims);
        data.m_root_module->unregister_type(type_it);

        // 从类型信息中移除
        auto type_id = it->second->type_id;
        data.m_type_infos_by_id.erase(type_id);
        data.m_type_infos.erase(it);

        return encode_type_id(data.m_factory_id, type_id);
    });
}

std::pair<factory_id_type, factory_ptr> reflection_factory::impl::register_factory(
    std::string_view module_name, factory_id_type factory_id, factory_ptr factory) {
    return m_data.with_lock([&](auto& data) -> std::pair<factory_id_type, factory_ptr> {
        auto it = data.m_factories.find(module_name);
        if (it != data.m_factories.end()) {
            return {INVALID_FACTORY_ID, it->second->factory.lock()};
        }

        auto it_by_id = data.m_factories_by_id.find(factory_id);
        if (it_by_id != data.m_factories_by_id.end()) {
            return {INVALID_FACTORY_ID, it_by_id->second->factory.lock()};
        }

        auto info = std::make_unique<factory_info>(
            factory_id,
            std::string(module_name),
            std::move(factory));
        auto name_view = std::string_view(info->module_name);
        auto ret       = data.m_factories.emplace(
            std::piecewise_construct, std::forward_as_tuple(name_view),
            std::forward_as_tuple(std::move(info)));
        data.m_factories_by_id[factory_id] = ret.first->second.get();

        // 注册到模块树中
        split_iterator type_it(name_view, delims);
        data.m_root_module->register_factory(type_it, ret.first->second.get());

        return {factory_id, factory};
    });
}

factory_id_type reflection_factory::impl::unregister_factory(std::string_view factory_name) {
    return m_data.with_lock([&](auto& data) {
        auto it = data.m_factories.find(factory_name);
        if (it == data.m_factories.end()) {
            return INVALID_FACTORY_ID;
        }

        auto factory_id = it->second->factory_id;
        data.m_factories_by_id.erase(factory_id);
        data.m_factories.erase(it);

        // 从模块树中移除
        split_iterator type_it(factory_name, delims);
        data.m_root_module->unregister_factory(type_it);

        return factory_id;
    });
}

void reflection_factory::impl::get_registered_types(
    const data_t& data, std::vector<std::string>& types) const {
    std::string path(m_factory_name);
    data.m_root_module->collect_module_types(path, types);
}

const module_node* reflection_factory::impl::find_module_node(std::string_view path, const data_t& data) const {
    if (path.empty()) {
        return data.m_root_module.get();
    }

    split_iterator type_it(path, delims);
    return data.m_root_module->find_module_node(type_it);
}

void reflection_factory::impl::collect_module_paths(
    std::vector<std::string>& paths, const data_t& data) const {
    std::string path(m_factory_name);
    data.m_root_module->collect_module_paths(path, paths);
}

const std::string& reflection_factory::impl::get_pretty_name() const {
    return m_factory_name.empty() ? global_name : m_factory_name;
}

const std::string& reflection_factory::impl::get_namespace_type_name() const {
    return m_namespace_type_name;
}

} // namespace mc::reflect