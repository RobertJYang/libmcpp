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

#ifndef MC_ENGINE_MATCH_CONDITION_FILTER_H
#define MC_ENGINE_MATCH_CONDITION_FILTER_H

#include <mc/common.h>
#include <mc/engine/match/filter.h>

namespace mc::db::query {
class condition;
} // namespace mc::db::query

namespace mc::engine::match {

class MC_API condition_filter_backend final : public filter_backend {
public:
    static constexpr std::uint32_t kBackendType = condition_filter;

    std::uint32_t backend_type() const noexcept override
    {
        return kBackendType;
    }
    mc::string_view name() const noexcept override
    {
        return "condition";
    }

    filter_compiled_ptr compile(mc::string_view text) const override;
    bool                evaluate(const filter_compiled& compiled, const message& msg) const override;
};

MC_API filter_spec make_condition_filter(const mc::db::query::condition& cond);
MC_API void        register_condition_filter_backend();

} // namespace mc::engine::match

#endif // MC_ENGINE_MATCH_CONDITION_FILTER_H
