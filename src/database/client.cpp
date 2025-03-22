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

#include <chrono>
#include <cstring>
#include <iostream>
#include <mc/database/client.h>
#include <mc/database/object.h>
#include <mc/database/property.h>
#include <memory>
#include <thread>
#include <unordered_map>
#include <mc/reflect.h>

namespace mc {
namespace database {

// 错误码描述实现
std::string_view error_to_string(error_code ec) {
    switch (ec) {
        case error_code::success:
            return "成功";
        case error_code::not_initialized:
            return "未初始化";
        case error_code::already_exists:
            return "对象已存在";
        case error_code::not_found:
            return "对象不存在";
        case error_code::invalid_path:
            return "无效的路径";
        case error_code::access_denied:
            return "拒绝访问";
        case error_code::out_of_memory:
            return "内存不足";
        case error_code::invalid_argument:
            return "无效的参数";
        case error_code::internal_error:
            return "内部错误";
        case error_code::timeout:
            return "操作超时";
        case error_code::connection_failure:
            return "连接失败";
        default:
            return "未知错误";
    }
}

// 客户端实现中非模板方法的实现
error_code client::impl::remove_object(const path& object_path) {
    if (!is_connected()) {
        return error_code::not_initialized;
    }

    if (object_path.empty() || object_path[0] != '/') {
        return error_code::invalid_path;
    }

    std::string path_str = std::string(object_path);
    auto it = m_objects.find(path_str);
    if (it == m_objects.end()) {
        return error_code::not_found;
    }

    m_objects.erase(it);
    return error_code::success;
}

bool client::impl::exists_object(const path& object_path, error_code& ec) {
    if (!is_connected()) {
        ec = error_code::not_initialized;
        return false;
    }

    if (object_path.empty() || object_path[0] != '/') {
        ec = error_code::invalid_path;
        return false;
    }

    std::string path_str = std::string(object_path);
    ec = error_code::success;
    return m_objects.find(path_str) != m_objects.end();
}

std::vector<std::string> client::impl::list_child_objects(const path& parent_path, error_code& ec) {
    if (!is_connected()) {
        ec = error_code::not_initialized;
        return {};
    }

    if (!parent_path.empty() && parent_path[0] != '/') {
        ec = error_code::invalid_path;
        return {};
    }

    std::string parent_str = std::string(parent_path);
    if (!parent_str.empty() && parent_str.back() != '/') {
        parent_str += '/';
    }

    std::vector<std::string> result;
    for (const auto& pair : m_objects) {
        const std::string& obj_path = pair.first;
        
        // 检查是否是父路径的直接子对象
        if (obj_path.find(parent_str) == 0) {
            std::string relative_path = obj_path.substr(parent_str.length());
            size_t slash_pos = relative_path.find('/');
            
            // 只包含直接子对象，不包含更深层次的对象
            if (slash_pos == std::string::npos) {
                result.push_back(relative_path);
            }
        }
    }
    
    ec = error_code::success;
    return result;
}

// 客户端公共API实现
client::client() : m_impl(std::make_unique<impl>()) {}

client::~client() {
    if (m_impl->is_connected()) {
        disconnect();
    }
}

client::client(client&& other) noexcept : m_impl(std::move(other.m_impl)) {}

client& client::operator=(client&& other) noexcept {
    if (this != &other) {
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

void client::async_connect(completion_handler handler) {
    error_code ec = connect();
    handler(ec);
}

void client::async_disconnect(completion_handler handler) {
    error_code ec = disconnect();
    handler(ec);
}

error_code client::connect() {
    return m_impl->connect();
}

error_code client::disconnect() {
    return m_impl->disconnect();
}

bool client::is_connected() const {
    return m_impl->is_connected();
}

error_code client::remove(const path& object_path) {
    return m_impl->remove_object(object_path);
}

bool client::exists(const path& object_path, error_code& ec) {
    return m_impl->exists_object(object_path, ec);
}

std::vector<std::string> client::list_children(const path& parent_path, error_code& ec) {
    return m_impl->list_child_objects(parent_path, ec);
}

void client::async_remove(const path& object_path, completion_handler handler) {
    error_code ec = remove(object_path);
    handler(ec);
}

void client::async_list_children(const path& parent_path, string_list_handler handler) {
    error_code ec;
    auto children = list_children(parent_path, ec);
    handler(ec, children);
}

void client::async_exists(const path& object_path, boolean_handler handler) {
    error_code ec;
    bool exists_result = exists(object_path, ec);
    handler(ec, exists_result);
}

} // namespace database
} // namespace mc