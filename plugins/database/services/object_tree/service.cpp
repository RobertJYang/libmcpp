/*
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "service.h"
#include "mc/log.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mc::db {

// 对象节点
struct object_node {
    void*                                                         data;
    size_t                                                        size;
    std::unordered_map<std::string, std::unique_ptr<object_node>> children;
};

// 对象树服务实现
class object_tree_service::impl {
public:
    impl() = default;

    // 根节点
    object_node m_root;

    // 线程安全保护
    std::mutex m_mutex;

    // 路径解析
    std::vector<std::string> parse_path(const path& object_path) {
        std::vector<std::string> components;
        std::string              current;

        for (char c : object_path) {
            if (c == '/') {
                if (!current.empty()) {
                    components.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }

        if (!current.empty()) {
            components.push_back(current);
        }

        return components;
    }

    // 查找节点
    object_node* find_node(const std::vector<std::string>& path_components, bool create = false) {
        object_node* current = &m_root;

        for (const auto& component : path_components) {
            if (component.empty()) {
                continue;
            }

            auto it = current->children.find(component);
            if (it == current->children.end()) {
                if (!create) {
                    return nullptr;
                }

                // 创建新节点
                auto new_node  = std::make_unique<object_node>();
                new_node->data = nullptr;
                new_node->size = 0;

                current->children[component] = std::move(new_node);
                current                      = current->children[component].get();
            } else {
                current = it->second.get();
            }
        }

        return current;
    }
};

object_tree_service::object_tree_service(std::string name)
    : service_base(std::move(name)), m_impl(std::make_unique<impl>()) {
}

object_tree_service::~object_tree_service() = default;

bool object_tree_service::init(dict args) {
    set_state(service_state::starting);
    ilog("初始化对象树服务: ${name}", ("name", name()));

    // 读取配置，如有必要
    if (args.contains("dependencies")) {
        m_dependencies = args.at("dependencies").as<std::vector<std::string>>();
    } else {
        // 默认依赖内存管理服务
        m_dependencies = {"memory_manager"};
    }

    set_state(service_state::stopped);
    return true;
}

bool object_tree_service::start() {
    if (get_state() == service_state::running) {
        return true;
    }

    set_state(service_state::starting);
    ilog("启动对象树服务: ${name}", ("name", name()));
    set_state(service_state::running);
    return true;
}

bool object_tree_service::stop() {
    if (get_state() != service_state::running) {
        return true;
    }

    set_state(service_state::stopping);
    ilog("停止对象树服务: ${name}", ("name", name()));
    set_state(service_state::stopped);
    return true;
}

void object_tree_service::cleanup() {
    ilog("清理对象树服务: ${name}", ("name", name()));

    // 清理对象树
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    m_impl->m_root.children.clear();
}

bool object_tree_service::is_healthy() const {
    return get_state() == service_state::running;
}

error_code object_tree_service::register_object(const path& object_path, void* data, size_t size) {
    if (object_path.empty()) {
        return error_code::invalid_path;
    }

    std::lock_guard<std::mutex> lock(m_impl->m_mutex);

    // 解析路径
    auto components = m_impl->parse_path(object_path);
    if (components.empty()) {
        return error_code::invalid_path;
    }

    // 查找或创建节点
    auto node = m_impl->find_node(components, true);
    if (!node) {
        return error_code::internal_error;
    }

    // 检查对象是否已存在
    if (node->data != nullptr) {
        return error_code::already_exists;
    }

    // 设置对象数据
    node->data = data;
    node->size = size;

    ilog("注册对象: ${path}, 数据指针: ${ptr}, 大小: ${size} 字节",
         ("path", object_path)("ptr", static_cast<void*>(data))("size", size));
    return error_code::success;
}

error_code object_tree_service::update_object_property(const path& object_path,
                                                       const char* property_name, const void* value,
                                                       size_t value_size) {
    if (object_path.empty() || !property_name) {
        return error_code::invalid_argument;
    }

    std::lock_guard<std::mutex> lock(m_impl->m_mutex);

    // 解析路径
    auto components = m_impl->parse_path(object_path);
    if (components.empty()) {
        return error_code::invalid_path;
    }

    // 查找节点
    auto node = m_impl->find_node(components);
    if (!node || !node->data) {
        return error_code::not_found;
    }

    // 实际属性更新将由对象自身处理
    // 目前简化实现，只输出日志
    ilog("更新对象属性: ${path}.${property}, 值大小: ${size} 字节",
         ("path", object_path)("property", property_name)("size", value_size));

    return error_code::success;
}

void* object_tree_service::get_object(const path& object_path, error_code& ec) {
    if (object_path.empty()) {
        ec = error_code::invalid_path;
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_impl->m_mutex);

    // 解析路径
    auto components = m_impl->parse_path(object_path);
    if (components.empty()) {
        ec = error_code::invalid_path;
        return nullptr;
    }

    // 查找节点
    auto node = m_impl->find_node(components);
    if (!node || !node->data) {
        ec = error_code::not_found;
        return nullptr;
    }

    ec = error_code::success;
    return node->data;
}

std::vector<std::string> object_tree_service::list_children(const path& parent_path,
                                                            error_code& ec) {
    std::vector<std::string> children;

    std::lock_guard<std::mutex> lock(m_impl->m_mutex);

    // 解析路径
    auto components = m_impl->parse_path(parent_path);

    // 查找节点
    object_node* node = nullptr;
    if (components.empty()) {
        // 根节点
        node = &m_impl->m_root;
    } else {
        node = m_impl->find_node(components);
    }

    if (!node) {
        ec = error_code::not_found;
        return children;
    }

    // 收集子节点名称
    for (const auto& [name, child] : node->children) {
        children.push_back(name);
    }

    ec = error_code::success;
    return children;
}

bool object_tree_service::exists(const path& object_path) {
    if (object_path.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_impl->m_mutex);

    // 解析路径
    auto components = m_impl->parse_path(object_path);
    if (components.empty()) {
        return false;
    }

    // 查找节点
    auto node = m_impl->find_node(components);
    return node && node->data != nullptr;
}

error_code object_tree_service::remove_object(const path& object_path) {
    if (object_path.empty()) {
        return error_code::invalid_path;
    }

    std::lock_guard<std::mutex> lock(m_impl->m_mutex);

    // 解析路径
    auto components = m_impl->parse_path(object_path);
    if (components.empty()) {
        return error_code::invalid_path;
    }

    // 如果只有一个组件，处理根节点的直接子节点
    if (components.size() == 1) {
        auto it = m_impl->m_root.children.find(components[0]);
        if (it == m_impl->m_root.children.end() || !it->second->data) {
            return error_code::not_found;
        }

        // 移除对象
        it->second->data = nullptr;
        it->second->size = 0;

        // 如果没有子节点，可以完全移除
        if (it->second->children.empty()) {
            m_impl->m_root.children.erase(it);
        }

        return error_code::success;
    }

    // 查找父节点
    auto parent_components = components;
    parent_components.pop_back();
    auto parent = m_impl->find_node(parent_components);

    if (!parent) {
        return error_code::not_found;
    }

    // 获取目标节点名称
    const auto& name = components.back();

    // 查找子节点
    auto it = parent->children.find(name);
    if (it == parent->children.end() || !it->second->data) {
        return error_code::not_found;
    }

    // 移除对象
    it->second->data = nullptr;
    it->second->size = 0;

    // 如果没有子节点，可以完全移除
    if (it->second->children.empty()) {
        parent->children.erase(it);
    }

    ilog("删除对象: ${path}", ("path", object_path));
    return error_code::success;
}

} // namespace mc::db