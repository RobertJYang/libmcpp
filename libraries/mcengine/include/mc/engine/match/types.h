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

#ifndef MC_ENGINE_MATCH_TYPES_H
#define MC_ENGINE_MATCH_TYPES_H

#include <mc/common.h>
#include <mc/engine/message.h>
#include <mc/string.h>
#include <mc/string_view.h>

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace mc::engine::match {

// 订阅句柄。0 视为非法/未注册。
using match_id = std::uint64_t;

using match_callback = std::function<void(const message&)>;

struct target {
    std::uint16_t endpoint_id{0};
    std::uint32_t instance_id{0};
    mc::string    service_name;
    match_id      id{0};
};

inline constexpr const char* target_match_ids_services_key = "mc.match.ids.services";
inline constexpr const char* target_match_ids_ids_key      = "mc.match.ids.ids";

MC_API void set_target_match_ids(message_header& header, const std::vector<std::pair<mc::string, match_id>>& pairs);

MC_API std::vector<std::pair<mc::string, match_id>> get_target_match_ids(const message_header& header);

} // namespace mc::engine::match

#endif // MC_ENGINE_MATCH_TYPES_H
