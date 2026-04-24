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

#ifndef MC_ENGINE_MATCH_INTERNAL_SHARED_TABLE_H
#define MC_ENGINE_MATCH_INTERNAL_SHARED_TABLE_H

#include <mc/engine/match/table.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM

namespace mc::shm {
class shm_runtime;
class ipc_shared_mutex;
} // namespace mc::shm

namespace mc::engine::match {

namespace detail {
struct shared_root;
} // namespace detail

// match::table 的 SHM trie 实现。元数据跨进程共享，callback 留在 owner 进程本地。
class shared_table final : public table {
public:
    explicit shared_table(mc::shm::shm_runtime& runtime,
                          std::uint16_t         owner_endpoint_id  = 0,
                          std::uint32_t         owner_instance_id  = 0);
    ~shared_table() override;

    match_id    subscribe(mc::string_view service_name,
                          match_rule      rule,
                          filter_spec     spec,
                          match_callback  callback) override;
    void        unsubscribe(match_id id) noexcept override;
    void        clear() noexcept override;
    std::size_t size() const noexcept override;

    std::vector<target> find_targets(const message& msg) const override;
    match_callback      lookup_callback(mc::string_view service_name, match_id id) const override;

    // 清除指定 endpoint 的死亡残留订阅条目。
    std::size_t sweep_dead_endpoint(std::uint16_t endpoint_id,
                                    std::uint32_t min_alive_instance_id) noexcept override;

    // 本 endpoint 的身份标识。0 表示未绑定，所有命中按本地处理。
    std::uint16_t local_endpoint_id() const noexcept
    {
        return m_endpoint_id;
    }
    std::uint32_t local_instance_id() const noexcept
    {
        return m_instance_id;
    }

private:
    struct local_entry {
        mc::string     service_name;
        match_callback callback;
    };

    void _attach_or_create();

    std::int64_t _ensure_path_for_rule(const match_rule& rule);

    std::int64_t _push_subscription_entry(std::int64_t                  node_offset,
                                          std::uint64_t                 match_id,
                                          mc::string_view               service_name,
                                          const filter_spec&            spec);

    void _drop_owned_entries() noexcept;

    void _drop_entry_locked(std::uint64_t id) noexcept;

    mc::shm::shm_runtime&           m_runtime;
    mc::shm::ipc_shared_mutex*      m_lock{nullptr};
    detail::shared_root*            m_root{nullptr};
    std::uint16_t                   m_endpoint_id{0};
    std::uint32_t                   m_instance_id{0};

    mutable std::mutex                                     m_local_mutex;
    std::unordered_map<match_id, local_entry>              m_local_callbacks;
    std::unordered_map<match_id, std::int64_t>             m_owned_entry_offsets;
};

} // namespace mc::engine::match

#endif // MCENGINE_USE_SHM

#endif // MC_ENGINE_MATCH_INTERNAL_SHARED_TABLE_H
