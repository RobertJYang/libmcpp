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

static std::optional<std::string_view> remove_prefix_if_matches(std::string_view text, std::string_view prefix) {
    if (prefix.empty()) {
        return text;
    }

    bool is_first_match = false;
    auto text_it        = split_iterator(text, delims);
    auto prefix_it      = split_iterator(prefix, delims);
    while (!text_it.is_end() && !prefix_it.is_end()) {
        if (*text_it != *prefix_it) {
            if (is_first_match) {
                return std::nullopt; // 部分前缀匹配，不允许
            }

            return text; // 第一个就不匹配，允许，整个放到 prefix 命名空间之下
        }

        ++text_it;
        ++prefix_it;
        is_first_match = true;
    }

    if (!prefix_it.is_end()) {
        return std::nullopt; // 不允许，竟然在当前命名空间之上
    }

    if (text_it.is_end()) {
        return std::string_view{}; // 完全匹配，返回一个空字符串，实际也不允许，因为不允许空类型名
    }

    return text_it.tail();
}

reflection_factory& reflection_factory::global() {
    return *global_ptr();
}

factory_ptr& reflection_factory::global_ptr() {
    return mc::singleton<factory_ptr, global_namespace>::instance_with_creator([]() {
        return new factory_ptr(new reflection_factory(global_namespace::factory_name, true));
    });
}

factory_ptr reflection_factory::try_global_ptr() {
    auto p = mc::singleton<factory_ptr, global_namespace>::try_get();
    return p ? *p : factory_ptr();
}

reflection_factory::reflection_factory(std::string_view factory_name, bool is_global)
    : m_impl(std::make_unique<impl>(factory_name, is_global)) {
}

reflection_factory::~reflection_factory() = default;

reflection_metadata_ptr reflection_factory::get_metadata(type_id_type type_id) {
    return m_impl->m_data.with_rlock([&](auto& data) {
        return m_impl->get_metadata_by_id(type_id, data);
    });
}

reflection_metadata_ptr reflection_factory::get_metadata(std::string_view type_name) {
    auto result = remove_prefix_if_matches(type_name, get_factory_name());
    if (!result) {
        return nullptr;
    }

    return m_impl->m_data.with_rlock([&](auto& data) {
        return m_impl->get_metadata_by_name(*result, data);
    });
}

reflected_object_ptr reflection_factory::try_create_object(type_id_type type_id) {
    return m_impl->m_data.with_rlock([&](auto& data) -> reflected_object_ptr {
        auto metadata = m_impl->get_metadata_by_id(type_id, data);
        if (!metadata) {
            return nullptr;
        }

        return metadata->create_object();
    });
}

reflected_object_ptr reflection_factory::try_create_object(std::string_view type_name) {
    auto result = remove_prefix_if_matches(type_name, get_factory_name());
    if (!result) {
        return nullptr;
    }

    return m_impl->m_data.with_rlock([&](auto& data) -> reflected_object_ptr {
        auto metadata = m_impl->get_metadata_by_name(*result, data);
        if (!metadata) {
            return nullptr;
        }

        return metadata->create_object();
    });
}

reflected_object_ptr reflection_factory::create_object(type_id_type type_id) {
    auto obj = try_create_object(type_id);
    if (!obj) {
        MC_THROW(mc::bad_type_exception, "类型不存在: ${type_id}", ("type_id", type_id));
    }
    return obj;
}

reflected_object_ptr reflection_factory::create_object(std::string_view type_name) {
    auto obj = try_create_object(type_name);
    if (!obj) {
        MC_THROW(mc::bad_type_exception, "类型不存在: ${type_name}", ("type_name", type_name));
    }
    return obj;
}

type_id_type reflection_factory::get_type_id(std::string_view type_name) const {
    auto result = remove_prefix_if_matches(type_name, get_factory_name());
    if (!result) {
        return INVALID_TYPE_ID;
    }

    return m_impl->m_data.with_rlock([&](auto& data) {
        auto metadata = m_impl->get_metadata_by_name(*result, data);
        if (!metadata) {
            return INVALID_TYPE_ID;
        }

        return metadata->get_type_id();
    });
}

std::vector<std::string> reflection_factory::get_registered_types() const {
    return m_impl->m_data.with_rlock([&](auto& data) {
        std::vector<std::string> types;
        m_impl->get_registered_types(data, types, std::string(get_factory_name()));
        return types;
    });
}

std::vector<std::pair<std::string, type_id_type>>
reflection_factory::get_module_types(std::string_view module_path) const {
    return m_impl->m_data.with_rlock([&](auto& data) -> std::vector<std::pair<std::string, type_id_type>> {
        auto* node = m_impl->find_module_node(module_path, *data.m_root_module);
        if (!node) {
            return {};
        }

        return node->get_module_types(data.m_factory_id);
    });
}

std::vector<std::string> reflection_factory::get_module_paths() const {
    return m_impl->m_data.with_rlock([&](auto& data) {
        std::vector<std::string> paths;
        m_impl->collect_module_paths(
            data, *data.m_root_module, std::string(get_factory_name()), paths);
        return paths;
    });
}

