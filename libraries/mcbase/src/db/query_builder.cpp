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

#include <mc/db/query/builder.h>

namespace mc::db::query {

query_builder::query_builder(const condition& cond)
{
    m_condition = cond;
}

query_builder::query_builder(const mc::dict& spec)
{
    m_condition = to_condition(spec);
}

query_builder::query_builder(const mc::variant& spec)
{
    m_condition = to_condition(spec);
}

query_builder& query_builder::where(mc::string_view field, const mc::variant& value)
{
    return where(field, compare_op::eq, value);
}

query_builder& query_builder::where(mc::string_view field, compare_op op, const mc::variant& value)
{
    condition cond(op, mc::string(field), value);
    return add_condition(cond);
}

query_builder& query_builder::or_where(mc::string_view field, const mc::variant& value)
{
    return or_where(field, compare_op::eq, value);
}

query_builder& query_builder::or_where(mc::string_view field, compare_op op, const mc::variant& value)
{
    condition cond(op, mc::string(field), value);
    if (!m_condition.has_value()) {
        m_condition = cond;
        return *this;
    }

    std::vector<condition> conditions;
    if (m_condition->is_logical() && m_condition->get_logical_op() == logical_op::OR) {
        conditions = m_condition->get_conditions();
    } else {
        conditions.push_back(*m_condition);
    }
    conditions.push_back(cond);
    m_condition = condition(logical_op::OR, std::move(conditions));
    return *this;
}

query_builder& query_builder::where(const condition& cond)
{
    return add_condition(cond);
}

query_builder& query_builder::where_like(mc::string_view field, mc::string_view pattern)
{
    condition like_cond = conditions::like(mc::string(field), mc::string(pattern));
    return add_condition(like_cond);
}

bool query_builder::is_empty() const
{
    return !m_condition.has_value();
}

bool query_builder::has_condition() const
{
    return m_condition.has_value();
}

const condition& query_builder::get_condition() const
{
    if (!m_condition.has_value()) {
        static condition empty_condition;
        return empty_condition;
    }
    return m_condition.value();
}

void query_builder::clear()
{
    m_condition.reset();
}

mc::string query_builder::to_string() const
{
    if (!has_condition()) {
        return "空条件";
    }
    return m_condition->to_string();
}

query_builder& query_builder::limit(size_t limit_value)
{
    m_limit = limit_value;
    return *this;
}

size_t query_builder::get_limit() const
{
    return m_limit;
}

bool query_builder::has_limit() const
{
    return m_limit > 0;
}

query_builder& query_builder::add_condition(const condition& cond)
{
    if (!m_condition.has_value()) {
        m_condition = cond;
        return *this;
    }

    std::vector<condition> conditions;
    if (m_condition->is_logical() && m_condition->get_logical_op() == logical_op::AND) {
        conditions = m_condition->get_conditions();
    } else {
        conditions.push_back(*m_condition);
    }
    conditions.push_back(cond);
    m_condition = condition(logical_op::AND, std::move(conditions));
    return *this;
}

} // namespace mc::db::query
