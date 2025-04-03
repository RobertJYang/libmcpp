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

#ifndef MC_DATABASE_QUERY_PLANNER_H
#define MC_DATABASE_QUERY_PLANNER_H

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <mc/database/query/builder.h>
#include <mc/database/query/condition.h>
#include <mc/database/query/metadata.h>
#include <mc/dict.h>
#include <mc/variant.h>

namespace mc::database::query {

/**
 * 查询计划类型枚举
 */
enum class query_plan_type {
    full_scan,         // 全表扫描
    index_exact_match, // 索引精确匹配
    index_range,       // 索引范围查询
    composite_key,     // 复合键查询
    empty_result       // 空结果查询（优化，当可以确定查询结果一定为空时使用）
};

/**
 * 查询范围类型
 */
struct query_range {
    bool        has_lower_bound = false; // 是否有下界
    bool        has_upper_bound = false; // 是否有上界
    bool        include_lower   = false; // 是否包含下界
    bool        include_upper   = false; // 是否包含上界
    mc::variant lower_bound;             // 下界值
    mc::variant upper_bound;             // 上界值
};

/**
 * 查询计划，描述如何执行查询
 */
struct query_plan {
    query_plan_type               plan_type = query_plan_type::full_scan; // 计划类型
    size_t                        index_id  = 0;                          // 使用的索引ID
    std::vector<std::string_view> fields;                                 // 相关字段
    mc::variant                   key_value;                              // 索引键值(用于精确匹配)
    query_range                   range;                                  // 查询范围(用于范围查询)
    std::vector<mc::variant>      values;                                 // 多个值(用于IN查询)
    bool                          use_index = false;                      // 是否使用索引
};

/**
 * 查询规划器，分析查询条件并生成最佳查询计划
 * @tparam ObjectType 对象类型
 */
template <typename ObjectType>
class query_planner {
public:
    /**
     * 构造函数
     * @param metadata 表的索引元数据
     */
    explicit query_planner(const table_index_metadata<ObjectType>& metadata)
        : m_metadata(metadata) {
    }

    /**
     * 为字典查询生成计划
     * @param dict 查询字典
     * @return 查询计划
     */
    query_plan plan_for_dict(const mc::dict& dict) const {
        // 如果字典为空，返回全表扫描计划
        if (dict.empty()) {
            return create_full_scan_plan();
        }

        // 分析字典条件，找出最佳索引
        std::optional<size_t> best_index_id;
        float                 best_score = 0.0f;

        for (const auto& entry : dict) {
            std::string_view   field_name = entry.key;
            const mc::variant& value      = entry.value;

            // 查找该字段的索引
            auto indices = m_metadata.find_indices_by_field(field_name);
            for (const auto* index : indices) {
                float score = evaluate_index_for_eq(index, field_name);
                if (score > best_score) {
                    best_score    = score;
                    best_index_id = index->index_id;
                }
            }
        }

        // 根据最佳索引创建查询计划
        if (best_index_id.has_value() && best_score > 0) {
            const auto* index = m_metadata.get_index_by_id(best_index_id.value());
            if (index && index->field_names.size() > 0) {
                auto field_name = index->field_names[0];

                // 创建精确匹配查询计划
                query_plan plan;
                plan.plan_type = query_plan_type::index_exact_match;
                plan.index_id  = index->index_id;
                plan.fields.push_back(field_name);
                plan.key_value = dict.at(std::string(field_name));
                plan.use_index = true;

                return plan;
            }
        }

        // 如果没有找到合适的索引，返回全表扫描计划
        return create_full_scan_plan();
    }

    /**
     * 为复杂查询生成计划
     * @param builder 查询构建器
     * @return 查询计划
     */
    query_plan plan_for_query(const query_builder& builder) const {
        // 如果查询为空，返回全表扫描计划
        if (builder.is_empty()) {
            return create_full_scan_plan();
        }

        // 分析所有条件
        const auto& conditions = builder.get_conditions();

        // 记录每个字段的条件
        std::unordered_map<std::string_view, std::vector<std::pair<logical_op, condition>>>
            field_conditions;

        for (const auto& [op, cond] : conditions) {
            field_conditions[cond.get_field()].push_back({op, cond});
        }

        // 评估每个字段的索引可用性
        std::optional<size_t> best_index_id;
        float                 best_score = 0.0f;
        std::string_view      best_field;

        for (const auto& [field, conds] : field_conditions) {
            auto indices = m_metadata.find_indices_by_field(field);

            for (const auto* index : indices) {
                float score = evaluate_index_for_conditions(index, field, conds);
                if (score > best_score) {
                    best_score    = score;
                    best_index_id = index->index_id;
                    best_field    = field;
                }
            }
        }

        // 根据最佳索引创建查询计划
        if (best_index_id.has_value() && best_score > 0) {
            return create_best_plan_for_conditions(best_index_id.value(), best_field,
                                                   field_conditions[best_field]);
        }

        // 如果没有找到合适的索引，返回全表扫描计划
        return create_full_scan_plan();
    }

private:
    /**
     * 创建全表扫描计划
     * @return 全表扫描查询计划
     */
    query_plan create_full_scan_plan() const {
        query_plan plan;
        plan.plan_type = query_plan_type::full_scan;
        plan.use_index = false;
        return plan;
    }

