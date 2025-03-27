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
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace mc::database {

// 通信服务实现
class communication_service::impl {
public:
    impl() : m_next_client_id(1) {}
    
    // 消息处理器映射
    std::unordered_map<message_type, message_handler> m_handlers;
    
    // 活跃的客户端集合
    std::unordered_map<client_id, bool> m_active_clients;
    
    // 线程安全保护
    std::mutex m_mutex;
    
    // 客户端ID计数器
    std::atomic<client_id> m_next_client_id;
};

communication_service::communication_service(std::string name)
    : service_base(std::move(name)), m_impl(std::make_unique<impl>()) {}

communication_service::~communication_service() = default;

bool communication_service::init(dict args) {
    set_state(service_state::starting);
    ilog("初始化通信服务: ${name}", ("name", name()));
    
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

bool communication_service::start() {
    if (get_state() == service_state::running) {
        return true;
    }
    
    set_state(service_state::starting);
    ilog("启动通信服务: ${name}", ("name", name()));
    set_state(service_state::running);
    return true;
}

bool communication_service::stop() {
    if (get_state() != service_state::running) {
        return true;
    }
    
    set_state(service_state::stopping);
    ilog("停止通信服务: ${name}", ("name", name()));
    set_state(service_state::stopped);
    return true;
}

void communication_service::cleanup() {
    ilog("清理通信服务: ${name}", ("name", name()));
    
    // 关闭所有客户端连接
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    if (!m_impl->m_active_clients.empty()) {
        ilog("关闭 ${count} 个客户端连接", ("count", m_impl->m_active_clients.size()));
        m_impl->m_active_clients.clear();
    }
    
    // 清理处理器
    m_impl->m_handlers.clear();
}

bool communication_service::is_healthy() const {
    return get_state() == service_state::running;
}

void communication_service::register_handler(message_type type, message_handler handler) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    m_impl->m_handlers[type] = std::move(handler);
}

void communication_service::send_message(client_id client, message_type type, const void* data, size_t data_size) {
    // 检查客户端是否存在
    {
        std::lock_guard<std::mutex> lock(m_impl->m_mutex);
        auto it = m_impl->m_active_clients.find(client);
        if (it == m_impl->m_active_clients.end()) {
            elog("尝试向不存在的客户端发送消息，ID: ${client}", ("client", client));
            return;
        }
    }
    
    ilog("向客户端 ${client} 发送消息，类型: ${type}，数据大小: ${size} 字节", 
         ("client", client)("type", static_cast<int>(type))("size", data_size));
    
    // 实际发送逻辑将在后续实现
}

client_id communication_service::register_client() {
    client_id id = m_impl->m_next_client_id.fetch_add(1);
    
    {
        std::lock_guard<std::mutex> lock(m_impl->m_mutex);
        m_impl->m_active_clients[id] = true;
    }
    
    ilog("注册新客户端，ID: ${id}", ("id", id));
    return id;
}

void communication_service::unregister_client(client_id client) {
    bool removed = false;
    
    {
        std::lock_guard<std::mutex> lock(m_impl->m_mutex);
        auto it = m_impl->m_active_clients.find(client);
        if (it != m_impl->m_active_clients.end()) {
            m_impl->m_active_clients.erase(it);
            removed = true;
        }
    }
    
    if (removed) {
        ilog("注销客户端，ID: ${client}", ("client", client));
    } else {
        elog("尝试注销不存在的客户端，ID: ${client}", ("client", client));
    }
}

} // namespace mc::database 