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
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <string>

namespace mc::database {

// 引用跟踪服务实现
class reference_tracker_service::impl {
public:
    impl() = default;
    
    // 对象引用记录
    std::unordered_map<std::string, std::unordered_set<client_id>> m_object_references;
    
    // 客户端引用记录
    std::unordered_map<client_id, std::unordered_set<std::string>> m_client_references;
    
    // 线程安全保护
    std::mutex m_mutex;
};

reference_tracker_service::reference_tracker_service(std::string name)
    : service_base(std::move(name)), m_impl(std::make_unique<impl>()) {}

reference_tracker_service::~reference_tracker_service() = default;

bool reference_tracker_service::init(dict args) {
    set_state(service_state::starting);
    ilog("初始化引用跟踪服务: ${name}", ("name", name()));
    
    // 读取配置，如有必要
    if (args.contains("dependencies")) {
        m_dependencies = args.at("dependencies").as<std::vector<std::string>>();
    } else {
        // 默认依赖通信服务和对象树服务
        m_dependencies = {"communication", "object_tree"};
    }
    
    set_state(service_state::stopped);
    return true;
}

bool reference_tracker_service::start() {
    if (get_state() == service_state::running) {
        return true;
    }
    
    set_state(service_state::starting);
    ilog("启动引用跟踪服务: ${name}", ("name", name()));
    set_state(service_state::running);
    return true;
}

bool reference_tracker_service::stop() {
    if (get_state() != service_state::running) {
        return true;
    }
    
    set_state(service_state::stopping);
    ilog("停止引用跟踪服务: ${name}", ("name", name()));
    set_state(service_state::stopped);
    return true;
}

void reference_tracker_service::cleanup() {
    ilog("清理引用跟踪服务: ${name}", ("name", name()));
    
    // 清理引用记录
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    m_impl->m_object_references.clear();
    m_impl->m_client_references.clear();
}

bool reference_tracker_service::is_healthy() const {
    return get_state() == service_state::running;
}

error_code reference_tracker_service::add_reference(const path& object_path, client_id client) {
    if (object_path.empty()) {
        return error_code::invalid_path;
    }
    
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    
    // 添加对象的客户端引用
    std::string path_str(object_path);
    m_impl->m_object_references[path_str].insert(client);
    
    // 添加客户端的对象引用
    m_impl->m_client_references[client].insert(path_str);
    
    ilog("添加引用：客户端 ${client} -> 对象 ${path}", 
         ("client", client)("path", object_path));
    return error_code::success;
}

error_code reference_tracker_service::remove_reference(const path& object_path, client_id client) {
    if (object_path.empty()) {
        return error_code::invalid_path;
    }
    
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    
    // 转换路径为字符串
    std::string path_str(object_path);
    
    // 检查对象引用是否存在
    auto obj_it = m_impl->m_object_references.find(path_str);
    if (obj_it == m_impl->m_object_references.end()) {
        return error_code::not_found;
    }
    
    // 移除对象的客户端引用
    obj_it->second.erase(client);
    if (obj_it->second.empty()) {
        m_impl->m_object_references.erase(obj_it);
    }
    
    // 移除客户端的对象引用
    auto client_it = m_impl->m_client_references.find(client);
    if (client_it != m_impl->m_client_references.end()) {
        client_it->second.erase(path_str);
        if (client_it->second.empty()) {
            m_impl->m_client_references.erase(client_it);
        }
    }
    
    ilog("移除引用：客户端 ${client} -> 对象 ${path}", 
         ("client", client)("path", object_path));
    return error_code::success;
}

size_t reference_tracker_service::get_reference_count(const path& object_path) {
    if (object_path.empty()) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    
    // 获取对象的引用计数
    std::string path_str(object_path);
    auto it = m_impl->m_object_references.find(path_str);
    if (it == m_impl->m_object_references.end()) {
        return 0;
    }
    
    return it->second.size();
}

std::vector<client_id> reference_tracker_service::get_referencing_clients(const path& object_path) {
    std::vector<client_id> clients;
    
    if (object_path.empty()) {
        return clients;
    }
    
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    
    // 获取引用对象的所有客户端
    std::string path_str(object_path);
    auto it = m_impl->m_object_references.find(path_str);
    if (it == m_impl->m_object_references.end()) {
        return clients;
    }
    
    clients.reserve(it->second.size());
    for (client_id client : it->second) {
        clients.push_back(client);
    }
    
    return clients;
}

void reference_tracker_service::cleanup_client_references(client_id client) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    
    // 获取客户端引用的所有对象
    auto client_it = m_impl->m_client_references.find(client);
    if (client_it == m_impl->m_client_references.end()) {
        return;
    }
    
    // 保存对象路径的副本，因为在删除过程中需要修改集合
    std::vector<std::string> objects(client_it->second.begin(), client_it->second.end());
    
    ilog("清理客户端 ${client} 的所有引用，共 ${count} 个对象", 
         ("client", client)("count", objects.size()));
    
    // 逐个移除引用
    for (const auto& obj_path : objects) {
        // 从对象引用记录中移除该客户端
        auto obj_it = m_impl->m_object_references.find(obj_path);
        if (obj_it != m_impl->m_object_references.end()) {
            obj_it->second.erase(client);
            if (obj_it->second.empty()) {
                m_impl->m_object_references.erase(obj_it);
            }
        }
    }
    
    // 最后，删除客户端的引用记录
    m_impl->m_client_references.erase(client);
}

} // namespace mc::database 