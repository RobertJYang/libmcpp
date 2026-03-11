/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/**
 * @file planner.h
 * @brief 提供查询计划生成和优化功能
 */
#ifndef MC_DATABASE_QUERY_PLANNER_H
#define MC_DATABASE_QUERY_PLANNER_H

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <mc/db/query/builder.h>
#include <mc/db/query/condition.h>
#include <mc/db/query/metadata.h>
#include <mc/variant.h>

namespace mc::db::query {

/**
 * 查询计划类型枚举
 */
enum class query_plan_type {
    full_scan,         // 全表扫描
    index_exact_match, // 索引精确匹配
    index_range,       // 索引范围查询
    composite_key,     // 复合键查询
    empty_result       // 空结果优化
};

/**
 * 查询范围结构体
 */
struct query_range {
    bool        has_lower_bound = false; // 是否有下界
    bool        include_lower   = false; // 是否包含下界
    mc::variant lower_bound;             // 下界值
    bool        has_upper_bound = false; // 是否有上界
    bool        include_upper   = false; // 是否包含上界
    mc::variant upper_bound;             // 上界值
};

/**
 * 查询计划结构体
 */
struct query_plan {
    query_plan_type               plan_type = query_plan_type::full_scan; // 计划类型
    bool                          use_index = false;                      // 是否使用索引
    size_t                        index_id  = 0;                          // 索引ID
    mc::variant                   key_value;                              // 用于精确匹配的键值
    mc::variants                  values;                                 // 用于IN查询的多个值
    query_range                   range;                                  // 用于范围查询的范围
    std::vector<std::string_view> fields;                                 // 涉及的字段
};

/**
 * 查询计划生成器，负责生成优化的查询执行计划
 *
 * @tparam ObjectType 对象类型
 */
template <typename ObjectType>
class query_planner {
public:
    /**
     * 构造函数 - 接受元数据引用
     *
     * @param metadata 表元数据
     */
    explicit query_planner(const table_index_metadata<ObjectType>& metadata)
        : m_metadata(metadata)
    {
    }

    /**
     * 构造函数 - 接受元数据共享指针
     *
     * @param reflection_ptr 表元数据共享指针
     */
    explicit query_planner(const std::shared_ptr<table_index_metadata<ObjectType>>& reflection_ptr)
        : m_metadata(*reflection_ptr)
    {
    }

    /**
     * 为查询生成执行计划
     *
     * @param builder 查询构建器
     * @return 查询计划
     */
    query_plan plan_for_query(const query_builder& builder) const
    {
        query_plan plan;

        // 如果没有条件，则返回全表扫描计划
        if (!builder.has_condition()) {
            return plan;
        }

        // 获取主条件
        const auto& condition = builder.get_condition();

        // 尝试为条件找到合适的索引
        return find_matching_index(condition);
    }

    /**
     * 为条件生成执行计划
     *
     * @param cond 查询条件
     * @return 查询计划
     */
    query_plan generate_plan(const condition& cond) const
    {
        // 创建默认的全表扫描计划
        query_plan plan;

        // 尝试为条件找到合适的索引
        return find_matching_index(cond);
    }

private:
    /**
     * 为条件找到合适的索引
     *
     * @param condition 条件
     * @return 查询计划
     */
    query_plan find_matching_index(const condition& condition) const
    {
        query_plan plan;

        // 处理逻辑条件
        if (condition.is_logical()) {
            logical_op  op             = condition.get_logical_op();
            const auto& sub_conditions = condition.get_conditions();

            if (op == logical_op::AND) {
                // 对于AND条件，尝试从子条件中找到最优索引
                return find_best_index_for_and(sub_conditions);
            } else if (op == logical_op::NOT && sub_conditions.size() == 1) {
                // 处理单个条件的NOT (即 NOT 条件)
                if (sub_conditions.empty()) {
                    return plan;
                }

                // 处理 NOT IN 条件的特殊情况
                if (sub_conditions[0].get_op() == compare_op::in) {
                    plan.plan_type = query_plan_type::full_scan;
                    return plan;
                }

                // 处理 NOT 等值条件的特殊情况
                if (sub_conditions[0].get_op() == compare_op::eq) {
                    plan.plan_type = query_plan_type::full_scan;
                    return plan;
                }

                // 其他NOT条件通常需要全表扫描
                plan.plan_type = query_plan_type::full_scan;
                return plan;
            } else {
                // OR条件通常需要单独处理
                return plan_for_or_condition(sub_conditions);
            }
        }

        // 处理字段条件（非逻辑条件）
        return plan_for_field_condition(condition);
    }

