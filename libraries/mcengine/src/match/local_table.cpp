/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "local_table.h"

#include <mc/exception.h>

#include <utility>

namespace mc::engine::match {

std::atomic<match_id>& local_table::id_counter()
{
    // 全局递增，跨多个 local_table 实例也不会冲突；测试场景下显式 reset 会让
    // 计数继续向前，但 0 永远不分配，避免被误判为非法。
    static std::atomic<match_id> s_counter{0};
    return s_counter;
}

match_id local_table::allocate_id()
{
    auto&    counter = id_counter();
    match_id id      = 0;
    do {
        id = counter.fetch_add(1, std::memory_order_relaxed) + 1;
    } while (id == 0);
    return id;
}

local_table::local_table()  = default;
local_table::~local_table() = default;

match_id local_table::subscribe(mc::string_view service_name, match_rule rule, filter_spec spec,
                                match_callback callback)
{
    MC_ASSERT_THROW(callback, mc::invalid_arg_exception, "match::table::subscribe 需要非空 callback");

    if (spec.backend_type != no_filter) {
        auto compiled = filter_backend_registry::instance().get_or_compile(spec.backend_type, spec.text);
        MC_ASSERT_THROW(compiled, mc::invalid_arg_exception,
                        "filter backend 未注册或表达式编译失败: backend_type=${type}", ("type", spec.backend_type));
    }

    entry e;
    e.service_name = mc::string(service_name);
    e.rule         = std::move(rule);
    e.spec         = std::move(spec);
    e.callback     = std::move(callback);

    auto id = allocate_id();
    {
        std::lock_guard lock(m_mutex);
        m_entries.emplace(id, std::move(e));
    }
    return id;
}

void local_table::unsubscribe(match_id id) noexcept
{
    if (id == 0) {
        return;
    }
    std::lock_guard lock(m_mutex);
    m_entries.erase(id);
}

void local_table::clear() noexcept
{
    std::lock_guard lock(m_mutex);
    m_entries.clear();
}

std::size_t local_table::size() const noexcept
{
    std::lock_guard lock(m_mutex);
    return m_entries.size();
}

std::vector<target> local_table::find_targets(const message& msg) const
{
    auto&               registry = filter_backend_registry::instance();
    std::vector<target> result;
    std::lock_guard     lock(m_mutex);
    result.reserve(m_entries.size());
    for (const auto& kv : m_entries) {
        const auto& e = kv.second;
        if (!matches(e.rule, msg.header)) {
            continue;
        }
        if (!e.spec.empty() && !registry.evaluate(e.spec.backend_type, e.spec.text, msg)) {
            continue;
        }
        target t;
        t.endpoint_id  = 0;
        t.instance_id  = 0;
        t.service_name = e.service_name;
        t.id           = kv.first;
        result.push_back(std::move(t));
    }
    return result;
}

match_callback local_table::lookup_callback(mc::string_view service_name, match_id id) const
{
    if (id == 0) {
        return {};
    }
    std::lock_guard lock(m_mutex);
    auto            it = m_entries.find(id);
    if (it == m_entries.end()) {
        return {};
    }
    if (!service_name.empty() && it->second.service_name != service_name) {
        // service_name 不匹配视为未找到，避免错调到别的 service 的回调。
        return {};
    }
    return it->second.callback;
}

} // namespace mc::engine::match