    /**
     * 评估索引对相等条件的适用性
     * @param index 索引元数据
     * @param field 字段名
     * @return 评分，越高表示越适合使用
     */
    float evaluate_index_for_eq(const index_metadata* index, std::string_view field) const {
        // 如果字段不在该索引中，不可用
        if (std::find(index->field_names.begin(), index->field_names.end(), field) ==
            index->field_names.end()) {
            return 0.0f;
        }

        // 如果是复合键，并且字段不是第一个字段，可能无法高效使用
        if (index->extractor_type == key_extractor_type::composite_key &&
            index->field_names[0] != field) {
            return 0.1f; // 可能可以部分使用
        }

        // 唯一索引优先
        if (index->is_unique) {
            return 1.0f;
        }

        // 非唯一索引次之
        return 0.8f;
    }

    /**
     * 评估索引对一组条件的适用性
     * @param index 索引元数据
     * @param field 字段名
     * @param conditions 字段上的条件列表
     * @return 评分，越高表示越适合使用
     */
    float evaluate_index_for_conditions(
        const index_metadata* index, std::string_view field,
        const std::vector<std::pair<logical_op, condition>>& conditions) const {
        // 如果字段不在该索引中，不可用
        if (std::find(index->field_names.begin(), index->field_names.end(), field) ==
            index->field_names.end()) {
            return 0.0f;
        }

        // 如果是复合键，并且字段不是第一个字段，可能无法高效使用
        if (index->extractor_type == key_extractor_type::composite_key &&
            index->field_names[0] != field) {
            return 0.1f; // 可能可以部分使用
        }

        // 基础分数
        float base_score = index->is_unique ? 1.0f : 0.8f;

        // 检查条件类型，有些条件更适合索引
        bool has_eq    = false;
        bool has_range = false;
        bool has_in    = false;

        for (const auto& [op, cond] : conditions) {
            if (op != logical_op::AND) {
                // 目前只考虑AND条件
                continue;
            }

            switch (cond.get_op()) {
            case compare_op::eq:
                has_eq = true;
                break;

            case compare_op::gt:
            case compare_op::ge:
            case compare_op::lt:
            case compare_op::le:
            case compare_op::between:
                has_range = true;
                break;

            case compare_op::in:
                has_in = true;
                break;

            default:
                // 其他条件不适合直接使用索引
                break;
            }
        }

        // 等值条件最适合索引
        if (has_eq) {
            return base_score;
        }

        // IN条件次之
        if (has_in) {
            return base_score * 0.9f;
        }

        // 范围条件再次之
        if (has_range) {
            return base_score * 0.8f;
        }

        // 其他情况，索引帮助较小
        return base_score * 0.5f;
    }

