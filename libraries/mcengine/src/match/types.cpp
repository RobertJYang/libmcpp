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

#include <mc/engine/match/types.h>

#include <mc/variant.h>

#include <algorithm>
#include <cstdint>

namespace mc::engine::match {

// ---------------------------------------------------------------------------
// pair 版本（首选）
// ---------------------------------------------------------------------------

void set_target_match_ids(message_header&                                    header,
                          const std::vector<std::pair<mc::string, match_id>>& pairs)
{
    mc::variants services;
    mc::variants ids;
    services.reserve(pairs.size());
    ids.reserve(pairs.size());
    for (const auto& p : pairs) {
        services.emplace_back(p.first);
        ids.emplace_back(static_cast<std::uint64_t>(p.second));
    }
    header.context[target_match_ids_services_key] = std::move(services);
    header.context[target_match_ids_ids_key]      = std::move(ids);
}

std::vector<std::pair<mc::string, match_id>>
get_target_match_ids(const message_header& header)
{
    std::vector<std::pair<mc::string, match_id>> result;

    auto svc_it = header.context.find(target_match_ids_services_key);
    auto ids_it = header.context.find(target_match_ids_ids_key);
    if (svc_it == header.context.end() || ids_it == header.context.end()) {
        return result;
    }

    try {
        const auto& svc_value = (*svc_it).value;
        const auto& ids_value = (*ids_it).value;
        if (!svc_value.is_array() || !ids_value.is_array()) {
            return result;
        }

        const auto& svc_arr = svc_value.get_array();
        const auto& ids_arr = ids_value.get_array();
        const auto  count   = std::min(svc_arr.size(), ids_arr.size());
        result.reserve(count);

        for (std::size_t i = 0; i < count; ++i) {
            if (!svc_arr[i].is_string() || !ids_arr[i].is_numeric()) {
                continue;
            }
            result.emplace_back(mc::string(svc_arr[i].get_string()),
                                static_cast<match_id>(ids_arr[i].template as<std::uint64_t>()));
        }
    } catch (...) {
        result.clear();
    }
    return result;
}

} // namespace mc::engine::match
