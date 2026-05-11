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

#include <mc/db/query/planner.h>

namespace mc::db::query {

query_planner_runtime::query_planner_runtime(const table_index_metadata_base& metadata) : m_metadata(metadata)
{}

query_plan query_planner_runtime::plan_for_query(const query_builder& builder) const
{
    if (!builder.has_condition()) {
        return {};
    }

    return find_matching_index(builder.get_condition());
}

query_plan query_planner_runtime::generate_plan(const condition& cond) const
{
    return find_matching_index(cond);
}

query_plan query_planner_runtime::find_matching_index(const condition& condition) const
{
    query_plan plan;

    if (condition.is_logical()) {
        logical_op  op             = condition.get_logical_op();
        const auto& sub_conditions = condition.get_conditions();

        if (op == logical_op::AND) {
            return find_best_index_for_and(sub_conditions);
        }

        if (op == logical_op::NOT && sub_conditions.size() == 1) {
            if (sub_conditions[0].get_op() == compare_op::in || sub_conditions[0].get_op() == compare_op::eq) {
                plan.plan_type = query_plan_type::full_scan;
            }
            return plan;
        }

        return plan_for_or_condition(sub_conditions);
    }

    return plan_for_field_condition(condition);
}

query_plan query_planner_runtime::plan_for_field_condition(const condition& cond) const
{
    query_plan      plan;
    compare_op      op    = cond.get_op();
    mc::string_view field = cond.get_field();

    auto index_id = m_metadata.find_best_index_id(field);
    if (index_id == -1) {
        return plan;
    }

    plan.use_index = true;
    plan.index_id  = index_id;
    plan.fields.push_back(mc::string(field));

    switch (op) {
        case compare_op::eq:
            plan.plan_type = query_plan_type::index_exact_match;
            plan.key_value = cond.get_value();
            break;
        case compare_op::in:
            plan.plan_type = query_plan_type::index_exact_match;
            if (cond.get_value().is_array()) {
                plan.values = cond.get_value().get_array();
            }
            break;
        case compare_op::gt:
        case compare_op::ge:
        case compare_op::lt:
        case compare_op::le:
        case compare_op::between:
            plan.plan_type = query_plan_type::index_range;
            if (op == compare_op::gt) {
                plan.range.has_lower_bound = true;
                plan.range.include_lower   = false;
                plan.range.lower_bound     = cond.get_value();
            } else if (op == compare_op::ge) {
                plan.range.has_lower_bound = true;
                plan.range.include_lower   = true;
                plan.range.lower_bound     = cond.get_value();
            } else if (op == compare_op::lt) {
                plan.range.has_upper_bound = true;
                plan.range.include_upper   = false;
                plan.range.upper_bound     = cond.get_value();
            } else if (op == compare_op::le) {
                plan.range.has_upper_bound = true;
                plan.range.include_upper   = true;
                plan.range.upper_bound     = cond.get_value();
            } else {
                const auto& values = cond.get_value();
                if (!values.is_array() || values.size() < 2) {
                    break;
                }

                plan.range.has_lower_bound = true;
                plan.range.include_lower   = true;
                plan.range.lower_bound     = values[0];
                plan.range.has_upper_bound = true;
                plan.range.include_upper   = true;
                plan.range.upper_bound     = values[1];
            }
            break;
        default:
            plan.use_index = false;
            plan.plan_type = query_plan_type::full_scan;
            break;
    }

    return plan;
}

query_plan query_planner_runtime::find_best_index_for_and(const std::vector<condition>& conditions) const
{
    query_plan best_plan;
    bool       has_valid_plan = false;

    for (const auto& cond : conditions) {
        if (cond.is_logical()) {
            continue;
        }

        auto plan = plan_for_field_condition(cond);
        if (plan.use_index && (!has_valid_plan || is_better_plan(plan, best_plan))) {
            best_plan      = std::move(plan);
            has_valid_plan = true;
        }
    }

    return best_plan;
}

query_plan query_planner_runtime::plan_for_or_condition(const std::vector<condition>& conditions) const
{
    query_plan plan;
    mc::string common_field;
    if (!find_common_field_for_or(conditions, common_field)) {
        return plan;
    }

    mc::variants values;
    for (const auto& cond : conditions) {
        if (cond.is_logical() || cond.get_field() != common_field || cond.get_op() != compare_op::eq) {
            return plan;
        }
        values.push_back(cond.get_value());
    }

    if (values.empty()) {
        return plan;
    }

    auto index_id = m_metadata.find_best_index_id(common_field);
    if (index_id <= 0) {
        return plan;
    }

    plan.use_index = true;
    plan.plan_type = query_plan_type::index_exact_match;
    plan.index_id  = index_id;
    plan.values    = std::move(values);
    plan.fields.push_back(common_field);
    return plan;
}

bool query_planner_runtime::find_common_field_for_or(const std::vector<condition>& conditions,
                                                     mc::string&                   out_field) const
{
    if (conditions.empty() || conditions[0].is_logical()) {
        return false;
    }

    out_field = mc::string(conditions[0].get_field());
    for (size_t i = 1; i < conditions.size(); ++i) {
        if (conditions[i].is_logical() || conditions[i].get_field() != out_field) {
            return false;
        }
    }

    return true;
}

bool query_planner_runtime::is_better_plan(const query_plan& a, const query_plan& b) const
{
    if (a.plan_type == query_plan_type::index_exact_match && b.plan_type != query_plan_type::index_exact_match) {
        return true;
    }

    if (a.plan_type == query_plan_type::index_range && b.plan_type == query_plan_type::full_scan) {
        return true;
    }

    if (a.plan_type == b.plan_type && a.use_index && b.use_index) {
        const auto* a_index = m_metadata.get_index_by_id(a.index_id);
        const auto* b_index = m_metadata.get_index_by_id(b.index_id);
        if (a_index != nullptr && b_index != nullptr && a_index->is_unique && !b_index->is_unique) {
            return true;
        }
    }

    return false;
}

} // namespace mc::db::query
