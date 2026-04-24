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

#include "message_type_name.h"
#include <mc/engine/match/condition_filter.h>

#include <mc/db/query/condition.h>
#include <mc/json.h>

#include <algorithm>
#include <utility>

namespace mc::engine::match {

namespace {

using mc::db::query::compare_op;
using mc::db::query::condition;
using mc::db::query::logical_op;

bool fetch_header_field(mc::string_view name, const message_header& header, mc::string& out)
{
    if (name == "type") {
        out = mc::string(message_type_to_name(header.type));
        return true;
    }
    if (name == "sender") {
        out = header.sender;
        return true;
    }
    if (name == "destination") {
        out = header.destination;
        return true;
    }
    if (name == "path") {
        out = header.path;
        return true;
    }
    if (name == "interface_name") {
        out = header.interface_name;
        return true;
    }
    if (name == "member_name") {
        out = header.member_name;
        return true;
    }
    return false;
}

bool variant_equals_string(const mc::variant& value, const mc::string& other)
{
    if (value.is_string()) {
        return value.get_string() == other;
    }
    return value.as_string() == other;
}

// 一期支持的字段对比操作集：eq / ne / in / like / contains。
// gt/ge/lt/le/between 对字符串字段无意义，对 header 6 字段不放开；后续 backend
// 引用 message body 时再扩展。
bool eval_compare_leaf(const condition& cond, const mc::string& field_value)
{
    const auto& value = cond.get_value();
    switch (cond.get_op()) {
        case compare_op::eq:
            return variant_equals_string(value, field_value);
        case compare_op::ne:
            return !variant_equals_string(value, field_value);
        case compare_op::in: {
            if (!value.is_array()) {
                return false;
            }
            for (const auto& item : value.get_array()) {
                if (variant_equals_string(item, field_value)) {
                    return true;
                }
            }
            return false;
        }
        case compare_op::like:
            return mc::db::query::string_ops::like(field_value, value.as_string());
        case compare_op::contains:
            return mc::db::query::string_ops::contains(field_value, value.as_string());
        default:
            return false;
    }
}

bool eval_condition(const condition& cond, const message_header& header);

bool eval_logical(const condition& cond, const message_header& header)
{
    const auto& children = cond.get_conditions();
    switch (cond.get_logical_op()) {
        case logical_op::AND:
            return std::all_of(children.begin(), children.end(), [&](const condition& child) {
                return eval_condition(child, header);
            });
        case logical_op::OR:
            return std::any_of(children.begin(), children.end(), [&](const condition& child) {
                return eval_condition(child, header);
            });
        case logical_op::NOT:
            return !children.empty() && !eval_condition(children.front(), header);
    }
    return false;
}

bool eval_condition(const condition& cond, const message_header& header)
{
    if (cond.is_logical()) {
        return eval_logical(cond, header);
    }
    mc::string field_value;
    if (!fetch_header_field(cond.get_field(), header, field_value)) {
        return false; // 引用了 header 之外的字段：用户契约失败 → 不匹配
    }
    return eval_compare_leaf(cond, field_value);
}

class condition_filter_compiled final : public filter_compiled {
public:
    explicit condition_filter_compiled(condition cond) : m_cond(std::move(cond))
    {}
    const condition& get() const noexcept
    {
        return m_cond;
    }

private:
    condition m_cond;
};

} // namespace

filter_compiled_ptr condition_filter_backend::compile(mc::string_view text) const
{
    if (text.empty()) {
        return mc::make_shared<condition_filter_compiled>(condition{});
    }

    mc::variant decoded;
    try {
        decoded = mc::json::json_decode(text);
    } catch (...) {
        return {};
    }
    if (!decoded.is_object()) {
        return {};
    }

    try {
        auto cond = mc::db::query::to_condition(decoded);
        return mc::make_shared<condition_filter_compiled>(std::move(cond));
    } catch (...) {
        return {};
    }
}

bool condition_filter_backend::evaluate(const filter_compiled& compiled, const message& msg) const
{
    const auto& self = static_cast<const condition_filter_compiled&>(compiled);
    const auto& cond = self.get();

    // 顶层默认条件（is_logical=false 且 field 为空）视为永远命中，给"无 filter
    // 但又走 condition backend"留个安全口子。
    if (!cond.is_logical() && cond.get_field().empty()) {
        return true;
    }
    return eval_condition(cond, msg.header);
}

void register_condition_filter_backend()
{
    filter_backend_registry::instance().register_backend(mc::make_shared<condition_filter_backend>());
}

} // namespace mc::engine::match
