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

#ifndef MC_ENGINE_MATCH_TABLE_H
#define MC_ENGINE_MATCH_TABLE_H

#include <mc/common.h>
#include <mc/engine/match/filter.h>
#include <mc/engine/match/rule.h>
#include <mc/engine/match/types.h>
#include <mc/engine/message.h>
#include <mc/memory.h>
#include <mc/string_view.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mc::engine::match {

// 订阅与匹配的统一入口
class MC_API table : public mc::shared_base {
public:
    ~table() override = default;

    virtual match_id subscribe(mc::string_view service_name, match_rule rule, filter_spec spec,
                               match_callback callback) = 0;

    virtual void        unsubscribe(match_id id) noexcept = 0;
    virtual void        clear() noexcept                  = 0;
    virtual std::size_t size() const noexcept             = 0;

    virtual std::vector<target> find_targets(const message& msg) const                           = 0;
    virtual match_callback      lookup_callback(mc::string_view service_name, match_id id) const = 0;

    virtual std::size_t sweep_dead_endpoint(std::uint16_t /*endpoint_id*/,
                                            std::uint32_t /*min_alive_instance_id*/) noexcept
    {
        return 0;
    }
};

using table_ptr = mc::shared_ptr<table>;

} // namespace mc::engine::match

#endif // MC_ENGINE_MATCH_TABLE_H
