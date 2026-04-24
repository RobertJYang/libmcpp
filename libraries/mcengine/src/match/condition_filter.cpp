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

#include <mc/engine/match/condition_filter.h>

#include <mc/db/query/condition.h>
#include <mc/dict.h>
#include <mc/json.h>

namespace mc::engine::match {

namespace {

using mc::db::query::compare_op;
using mc::db::query::condition;
using mc::db::query::logical_op;

mc::string_view compare_op_name(compare_op op) noexcept
{
    switch (op) {
        case compare_op::eq:       return "eq";
        case compare_op::ne:       return "ne";
        case compare_op::gt:       return "gt";
        case compare_op::ge:       return "ge";
        case compare_op::lt:       return "lt";
        case compare_op::le:       return "le";
        case compare_op::contains: return "contains";
        case compare_op::like:     return "like";
        case compare_op::in:       return "in";
        case compare_op::between:  return "between";
    }
    return "eq";
}

mc::string_view logical_op_name(logical_op op) noexcept
{
    switch (op) {
        case logical_op::AND: return "and";
        case logical_op::OR:  return "or";
        case logical_op::NOT: return "not";
    }
    return "and";
}

// 输出格式严格匹配 mc::db::query::to_condition 接受的内部 AST：
//   叶子：{op:"eq", field:"...", value:...}
//   逻辑：{op:"and"|"or", conditions:[...]}
//        {op:"not", conditions:[child]}（沿用 to_condition 的 NOT 解析约定）
mc::variant condition_to_variant(const condition& cond)
{
    mc::dict out;
    if (cond.is_logical()) {
        out["op"] = mc::variant(mc::string(logical_op_name(cond.get_logical_op())));
        std::vector<mc::variant> children;
        children.reserve(cond.get_conditions().size());
        for (const auto& child : cond.get_conditions()) {
            children.push_back(condition_to_variant(child));
        }
        out["conditions"] = mc::variant(std::move(children));
    } else {
        out["op"]    = mc::variant(mc::string(compare_op_name(cond.get_op())));
        out["field"] = mc::variant(mc::string(cond.get_field()));
        out["value"] = cond.get_value();
    }
    return mc::variant(std::move(out));
}

} // namespace

filter_spec make_condition_filter(const condition& cond)
{
    filter_spec spec;
    spec.backend_type = condition_filter;
    spec.text         = mc::json::json_encode(condition_to_variant(cond));
    return spec;
}

} // namespace mc::engine::match