    /**
     * 为字段条件创建查询计划
     *
     * @param cond 字段条件
     * @return 查询计划
     */
    query_plan plan_for_field_condition(const condition& cond) const
    {
        query_plan       plan;
        compare_op       op    = cond.get_op();
        std::string_view field = cond.get_field();

        // 查找字段相关的索引
        auto index_id = m_metadata.find_best_index_id(field);
        if (index_id == -1) {
            return plan;
        }

        // 设置查询计划使用索引
        plan.use_index = true;
        plan.index_id  = index_id;
        plan.fields.push_back(field);

        // 根据操作类型确定查询计划类型
        switch (op) {
        case compare_op::eq: {
            // 等值查询使用精确匹配
            plan.plan_type = query_plan_type::index_exact_match;
            plan.key_value = cond.get_value();
            break;
        }
        case compare_op::in: {
            plan.plan_type = query_plan_type::index_exact_match;
            if (cond.get_value().is_array()) {
                plan.values = cond.get_value().get_array();
            }
            break;
        }
        case compare_op::gt:
        case compare_op::ge:
        case compare_op::lt:
        case compare_op::le:
        case compare_op::between: {
            // 范围查询
            plan.plan_type = query_plan_type::index_range;

            // 设置范围
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
            } else if (op == compare_op::between) {
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
        }
        default:
            // 其他操作不适合索引优化
            plan.use_index = false;
            plan.plan_type = query_plan_type::full_scan;
            break;
        }

        return plan;
    }

    /**
     * 为AND条件寻找最佳索引
     *
     * @param conditions 条件列表
     * @return 查询计划
     */
    query_plan find_best_index_for_and(const std::vector<condition>& conditions) const
    {
        query_plan best_plan;
        bool       has_valid_plan = false;

        // 对于每个子条件，寻找最佳索引
        for (const auto& cond : conditions) {
            if (cond.is_logical()) {
                // 跳过复杂的逻辑条件
                continue;
            }

            auto plan = plan_for_field_condition(cond);

            // 如果找到了索引，评估是否比当前最佳索引更优
            if (plan.use_index) {
                if (!has_valid_plan || is_better_plan(plan, best_plan)) {
                    best_plan      = plan;
                    has_valid_plan = true;
                }
            }
        }

        // 返回最佳索引计划
        return best_plan;
    }

    /**
     * 为OR条件创建查询计划
     *
     * @param conditions 条件列表
     * @return 查询计划
     */
    query_plan plan_for_or_condition(const std::vector<condition>& conditions) const
    {
        query_plan plan;

        // 收集所有等值条件的相同字段
        std::string_view common_field;
        bool             has_common_field = find_common_field_for_or(conditions, common_field);

        if (has_common_field) {
            // 收集等值条件的所有值
            mc::variants values;
            bool         all_eq_conds = true;

            for (const auto& cond : conditions) {
                if (cond.is_logical() || cond.get_field() != common_field) {
                    all_eq_conds = false;
                    break;
                }

                if (cond.get_op() == compare_op::eq) {
                    values.push_back(cond.get_value());
                } else {
                    all_eq_conds = false;
                    break;
                }
            }

            if (all_eq_conds && !values.empty()) {
                // 可以优化为IN查询
                auto index_id = m_metadata.find_best_index_id(common_field);
                if (index_id > 0) {
                    plan.use_index = true;
                    plan.plan_type = query_plan_type::index_exact_match;
                    plan.index_id  = index_id;
                    plan.values    = std::move(values);
                    plan.fields.push_back(common_field);
                    return plan;
                }
            }
        }

        // 默认为全表扫描
        return plan;
    }

    /**
     * 查找OR条件中的公共字段
     *
     * @param conditions 条件列表
     * @param out_field 输出参数，找到的公共字段
     * @return 是否找到公共字段
     */
    bool find_common_field_for_or(const std::vector<condition>& conditions,
                                  std::string_view&             out_field) const
    {
        if (conditions.empty()) {
            return false;
        }

        // 仅处理字段条件
        if (conditions[0].is_logical()) {
            return false;
        }

        out_field = conditions[0].get_field();

        // 检查所有条件是否使用相同字段
        for (size_t i = 1; i < conditions.size(); ++i) {
            if (conditions[i].is_logical() || conditions[i].get_field() != out_field) {
                return false;
            }
        }

        return true;
    }

    /**
     * 判断计划A是否优于计划B
     *
     * @param a 计划A
     * @param b 计划B
     * @return 是否A优于B
     */
    bool is_better_plan(const query_plan& a, const query_plan& b) const
    {
        // 优先级：精确匹配 > 范围查询 > 全表扫描
        if (a.plan_type == query_plan_type::index_exact_match &&
            b.plan_type != query_plan_type::index_exact_match) {
            return true;
        }

        if (a.plan_type == query_plan_type::index_range &&
            b.plan_type == query_plan_type::full_scan) {
            return true;
        }

        // 相同类型的计划，考虑索引类型
        if (a.plan_type == b.plan_type && a.use_index && b.use_index) {
            // 查找索引信息
            const auto* a_index = m_metadata.get_index_by_id(a.index_id);
            const auto* b_index = m_metadata.get_index_by_id(b.index_id);

            if (a_index && b_index) {
                // 优先选择唯一索引
                if (a_index->is_unique && !b_index->is_unique) {
                    return true;
                }
            }
        }

        return false;
    }

    const table_index_metadata<ObjectType>& m_metadata;
};

} // namespace mc::db::query

#endif // MC_DATABASE_QUERY_PLANNER_H