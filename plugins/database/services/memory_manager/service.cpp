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
#include <mutex>
#include <unordered_map>

namespace mc::db {

// 内存管理服务实现
class memory_manager_service::impl {
public:
    impl()  = default;
    ~impl() = default;

    // 内存分配记录
    std::unordered_map<void*, size_t> m_allocations;
    std::mutex                        m_mutex;
};

memory_manager_service::memory_manager_service(std::string name)
    : service_base(std::move(name)), m_impl(std::make_unique<impl>()) {
}

memory_manager_service::~memory_manager_service() = default;

bool memory_manager_service::init(dict args) {
    set_state(service_state::starting);
    ilog("初始化内存管理服务: ${name}", ("name", name()));
    set_state(service_state::stopped);
    return true;
}

bool memory_manager_service::start() {
    if (get_state() == service_state::running) {
        return true;
    }

    set_state(service_state::starting);
    ilog("启动内存管理服务: ${name}", ("name", name()));
    set_state(service_state::running);
    return true;
}

bool memory_manager_service::stop() {
    if (get_state() != service_state::running) {
        return true;
    }

    set_state(service_state::stopping);
    ilog("停止内存管理服务: ${name}", ("name", name()));
    set_state(service_state::stopped);
    return true;
}

void memory_manager_service::cleanup() {
    ilog("清理内存管理服务: ${name}", ("name", name()));

    // 检查内存泄漏
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    if (!m_impl->m_allocations.empty()) {
        wlog("检测到 ${count} 处内存泄漏！", ("count", m_impl->m_allocations.size()));

        // 释放所有未释放的内存
        for (auto& [ptr, size] : m_impl->m_allocations) {
            wlog("自动释放内存: ${ptr}，大小: ${size} 字节",
                 ("ptr", static_cast<void*>(ptr))("size", size));
            free(ptr);
        }

        m_impl->m_allocations.clear();
    }
}

bool memory_manager_service::is_healthy() const {
    return get_state() == service_state::running;
}

void* memory_manager_service::allocate(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        std::lock_guard<std::mutex> lock(m_impl->m_mutex);
        m_impl->m_allocations[ptr] = size;
    }
    return ptr;
}

void memory_manager_service::deallocate(void* ptr) {
    if (ptr) {
        {
            std::lock_guard<std::mutex> lock(m_impl->m_mutex);
            m_impl->m_allocations.erase(ptr);
        }
        free(ptr);
    }
}

} // namespace mc::db