bool reflection_factory::has_module(std::string_view module_path) const {
    return m_impl->m_data.with_rlock([&](auto& data) {
        return m_impl->find_module_node(module_path, *data.m_root_module) != nullptr;
    });
}

factory_id_type reflection_factory::register_factory(factory_ptr factory) {
    auto sub_factory_name = factory->get_factory_name();
    if (sub_factory_name.empty()) {
        wlog("注册反射模块失败：子模块名不能为空");
        return INVALID_FACTORY_ID;
    }

    auto result = remove_prefix_if_matches(sub_factory_name, get_factory_name());
    if (!result) {
        wlog("注册反射模块失败：子模块名=${sub_factory_name} 不匹配当前模块名=${factory_name}",
             ("sub_factory_name", sub_factory_name)("factory_name", get_factory_name()));
        return INVALID_FACTORY_ID;
    } else if (result->empty()) {
        wlog("注册反射模块失败：子模块名不能和当前模块名相同, 子模块名=${sub_factory_name}",
             ("sub_factory_name", sub_factory_name));
        return INVALID_FACTORY_ID;
    }

    // 去掉模块名前缀后，注册到当前模块命名空间中
    auto factory_id = m_impl->register_factory(*result, factory);
    if (factory_id == INVALID_FACTORY_ID) {
        wlog("注册反射模块失败：子模块名=${sub_factory_name} 已存在", ("sub_factory_name", sub_factory_name));
        return INVALID_FACTORY_ID;
    }

    dlog("注册反射模块成功：当前模块名=${factory_name}, 子模块名=${sub_factory_name}, 模块ID=${factory_id}",
         ("factory_name", m_impl->get_pretty_name()) // 当前模块名
         ("sub_factory_name", sub_factory_name)      // 子模块名
         ("factory_id", factory_id));                // 模块ID
    return factory_id;
}

void reflection_factory::unregister_factory(std::string_view factory_name) {
    if (m_impl->unregister_factory(factory_name)) {
        dlog("注销反射模块: 当前模块名=${factory_name}, 子模块名=${sub_factory_name}",
             ("factory_name", m_impl->get_pretty_name()) // 当前模块名
             ("sub_factory_name", factory_name));        // 子模块名
    }
}

factory_ptr reflection_factory::get_factory(std::string_view factory_name) const {
    return m_impl->m_data.with_rlock([&](auto& data) -> factory_ptr {
        auto it = data.m_factories.find(factory_name);
        return it != data.m_factories.end() ? it->second->factory.lock() : nullptr;
    });
}

std::vector<std::string_view> reflection_factory::get_factory_names() const {
    return m_impl->m_data.with_rlock([&](auto& data) {
        std::vector<std::string_view> names;
        names.reserve(data.m_factories.size());
        for (const auto& [name, _] : data.m_factories) {
            names.push_back(name);
        }
        return names;
    });
}

std::string_view reflection_factory::get_factory_name() const {
    return m_impl->m_factory_name;
}

type_id_type reflection_factory::register_type_impl(std::string_view type_name, std::function<reflection_metadata_ptr()>&& creator) {
    if (type_name.empty()) {
        wlog("注册类型失败：类型名不能为空");
        return INVALID_TYPE_ID;
    }

    auto result = remove_prefix_if_matches(type_name, get_factory_name());
    if (!result) {
        wlog("注册类型失败：类型名=${type_name} 不匹配模块名=${factory_name}",
             ("type_name", type_name)("factory_name", get_factory_name()));
        return INVALID_TYPE_ID;
    } else if (result->empty()) {
        wlog("注册类型失败：类型名不能和模块名相同, 类型名=${type_name}",
             ("type_name", type_name));
        return INVALID_TYPE_ID;
    }

    // 去掉模块名前缀后，注册到当前模块命名空间中
    auto type_id = m_impl->register_type(*result, std::move(creator));
    if (type_id == INVALID_TYPE_ID) {
        wlog("注册类型失败：类型名=${type_name} 已存在", ("type_name", type_name));
        return INVALID_TYPE_ID;
    }

    dlog("注册类型成功：模块名=${factory_name}, 类型名=${type_name}, 类型ID=${type_id}",
         ("factory_name", get_factory_name())("type_name", *result)("type_id", type_id));
    return type_id;
}

void reflection_factory::unregister_type_impl(std::string_view type_name) {
    if (type_name.empty()) {
        return;
    }

    auto result = remove_prefix_if_matches(type_name, get_factory_name());
    if (!result || result->empty()) {
        return;
    }

    if (m_impl->unregister_type(*result)) {
        dlog("注销类型: 当前模块名=${factory_name}, 类型名=${type_name}",
             ("factory_name", m_impl->get_pretty_name()) // 当前模块名
             ("type_name", type_name));                  // 类型名
    }
}

// 全局便利函数实现
reflected_object_ptr try_create_object(type_id_type type_id) {
    return reflection_factory::global().try_create_object(type_id);
}

reflected_object_ptr try_create_object(std::string_view type_name) {
    return reflection_factory::global().try_create_object(type_name);
}

reflected_object_ptr create_object(type_id_type type_id) {
    return reflection_factory::global().create_object(type_id);
}

reflected_object_ptr create_object(std::string_view type_name) {
    return reflection_factory::global().create_object(type_name);
}

} // namespace mc::reflect