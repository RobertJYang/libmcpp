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

#include <mc/event.h>
#include <mc/exception.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mc {

event::~event() = default;

namespace {

class event_type_registry {
public:
    event_type_id register_named(mc::string_view name)
    {
        if (name.empty()) {
            return register_anonymous();
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto                        it = m_named_ids.find(std::string(name));
        if (it != m_named_ids.end()) {
            return it->second;
        }

        auto id = next_id();
        m_named_ids.emplace(std::string(name), id);
        return id;
    }

private:
    event_type_id register_anonymous()
    {
        return next_id();
    }

    event_type_id next_id()
    {
        auto id = m_next_id.fetch_add(1, std::memory_order_relaxed);
        MC_ASSERT(id >= event_type_id_first_user, "事件类型 ID 已耗尽");
        return id;
    }

    std::mutex                                     m_mutex;
    std::unordered_map<std::string, event_type_id> m_named_ids;
    std::atomic<event_type_id>                     m_next_id{event_type_id_first_user};
};

event_type_registry& get_event_type_registry() noexcept
{
    static event_type_registry registry;
    return registry;
}

} // namespace

event_type_id register_event_type(mc::string_view name)
{
    return get_event_type_registry().register_named(name);
}

} // namespace mc