    /**
     * 为给定字段上的条件创建最佳查询计划
     * @param index_id 索引ID
     * @param field 字段名
     * @param conditions 条件列表
     * @return 查询计划
     */
    query_plan create_best_plan_for_conditions(
        size_t index_id, std::string_view field,
        const std::vector<std::pair<logical_op, condition>>& conditions) const {
        const auto* index = m_metadata.get_index_by_id(index_id);
        if (!index) {
            return create_full_scan_plan();
        }

        // 检查是否为复合键索引
        bool is_composite = (index->extractor_type == key_extractor_type::composite_key);

        if (is_composite && index->field_names.size() > 1) {
            return create_composite_key_plan(index_id, conditions);
        }

        // 先看是否有等值条件
        for (const auto& [op, cond] : conditions) {
            if (op == logical_op::AND && cond.get_op() == compare_op::eq) {
                // 创建精确匹配查询计划
                query_plan plan;
                plan.plan_type = query_plan_type::index_exact_match;
                plan.index_id  = index_id;
                plan.fields.push_back(field);
                plan.key_value = cond.get_value();
                plan.use_index = true;

                return plan;
            }
        }

        // 再看是否有IN条件
        for (const auto& [op, cond] : conditions) {
            if (op == logical_op::AND && cond.get_op() == compare_op::in) {
                query_plan plan;
                plan.plan_type = query_plan_type::index_exact_match;
                plan.index_id  = index_id;
                plan.fields.push_back(field);

                // 从IN条件中提取值列表
                if (cond.get_value().is_array()) {
                    plan.values = cond.get_value().as_array();

                    // 优化：如果IN列表只有一个值，转换为精确匹配
                    if (plan.values.size() == 1) {
                        plan.key_value = plan.values[0];
                        plan.values.clear();
                    }
                }

                plan.use_index = true;

                return plan;
            }
        }

        // 最后看是否有范围条件
        query_range range;

        // 记录每种比较运算符的最优条件
        std::map<compare_op, const condition*> best_conditions;

        for (const auto& [op, cond] : conditions) {
            if (op != logical_op::AND) {
                continue;
            }

            // 保存最优条件
            auto it = best_conditions.find(cond.get_op());
            if (it == best_conditions.end() ||
                is_better_condition(cond, *(it->second), cond.get_op())) {
                best_conditions[cond.get_op()] = &cond;
            }
        }

        // 应用最优条件
        for (const auto& [op, cond_ptr] : best_conditions) {
            const auto& cond = *cond_ptr;

            switch (op) {
            case compare_op::gt:
                if (!range.has_lower_bound ||
                    (range.has_lower_bound && range.include_lower &&
                     compare_values(cond.get_value(), range.lower_bound) > 0)) {
                    range.has_lower_bound = true;
                    range.include_lower   = false;
                    range.lower_bound     = cond.get_value();
                }
                break;

            case compare_op::ge:
                if (!range.has_lower_bound || (range.has_lower_bound && !range.include_lower) ||
                    (range.has_lower_bound && range.include_lower &&
                     compare_values(cond.get_value(), range.lower_bound) > 0)) {
                    range.has_lower_bound = true;
                    range.include_lower   = true;
                    range.lower_bound     = cond.get_value();
                }
                break;

            case compare_op::lt:
                if (!range.has_upper_bound ||
                    (range.has_upper_bound && range.include_upper &&
                     compare_values(cond.get_value(), range.upper_bound) < 0)) {
                    range.has_upper_bound = true;
                    range.include_upper   = false;
                    range.upper_bound     = cond.get_value();
                }
                break;

            case compare_op::le:
                if (!range.has_upper_bound || (range.has_upper_bound && !range.include_upper) ||
                    (range.has_upper_bound && range.include_upper &&
                     compare_values(cond.get_value(), range.upper_bound) < 0)) {
                    range.has_upper_bound = true;
                    range.include_upper   = true;
                    range.upper_bound     = cond.get_value();
                }
                break;

            case compare_op::between:
                // 从BETWEEN条件中提取上下界
                if (cond.get_value().is_dict()) {
                    const auto& dict = cond.get_value().as_dict();
                    if (dict.contains("min") && dict.contains("max")) {
                        if (!range.has_lower_bound ||
                            (range.has_lower_bound &&
                             compare_values(dict.at("min"), range.lower_bound) > 0)) {
                            range.has_lower_bound = true;
                            range.include_lower   = true;
                            range.lower_bound     = dict.at("min");
                        }

                        if (!range.has_upper_bound ||
                            (range.has_upper_bound &&
                             compare_values(dict.at("max"), range.upper_bound) < 0)) {
                            range.has_upper_bound = true;
                            range.include_upper   = true;
                            range.upper_bound     = dict.at("max");
                        }
                    }
                }
                break;

            default:
                break;
            }
        }

        // 检查范围条件是否有效
        if (range.has_lower_bound && range.has_upper_bound) {
            // 如果下界大于上界，查询结果必然为空
            if (compare_values(range.lower_bound, range.upper_bound) > 0) {
                query_plan plan;
                plan.plan_type = query_plan_type::empty_result;
                plan.index_id  = index_id;
                plan.use_index = true;
                return plan;
            }

            // 如果下界等于上界，并且至少一个边界是开区间，查询结果必然为空
            if (compare_values(range.lower_bound, range.upper_bound) == 0 &&
                (!range.include_lower || !range.include_upper)) {
                query_plan plan;
                plan.plan_type = query_plan_type::empty_result;
                plan.index_id  = index_id;
                plan.use_index = true;
                return plan;
            }
        }

        // 如果找到了范围条件
        if (range.has_lower_bound || range.has_upper_bound) {
            query_plan plan;
            plan.plan_type = query_plan_type::index_range;
            plan.index_id  = index_id;
            plan.fields.push_back(field);
            plan.range     = range;
            plan.use_index = true;

            return plan;
        }

        // 如果没有找到合适的条件，返回全表扫描计划
        return create_full_scan_plan();
    }

