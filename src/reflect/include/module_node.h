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
#ifndef MC_REFLECT_MODULE_NODE_H
#define MC_REFLECT_MODULE_NODE_H

#include <mc/exception.h>
#include <mc/log.h>
#include <mc/reflect/reflection.h>
#include <mc/reflect/reflection_factory.h>
#include <mc/singleton.h>
#include <mc/string.h>
#include <mc/sync/mutex_box.h>
#include <mc/sync/shared_mutex.h>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace mc::reflect {
using split_iterator = mc::string::split_iterator;

static constexpr std::string_view delims = ".:";
static std::atomic<int32_t>       s_factory_id{1};

inline type_id_type encode_type_id(factory_id_type factory_id, local_type_id_type type_id) {
    return (static_cast<type_id_type>(factory_id) << 32) | type_id;
}

inline std::pair<factory_id_type, local_type_id_type> decode_type_id(type_id_type id) {
    return {static_cast<factory_id_type>(id >> 32),
            static_cast<local_type_id_type>(id & 0xFFFFFFFF)};
}

inline std::string make_full_path(std::string_view path, std::string_view name) {
    if (path.empty()) {
        return std::string(name);
    } else if (name.empty()) {
        return std::string(path);
    } else {
        return mc::string::concat(path, ".", name);
    }
}

using metadata_creator = std::function<reflection_metadata_ptr()>;
struct type_info {
    type_info(local_type_id_type id, std::string name, metadata_creator&& creator)
        : type_id(id), type_name(std::move(name)), creator(std::move(creator)) {
    }

    type_info(const type_info& other)                = delete;
    type_info& operator=(const type_info& other)     = delete;
    type_info(type_info&& other) noexcept            = default;
    type_info& operator=(type_info&& other) noexcept = default;

    local_type_id_type       type_id;
    std::string              type_name; // 类型名，不包含模块名前缀
    reflection_metadata_wptr metadata;
    metadata_creator         creator;
};

struct factory_info {
    factory_info(factory_id_type id, std::string name, factory_ptr f)
        : factory_id(id), module_name(std::move(name)), factory(f) {
    }

    factory_info(const factory_info& other)                = delete;
    factory_info& operator=(const factory_info& other)     = delete;
    factory_info(factory_info&& other) noexcept            = default;
    factory_info& operator=(factory_info&& other) noexcept = default;

    factory_id_type factory_id;
    std::string     module_name; // 工厂在本模块中的名称，不包含当前模块名前缀
    factory_wptr    factory;
};

/**
 * @brief 模块节点结构
 *
 * 用于构建模块树，支持层次化的类型组织
 * 叶子节点可以包含本地类型或引用到子模块
 */
struct module_node {
    using submodules_map = std::unordered_map<std::string_view, std::unique_ptr<module_node>>;
    using types_map      = std::unordered_map<std::string_view, type_info*>;

    module_node(std::string_view name) : name(name) {
    }

    module_node()                              = default;
    module_node(const module_node&)            = delete;
    module_node& operator=(const module_node&) = delete;
    module_node(module_node&&)                 = default;
    module_node& operator=(module_node&&)      = default;

    bool is_empty() const {
        return types.empty() && submodules.empty() && sub_factory == nullptr;
    }

    void register_type(split_iterator& type_it, type_info* info);
    bool unregister_type(split_iterator& type_it);

    void register_factory(split_iterator& type_it, factory_info* factory);
    bool unregister_factory(split_iterator& type_it);

    reflection_metadata_ptr get_metadata(std::string_view type_name) const;
    reflection_metadata_ptr get_metadata(split_iterator& type_it) const;

    void collect_module_paths(std::string& path, std::vector<std::string>& paths) const;
    void collect_sub_factory_module_paths(std::string& path, std::vector<std::string>& paths) const;

    void collect_module_types(std::string& path, std::vector<std::string>& types) const;

    const module_node* find_module_node(split_iterator& type_it) const;
    factory_ptr        find_factory(split_iterator& type_it) const;

    module_node* new_module_node(std::string_view name);

    std::string           name;                 // 当前模块名称
    submodules_map        submodules;           // 本地子模块
    mutable types_map     types;                // 该模块下直接包含的类型
    mutable factory_info* sub_factory{nullptr}; // 其他子模块
};

} // namespace mc::reflect

#endif // MC_REFLECT_MODULE_NODE_H