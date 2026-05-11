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

#ifndef MC_ENGINE_MATCH_INTERNAL_LOCAL_TABLE_H
#define MC_ENGINE_MATCH_INTERNAL_LOCAL_TABLE_H

#include <mc/engine/match/table.h>

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace mc::engine::match {

// match::table 的进程内实现。O(N) 线扫，适合订阅条数较少的本地场景。
class local_table final : public table {
public:
    local_table();
    ~local_table() override;

    match_id    subscribe(mc::string_view service_name,
                          match_rule      rule,
                          filter_spec     spec,
                          match_callback  callback) override;
    void        unsubscribe(match_id id) noexcept override;
    void        clear() noexcept override;
    std::size_t size() const noexcept override;

    std::vector<target> find_targets(const message& msg) const override;
    match_callback      lookup_callback(mc::string_view service_name, match_id id) const override;

private:
    struct entry {
        mc::string     service_name;
        match_rule     rule;
        filter_spec    spec;
        match_callback callback;
    };

    static std::atomic<match_id>& id_counter();
    static match_id               allocate_id();

    mutable std::mutex                       m_mutex;
    std::unordered_map<match_id, entry>      m_entries;
};

} // namespace mc::engine::match

#endif // MC_ENGINE_MATCH_INTERNAL_LOCAL_TABLE_H
