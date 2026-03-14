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

#include "mc/singleton.h"
#include <algorithm>
#include <iostream>
#include <mc/log.h>

namespace mc {

// 单例访问
singleton_manager& singleton_manager::instance()
{
    static singleton_manager instance;
    return instance;
}

// 析构函数
singleton_manager::~singleton_manager()
{
    destroy_instances();
}

// 注册单例销毁函数
void singleton_manager::register_singleton(const type_key_t& key, destroy_fn_t destroy_fn, bool leaky)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!leaky) {
        // 非泄露模式才需要登记销毁函数
        m_non_leaky_instances[key] = std::move(destroy_fn);
    }
}

// 销毁所有非泄露单例
void singleton_manager::destroy_instances()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 将所有非泄露单例的销毁函数添加到销毁队列
    for (const auto& kv : m_non_leaky_instances) {
        m_destroy_queue.push_back(kv.second);
    }

    // 清空非泄露单例映射
    m_non_leaky_instances.clear();

    // 执行所有销毁函数
    for (auto it = m_destroy_queue.rbegin(); it != m_destroy_queue.rend(); ++it) {
        try {
            (*it)();
        } catch (const std::exception& e) {
            elog("单例销毁异常: ${error}", ("error", e.what()));
        } catch (...) {
            elog("单例销毁未知异常");
        }
    }

    // 清空销毁队列
    m_destroy_queue.clear();
}

// 清空所有单例相关信息（仅用于测试）
void singleton_manager::reset_for_test()
{
    destroy_instances();

    // 确保所有实例都被销毁
    std::lock_guard<std::mutex> lock(m_mutex);
    m_non_leaky_instances.clear();
    m_destroy_queue.clear();
}

} // namespace mc