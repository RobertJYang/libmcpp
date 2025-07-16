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

static void unique_sort(std::vector<std::string>& paths) {
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
}

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
        return new factory_ptr(new reflection_factory(
            global_namespace::factory_name, "global_namespace", true));
    });
}

factory_ptr reflection_factory::try_global_ptr() {
    auto p = mc::singleton<factory_ptr, global_namespace>::try_get();
    return p ? *p : factory_ptr();
}

void reflection_factory::reset_global() {
    mc::singleton<factory_ptr, global_namespace>::reset_for_test();
}

reflection_factory::reflection_factory(
    std::string_view factory_name, std::string_view factory_type_name, bool is_global)
    : m_impl(std::make_unique<impl>(factory_name, factory_type_name, is_global)) {
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
    std::vector<std::string> types;
    m_impl->m_data.with_rlock([&](auto& data) {
        m_impl->get_registered_types(data, types);
    });
    unique_sort(types);
    return types;
}

std::vector<std::string> reflection_factory::get_module_types(std::string_view module_path) const {
    if (module_path.empty()) {
        module_path = get_factory_name();
    }

    auto result = remove_prefix_if_matches(module_path, get_factory_name());
    if (!result) {
        return {};
    }

    std::vector<std::string> types;
    m_impl->m_data.with_rlock([&](auto& data) {
        auto* node = m_impl->find_module_node(*result, data);
        if (!node) {
            return;
        }

        std::string path = make_full_path(get_factory_name(), *result);
        node->collect_module_types(path, types);
    });
    unique_sort(types);
    return types;
}

std::vector<std::string> reflection_factory::get_module_paths() const {
    auto paths = m_impl->m_data.with_rlock([&](auto& data) {
        std::vector<std::string> paths;
        m_impl->collect_module_paths(paths, data);
        return paths;
    });
    unique_sort(paths);
    return paths;
}

bool reflection_factory::has_module(std::string_view module_path) const {
    auto result = remove_prefix_if_matches(module_path, get_factory_name());
    if (!result) {
        return {};
    }

    return m_impl->m_data.with_rlock([&](auto& data) {
        return m_impl->find_module_node(*result, data) != nullptr;
    });
}

factory_id_type reflection_factory::register_factory(factory_ptr factory) {
    if (!factory) {
        return INVALID_FACTORY_ID;
    }

    auto sub_name = factory->get_factory_name();
    if (sub_name.empty()) {
        wlog("注册反射模块失败：子模块名不能为空");
        return INVALID_FACTORY_ID;
    }

    // TODO:: 需要检查循环依赖

    auto result = remove_prefix_if_matches(sub_name, get_factory_name());
    if (!result) {
        wlog("注册反射模块失败：子模块 ${sub_name} 必须以当前模块 ${prefix} 作为前缀",
             ("sub_name", sub_name)("prefix", get_factory_name()));
        return INVALID_FACTORY_ID;
    } else if (result->empty()) {
        wlog("注册反射模块失败：子模块名不能和当前模块名相同, 子模块名=${sub_name}",
             ("sub_name", sub_name));
        return INVALID_FACTORY_ID;
    }

    return factory->m_impl->m_data.with_lock_ptr([&](auto lock_ptr) {
        // 如果子模块有父工厂，先从父工厂注销
        if (auto parent = lock_ptr->m_parent.lock()) {
            if (parent.get() == this) {
                // 如果子模块和当前模块是同一个工厂，直接返回工厂ID，什么都不需要处理
                return lock_ptr->m_factory_id;
            }

            // 解锁当前工厂，再从父工厂注销，防止出现死锁
            auto unlocker = lock_ptr.scoped_unlock();
            parent->unregister_factory(factory->m_impl->m_factory_name);
            lock_ptr->m_parent.reset();
        }

        // 去掉模块名前缀后，注册到当前模块命名空间中
        auto ret = m_impl->register_factory(*result, lock_ptr->m_factory_id, factory);
        if (ret.first == INVALID_FACTORY_ID) {
            if (ret.second) {
                wlog("注册反射模块失败：子模块名=${sub_name} 已存在, 已存在模块命名空间类型=${namespace_type_name}",
                     ("sub_name", sub_name)("namespace_type_name", ret.second->get_namespace_type_name()));
            } else {
                wlog("注册反射模块失败：子模块名=${sub_name} 已存在", ("sub_name", sub_name));
            }
            return INVALID_FACTORY_ID;
        }

        lock_ptr->m_parent = shared_from_this();
        dlog("注册反射模块成功：当前模块名=${factory_name}, 子模块名=${sub_name}, 模块ID=${factory_id}",
             ("factory_name", m_impl->get_pretty_name()) // 当前模块名
             ("sub_name", sub_name)                      // 子模块名
             ("factory_id", ret.first));                 // 模块ID
        return ret.first;
    });
}

void reflection_factory::unregister_factory(std::string_view factory_name) {
    auto result = remove_prefix_if_matches(factory_name, get_factory_name());
    if (!result) {
        return;
    }

    auto factory_id = m_impl->unregister_factory(*result);
    if (factory_id != INVALID_FACTORY_ID) {
        dlog("注销反射模块: 当前模块名=${factory_name}, 子模块名=${sub_name}",
             ("factory_name", m_impl->get_pretty_name()) // 当前模块名
             ("sub_name", factory_name));                // 子模块名

        on_factory_unregister(factory_id);
    }
}

factory_ptr reflection_factory::get_factory(std::string_view factory_name) const {
    auto result = remove_prefix_if_matches(factory_name, get_factory_name());
    if (!result) {
        return nullptr;
    }

    return m_impl->m_data.with_rlock([&](auto& data) -> factory_ptr {
        return m_impl->get_factory_by_name(*result, data);
    });
}

factory_ptr reflection_factory::get_factory_by_id(factory_id_type factory_id) const {
    return m_impl->m_data.with_rlock([&](auto& data) -> factory_ptr {
        return m_impl->get_factory_by_id(factory_id, data);
    });
}

factory_ptr reflection_factory::get_parent_factory() const {
    return m_impl->m_data.with_rlock([&](auto& data) -> factory_ptr {
        return data.m_parent.lock();
    });
}

std::vector<std::string> reflection_factory::get_factory_names() const {
    return m_impl->m_data.with_rlock([&](auto& data) {
        std::vector<std::string> names;
        m_impl->collect_factory_names(get_factory_name(), names, data);
        return names;
    });
}

const std::string& reflection_factory::get_factory_name() const {
    return m_impl->m_factory_name;
}

const std::string& reflection_factory::get_namespace_type_name() const {
    return m_impl->m_namespace_type_name;
}

factory_id_type reflection_factory::get_factory_id() const {
    return m_impl->m_data.unsafe_get_data().m_factory_id;
}

type_id_type reflection_factory::register_type_impl(
    std::string_view                           type_name,
    type_id_type                               old_type_id,
    std::function<reflection_metadata_ptr()>&& creator) {
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
    auto type_id = m_impl->register_type(*result, old_type_id, std::move(creator));
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

    auto type_id = m_impl->unregister_type(*result);
    if (type_id != INVALID_TYPE_ID) {
        dlog("注销类型: 当前模块名=${factory_name}, 类型名=${type_name}",
             ("factory_name", m_impl->get_pretty_name()) // 当前模块名
             ("type_name", type_name));                  // 类型名

        on_type_unregister(type_id);
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