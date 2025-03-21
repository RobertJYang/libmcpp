/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mc/core/event.h"
#include "mc/core/object.h"
#include <atomic>
#include <mutex>

namespace mc {
namespace core {

/**
 * @brief 全局事件分发器
 */
static std::shared_ptr<event_dispatcher> g_event_dispatcher = std::make_shared<standard_event_dispatcher>();
static std::mutex g_event_dispatcher_mutex;

std::shared_ptr<event_dispatcher> global_event_dispatcher() {
    std::lock_guard<std::mutex> lock(g_event_dispatcher_mutex);
    return g_event_dispatcher;
}

void set_global_event_dispatcher(std::shared_ptr<event_dispatcher> dispatcher) {
    std::lock_guard<std::mutex> lock(g_event_dispatcher_mutex);
    if (dispatcher) {
        g_event_dispatcher = dispatcher;
    }
}

/**
 * @brief 标准事件分发器的分发实现
 */
bool standard_event_dispatcher::dispatch(std::shared_ptr<event> e, std::shared_ptr<object> target) {
    if (!e || !target) {
        return false;
    }
    
    // 设置事件目标
    e->set_target(target);
    
    // 发送事件到目标对象
    return target->post_event(e);
}

/**
 * @brief 标准事件分发器的过滤实现
 */
bool standard_event_dispatcher::filter(std::shared_ptr<event> e, std::shared_ptr<object> target) {
    if (!e || !target) {
        return false;
    }
    
    // 调用目标对象的过滤方法
    return target->filter_event(target, e);
}

} // namespace core
} // namespace mc 