    /**
     * 为复合键创建查询计划
     * @param index_id 索引ID
     * @param conditions 条件列表
     * @return 查询计划
     */
    query_plan create_composite_key_plan(
        size_t index_id, const std::vector<std::pair<logical_op, condition>>& conditions) const {
        const auto* index = m_metadata.get_index_by_id(index_id);
        if (!index || index->field_names.size() <= 1) {
            return create_full_scan_plan();
        }

        // 创建查询计划
        query_plan plan;
        plan.plan_type = query_plan_type::composite_key;
        plan.index_id  = index_id;
        plan.use_index = true;

        // 为每个字段提取条件
        std::unordered_map<std::string, const condition*> field_conditions;

        for (const auto& [op, cond] : conditions) {
            if (op == logical_op::AND && cond.get_op() == compare_op::eq) {
                const std::string field_name(cond.get_field());

                // 只保留最后一个条件（如果有多个）
                field_conditions[field_name] = &cond;
            }
        }

        // 检查是否有足够的条件覆盖复合键的前缀
        bool             has_all_leading_fields = true;
        mc::mutable_dict composite_key;

        for (size_t i = 0; i < index->field_names.size(); ++i) {
            const auto& field = index->field_names[i];

            // 添加字段到查询计划
            plan.fields.push_back(field);

            auto it = field_conditions.find(field);
            if (it != field_conditions.end()) {
                // 有这个字段的条件，添加到复合键
                composite_key[field] = it->second->get_value();
            } else {
                // 缺少字段条件，只能使用前面的字段
                has_all_leading_fields = false;
                break;
            }
        }

        // 如果有完整的复合键匹配
        if (has_all_leading_fields) {
            plan.key_value = composite_key;
            return plan;
        }

        // 如果复合键不完整但至少有第一个字段
        if (!composite_key.empty()) {
            // 将复合键转换为部分键查询
            if (composite_key.size() == 1) {
                // 只有一个字段，转换为简单索引查询
                plan.plan_type = query_plan_type::index_exact_match;
                plan.fields    = {plan.fields[0]};
                plan.key_value = composite_key.begin()->value;
            } else {
                // 多个字段，保持为复合键查询
                plan.key_value = composite_key;
            }
            return plan;
        }

        // 没有足够的条件，回退到全表扫描
        return create_full_scan_plan();
    }

    /**
     * 比较两个条件的有效性
     * @param cond1 条件1
     * @param cond2 条件2
     * @param op 比较运算符
     * @return 如果条件1比条件2更优，返回true
     */
    bool is_better_condition(const condition& cond1, const condition& cond2, compare_op op) const {
        // 对于不同类型的比较运算符，有不同的优化策略
        switch (op) {
        case compare_op::gt:
        case compare_op::ge:
            // 对于下界，值越大越好
            return compare_values(cond1.get_value(), cond2.get_value()) > 0;

        case compare_op::lt:
        case compare_op::le:
            // 对于上界，值越小越好
            return compare_values(cond1.get_value(), cond2.get_value()) < 0;

        default:
            // 默认条件下，保持原样
            return false;
        }
    }

    /**
     * 比较两个值的大小
     * @param val1 值1
     * @param val2 值2
     * @return 比较结果：负数表示val1小于val2，0表示相等，正数表示val1大于val2
     */
    int compare_values(const mc::variant& val1, const mc::variant& val2) const {
        // 简单实现，主要处理基本类型
        if (val1.is_integer() && val2.is_integer()) {
            int v1 = val1.as<int>();
            int v2 = val2.as<int>();
            return v1 - v2;
        } else if (val1.is_double() && val2.is_double()) {
            double v1 = val1.as<double>();
            double v2 = val2.as<double>();
            return v1 < v2 ? -1 : (v1 > v2 ? 1 : 0);
        } else if (val1.is_string() && val2.is_string()) {
            std::string v1 = val1.as<std::string>();
            std::string v2 = val2.as<std::string>();
            return v1.compare(v2);
        } else if (val1.is_bool() && val2.is_bool()) {
            bool v1 = val1.as<bool>();
            bool v2 = val2.as<bool>();
            return v1 == v2 ? 0 : (v1 ? 1 : -1);
        }

        // 不同类型之间难以直接比较，返回0表示相等
        return 0;
    }

    const table_index_metadata<ObjectType>& m_metadata;
};

} // namespace mc::database::query

#endif // MC_DATABASE_QUERY_PLANNER_H