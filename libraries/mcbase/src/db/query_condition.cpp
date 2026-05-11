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

#include <mc/db/query/condition.h>
#include <mc/log/log.h>

namespace mc::db::query::string_ops {

bool contains(mc::string_view str, mc::string_view sub)
{
    return str.find(sub) != mc::string_view::npos;
}

bool like(mc::string_view str, mc::string_view pattern)
{
    if (pattern.empty()) {
        return str.empty();
    }

    if (pattern.back() == '%' && pattern.find('%') == pattern.size() - 1) {
        return str.substr(0, pattern.size() - 1) == pattern.substr(0, pattern.size() - 1);
    }

    if (pattern.front() == '%' && pattern.find('%', 1) == mc::string_view::npos) {
        return str.size() >= pattern.size() - 1 && str.substr(str.size() - (pattern.size() - 1)) == pattern.substr(1);
    }

    if (pattern.front() == '%' && pattern.back() == '%' && pattern.find('%', 1) == pattern.size() - 1) {
        auto sub = pattern.substr(1, pattern.size() - 2);
        return contains(str, sub);
    }

    return str == pattern;
}

} // namespace mc::db::query::string_ops

namespace mc::db::query {

mc::string condition::to_string() const
{
    if (m_is_logical) {
        mc::string result;
        switch (m_logical_op) {
            case logical_op::AND:
                result = "AND(";
                break;
            case logical_op::OR:
                result = "OR(";
                break;
            case logical_op::NOT:
                result = "NOT(";
                break;
        }

        for (size_t i = 0; i < m_conditions.size(); ++i) {
            if (i > 0) {
                result += ", ";
            }
            result += m_conditions[i].to_string();
        }
        result += ")";
        return result;
    }

    mc::string op_str;
    switch (m_op) {
        case compare_op::eq:
            op_str = "==";
            break;
        case compare_op::ne:
            op_str = "!=";
            break;
        case compare_op::gt:
            op_str = ">";
            break;
        case compare_op::ge:
            op_str = ">=";
            break;
        case compare_op::lt:
            op_str = "<";
            break;
        case compare_op::le:
            op_str = "<=";
            break;
        case compare_op::contains:
            op_str = "contains";
            break;
        case compare_op::like:
            op_str = "like";
            break;
        case compare_op::in:
            op_str = "in";
            break;
        case compare_op::between:
            op_str = "between";
            break;
    }

    mc::string value_str;
    if (m_value.is_string()) {
        value_str = "\"" + m_value.as_string() + "\"";
    } else {
        value_str = "value";
    }

    return m_field + " " + op_str + " " + value_str;
}

bool condition::eval_logical_erased(const void* obj, erased_match_fn match_fn) const
{
    if (m_conditions.empty()) {
        return true;
    }

    if (match_fn == nullptr) {
        return false;
    }

    switch (m_logical_op) {
        case logical_op::AND:
            for (const auto& cond : m_conditions) {
                if (!match_fn(cond, obj)) {
                    return false;
                }
            }
            return true;
        case logical_op::OR:
            for (const auto& cond : m_conditions) {
                if (match_fn(cond, obj)) {
                    return true;
                }
            }
            return false;
        case logical_op::NOT:
            return !m_conditions.empty() ? !match_fn(m_conditions[0], obj) : true;
        default:
            return false;
    }
}

bool condition::eval_object_erased(void* obj, mc::string_view type_name,
                                   const mc::reflect::property_type_info* property_info,
                                   const mc::reflect::method_type_info*   method_info) const
{
    if (property_info != nullptr) {
        return compare_values(property_info->get_value(obj), m_value, m_op);
    }

    if (method_info == nullptr) {
        return false;
    }

    try {
        return compare_values(method_info->invoke(obj, {}), m_value, m_op);
    } catch (const std::exception& e) {
        dlog("eval_object error: type: ${type}, field: ${field}, error: ${error}",
             ("type", type_name)("field", m_field)("error", e.what()));
        return false;
    }
}

bool condition::compare_values(const mc::variant& field_value, const mc::variant& value, compare_op op) const
{
    switch (op) {
        case compare_op::eq:
            return field_value == value;
        case compare_op::ne:
            return field_value != value;
        case compare_op::gt:
            return field_value > value;
        case compare_op::ge:
            return field_value >= value;
        case compare_op::lt:
            return field_value < value;
        case compare_op::le:
            return field_value <= value;
        case compare_op::contains:
            if (field_value.is_string() && value.is_string()) {
                return string_ops::contains(field_value.get_string(), value.get_string());
            }
            return false;
        case compare_op::like:
            if (field_value.is_string() && value.is_string()) {
                return string_ops::like(field_value.get_string(), value.get_string());
            }
            return false;
        case compare_op::in:
            if (value.is_array()) {
                for (const auto& item : value.get_array()) {
                    if (field_value == item) {
                        return true;
                    }
                }
            }
            return false;
        case compare_op::between:
            if (value.is_array() && value.size() >= 2) {
                const auto& arr = value.get_array();
                return field_value >= arr[0] && field_value <= arr[1];
            }
            return false;
        default:
            return false;
    }
}

namespace {

mc::string_view expect_string_value(const mc::variant& value, mc::string_view key_name)
{
    if (!value.is_string()) {
        MC_THROW(mc::invalid_arg_exception, "查询 AST 字段 ${key} 必须是字符串", ("key", key_name));
    }
    return value.get_string();
}

mc::string_view expect_key_string(const mc::variant& value)
{
    if (!value.is_string()) {
        MC_THROW(mc::invalid_arg_exception, "查询条件对象的键必须是字符串");
    }
    return value.get_string();
}

const mc::variant& require_dict_value(const mc::dict& dict, mc::string_view key_name)
{
    auto it = dict.find(key_name);
    if (it == dict.end()) {
        MC_THROW(mc::invalid_arg_exception, "查询 AST 缺少字段 ${key}", ("key", key_name));
    }
    return it->value;
}

compare_op parse_compare_op(mc::string_view op)
{
    if (op == "eq" || op == "$eq") {
        return compare_op::eq;
    }
    if (op == "ne" || op == "$ne") {
        return compare_op::ne;
    }
    if (op == "gt" || op == "$gt") {
        return compare_op::gt;
    }
    if (op == "ge" || op == "$ge" || op == "$gte") {
        return compare_op::ge;
    }
    if (op == "lt" || op == "$lt") {
        return compare_op::lt;
    }
    if (op == "le" || op == "$le" || op == "$lte") {
        return compare_op::le;
    }
    if (op == "contains" || op == "$contains") {
        return compare_op::contains;
    }
    if (op == "like" || op == "$like") {
        return compare_op::like;
    }
    if (op == "in" || op == "$in") {
        return compare_op::in;
    }
    if (op == "between" || op == "$between") {
        return compare_op::between;
    }

    MC_THROW(mc::invalid_arg_exception, "未知查询比较操作 ${op}", ("op", op));
}

condition parse_condition_dict_impl(const mc::dict& spec);

std::vector<condition> parse_condition_list(const mc::variant& value)
{
    if (!value.is_array()) {
        MC_THROW(mc::invalid_arg_exception, "查询 AST 的 conditions 必须是数组");
    }

    std::vector<condition> result;
    const auto&            items = value.get_array();
    result.reserve(items.size());
    for (const auto& item : items) {
        if (!item.is_object()) {
            MC_THROW(mc::invalid_arg_exception, "查询 AST 的 conditions 元素必须是对象");
        }
        result.push_back(parse_condition_dict_impl(item.get_object()));
    }
    return result;
}

condition parse_not_condition(const mc::dict& spec)
{
    if (spec.contains("condition")) {
        const auto& child = require_dict_value(spec, "condition");
        if (!child.is_object()) {
            MC_THROW(mc::invalid_arg_exception, "查询 AST 的 condition 必须是对象");
        }
        return conditions::not_cond(parse_condition_dict_impl(child.get_object()));
    }

    auto conditions = parse_condition_list(require_dict_value(spec, "conditions"));
    if (conditions.size() != 1) {
        MC_THROW(mc::invalid_arg_exception, "查询 AST 的 not 仅支持单个子条件");
    }
    return conditions::not_cond(std::move(conditions[0]));
}

bool is_internal_ast_dict(const mc::dict& spec)
{
    if (!spec.contains("op")) {
        return false;
    }

    return spec.contains("field") || spec.contains("conditions") || spec.contains("condition");
}

condition combine_conditions_with_and(std::vector<condition> conditions)
{
    if (conditions.empty()) {
        MC_THROW(mc::invalid_arg_exception, "查询条件不能为空对象");
    }

    if (conditions.size() == 1) {
        return std::move(conditions[0]);
    }

    return conditions::and_cond(std::move(conditions));
}

condition parse_mongodb_field_value(mc::string_view field_name, const mc::variant& value)
{
    if (!value.is_object()) {
        return condition(compare_op::eq, mc::string(field_name), value);
    }

    const auto& op_dict      = value.get_object();
    bool        has_operator = false;
    for (const auto& entry : op_dict) {
        auto op_name = expect_key_string(entry.key);
        if (!op_name.empty() && op_name.front() == '$') {
            has_operator = true;
            break;
        }
    }

    if (!has_operator) {
        return condition(compare_op::eq, mc::string(field_name), value);
    }

    std::vector<condition> conditions;
    conditions.reserve(op_dict.size());
    for (const auto& entry : op_dict) {
        auto op_name = expect_key_string(entry.key);
        conditions.emplace_back(parse_compare_op(op_name), mc::string(field_name), entry.value);
    }

    return combine_conditions_with_and(std::move(conditions));
}

condition parse_mongodb_logical_value(mc::string_view op_name, const mc::variant& value)
{
    if (op_name == "$and") {
        return conditions::and_cond(parse_condition_list(value));
    }
    if (op_name == "$or") {
        return conditions::or_cond(parse_condition_list(value));
    }
    if (op_name == "$not") {
        if (!value.is_object()) {
            MC_THROW(mc::invalid_arg_exception, "MongoDB 风格查询的 $not 必须是对象");
        }
        return conditions::not_cond(parse_condition_dict_impl(value.get_object()));
    }

    MC_THROW(mc::invalid_arg_exception, "未知逻辑操作符 ${op}", ("op", op_name));
}

condition parse_mongodb_dict(const mc::dict& spec)
{
    std::vector<condition> conditions;
    conditions.reserve(spec.size());
    for (const auto& entry : spec) {
        auto key_name = expect_key_string(entry.key);
        if (!key_name.empty() && key_name.front() == '$') {
            conditions.push_back(parse_mongodb_logical_value(key_name, entry.value));
            continue;
        }

        conditions.push_back(parse_mongodb_field_value(key_name, entry.value));
    }

    return combine_conditions_with_and(std::move(conditions));
}

condition parse_condition_dict_impl(const mc::dict& spec)
{
    if (is_internal_ast_dict(spec)) {
        auto op = expect_string_value(require_dict_value(spec, "op"), "op");
        if (op == "and") {
            return conditions::and_cond(parse_condition_list(require_dict_value(spec, "conditions")));
        }
        if (op == "or") {
            return conditions::or_cond(parse_condition_list(require_dict_value(spec, "conditions")));
        }
        if (op == "not") {
            return parse_not_condition(spec);
        }

        auto field = expect_string_value(require_dict_value(spec, "field"), "field");
        auto cmp   = parse_compare_op(op);
        return condition(cmp, mc::string(field), require_dict_value(spec, "value"));
    }

    return parse_mongodb_dict(spec);
}

} // namespace

condition to_condition(const mc::dict& spec)
{
    return parse_condition_dict_impl(spec);
}

condition to_condition(const mc::variant& spec)
{
    if (!spec.is_object()) {
        MC_THROW(mc::invalid_arg_exception, "查询 AST 顶层必须是对象");
    }
    return to_condition(spec.get_object());
}

} // namespace mc::db::query
