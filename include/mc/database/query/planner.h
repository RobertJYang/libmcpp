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
     * 为查询构建器生成查询计划
     * @param builder 查询构建器
     * @return 查询计划
     */
    query_plan plan_for_query(const query_builder& builder) const {
        if (builder.is_empty()) {
            return create_full_scan_plan();
        }

        // 新系统：使用类型系统
        if (builder.has_where()) {
            return generate_plan_for_condition(builder.get_where());
        }

        // 如果没有条件，返回全表扫描计划
        return create_full_scan_plan();
    }

    /**
     * 为条件生成查询计划
     * @param condition 条件对象
     * @return 查询计划
     */
    query_plan generate_plan_for_condition(const std::shared_ptr<base_condition>& condition) const {
        // 基于条件类型生成查询计划
        switch (condition->get_type()) {
        case condition_type::equal_condition: {
            auto eq_cond = std::static_pointer_cast<equal_condition>(condition);

            // 检查是否有适合的索引
            auto field    = eq_cond->get_field();
            int  index_id = find_best_index_for_field(field);

            if (index_id > 0) {
                return create_best_plan_for_conditions(index_id, field, condition);
            }
            break;
        }
        case condition_type::greater_condition:
        case condition_type::greater_equal_condition:
        case condition_type::less_condition:
        case condition_type::less_equal_condition: {
            std::string field;
            if (condition->get_type() == condition_type::greater_condition) {
                auto gt_cond = std::static_pointer_cast<greater_condition>(condition);
                field        = gt_cond->get_field();
            } else if (condition->get_type() == condition_type::greater_equal_condition) {
                auto ge_cond = std::static_pointer_cast<greater_equal_condition>(condition);
                field        = ge_cond->get_field();
            } else if (condition->get_type() == condition_type::less_condition) {
                auto lt_cond = std::static_pointer_cast<less_condition>(condition);
                field        = lt_cond->get_field();
            } else if (condition->get_type() == condition_type::less_equal_condition) {
                auto le_cond = std::static_pointer_cast<less_equal_condition>(condition);
                field        = le_cond->get_field();
            }

            // 检查是否有适合的索引
            int index_id = find_best_index_for_field(field);

            if (index_id > 0) {
                return create_best_plan_for_conditions(index_id, field, condition);
            }
            break;
        }
        case condition_type::and_condition: {
            auto        and_cond   = std::static_pointer_cast<and_condition>(condition);
            const auto& conditions = and_cond->get_conditions();

            // 分析所有条件，找出最优索引
            std::unordered_map<std::string, std::vector<std::shared_ptr<base_condition>>>
                field_conditions;

            // 按字段分组条件
            for (const auto& cond : conditions) {
                std::string field;

                if (cond->get_type() == condition_type::equal_condition) {
                    auto eq_cond = std::static_pointer_cast<equal_condition>(cond);
                    field        = eq_cond->get_field();
                } else if (cond->get_type() == condition_type::greater_condition) {
                    auto gt_cond = std::static_pointer_cast<greater_condition>(cond);
                    field        = gt_cond->get_field();
                } else if (cond->get_type() == condition_type::greater_equal_condition) {
                    auto ge_cond = std::static_pointer_cast<greater_equal_condition>(cond);
                    field        = ge_cond->get_field();
                } else if (cond->get_type() == condition_type::less_condition) {
                    auto lt_cond = std::static_pointer_cast<less_condition>(cond);
                    field        = lt_cond->get_field();
                } else if (cond->get_type() == condition_type::less_equal_condition) {
                    auto le_cond = std::static_pointer_cast<less_equal_condition>(cond);
                    field        = le_cond->get_field();
                } else {
                    continue; // 跳过不支持的条件类型
                }

                field_conditions[field].push_back(cond);
            }

            // 评估每个字段的索引适用性，选择最佳字段和索引
            float       best_score    = 0.0f;
            int         best_index_id = 0;
            std::string best_field;

            for (const auto& [field, conds] : field_conditions) {
                int index_id = find_best_index_for_field(field);
                if (index_id <= 0) {
                    continue; // 没有合适的索引
                }

                const auto* index = m_metadata.get_index_by_id(index_id);
                if (!index) {
                    continue;
                }

                float score = evaluate_index_for_conditions(index, field, condition);

                if (score > best_score) {
                    best_score    = score;
                    best_index_id = index_id;
                    best_field    = field;
                }
            }

            if (best_index_id > 0) {
                return create_best_plan_for_conditions(best_index_id, best_field, condition);
            }

            break;
        }
        case condition_type::or_condition: {
            return plan_for_or_condition(condition);
        }
        default:
            break;
        }

        // 如果没有找到合适的索引，返回全表扫描计划
        return create_full_scan_plan();
    }

    /**
     * 处理OR条件的查询计划
     * @param or_condition_ptr OR条件对象
     * @return 查询计划
     */
    query_plan
    plan_for_or_condition(const std::shared_ptr<base_condition>& or_condition_ptr) const {
        auto        or_cond    = std::static_pointer_cast<or_condition>(or_condition_ptr);
        const auto& conditions = or_cond->get_conditions();

        // 检查是否所有条件都是等值条件且针对同一字段
        bool        all_equal_on_same_field = true;
        std::string field_name;

        if (!conditions.empty() && conditions[0]->get_type() == condition_type::equal_condition) {
            auto first_cond = std::static_pointer_cast<equal_condition>(conditions[0]);
            field_name      = first_cond->get_field();

            for (size_t i = 1; i < conditions.size(); ++i) {
                if (conditions[i]->get_type() != condition_type::equal_condition) {
                    all_equal_on_same_field = false;
                    break;
                }

                auto equal_cond = std::static_pointer_cast<equal_condition>(conditions[i]);
                if (equal_cond->get_field() != field_name) {
                    all_equal_on_same_field = false;
                    break;
                }
            }
        } else {
            all_equal_on_same_field = false;
        }

        // 如果是IN优化的情况
        if (all_equal_on_same_field) {
            int index_id = find_best_index_for_field(field_name);
            if (index_id > 0) {
                // 提取所有值
                std::vector<mc::variant> values;
                for (const auto& cond : conditions) {
                    auto eq_cond = std::static_pointer_cast<equal_condition>(cond);
                    values.push_back(eq_cond->get_value());
                }

                // 创建IN查询计划
                query_plan plan;
                plan.plan_type = query_plan_type::index_exact_match;
                plan.index_id  = index_id;
                plan.fields.push_back(field_name);
                plan.values    = values;
                plan.use_index = true;

                return plan;
            }
        }

        // 尝试对每个OR条件找一个索引
        // 如果所有OR条件都能找到索引，理论上我们可以合并结果
        // 但是目前的实现简单地回退到全表扫描
        return create_full_scan_plan();
    }

    /**
     * 为指定字段查找最佳索引
     * @param field 字段名
     * @return 索引ID，如果没有匹配的索引则返回0
     */
    int find_best_index_for_field(std::string_view field) const {
        auto indices = m_metadata.find_indices_by_field(field);
        if (indices.empty()) {
            return 0;
        }

        // 如果有多个索引，优先选择唯一索引
        for (const auto* index : indices) {
            if (index->is_unique && index->index_id > 0) {
                return static_cast<int>(index->index_id);
            }
        }

        // 其次选择第一个字段匹配的索引
        for (const auto* index : indices) {
            if (!index->field_names.empty() && index->field_names[0] == field &&
                index->index_id > 0) {
                return static_cast<int>(index->index_id);
            }
        }

        // 最后选择任意匹配的索引
        for (const auto* index : indices) {
            if (index->index_id > 0) {
                return static_cast<int>(index->index_id);
            }
        }

        return 0;
    }

private:
    /**
     * 为空结果创建查询计划
     */
    query_plan create_empty_result_plan() const {
        query_plan plan;
        plan.use_index = false;
        plan.plan_type = query_plan_type::empty_result;
        return plan;
    }

    /**
     * 为全表扫描创建查询计划
     */
    query_plan create_full_scan_plan() const {
        query_plan plan;
        plan.use_index = false;
        plan.plan_type = query_plan_type::full_scan;
        return plan;
    }

    /**
     * 查找匹配的索引
     */
    const index_metadata* find_matching_index(const query_builder& builder) const {
        if (!builder.has_where()) {
            return nullptr;
        }

        // 处理各种条件类型
        switch (builder.get_where()->get_type()) {
        case condition_type::equal_condition: {
            auto eq_cond = std::static_pointer_cast<equal_condition>(builder.get_where());
            auto indices = m_metadata.find_indices_by_field(eq_cond->get_field());
            if (!indices.empty()) {
                return indices[0];
            }
            break;
        }
        case condition_type::greater_condition: {
            auto gt_cond = std::static_pointer_cast<greater_condition>(builder.get_where());
            auto indices = m_metadata.find_indices_by_field(gt_cond->get_field());
            if (!indices.empty()) {
                return indices[0];
            }
            break;
        }
        case condition_type::greater_equal_condition: {
            auto ge_cond = std::static_pointer_cast<greater_equal_condition>(builder.get_where());
            auto indices = m_metadata.find_indices_by_field(ge_cond->get_field());
            if (!indices.empty()) {
                return indices[0];
            }
            break;
        }
        case condition_type::less_condition: {
            auto lt_cond = std::static_pointer_cast<less_condition>(builder.get_where());
            auto indices = m_metadata.find_indices_by_field(lt_cond->get_field());
            if (!indices.empty()) {
                return indices[0];
            }
            break;
        }
        case condition_type::less_equal_condition: {
            auto le_cond = std::static_pointer_cast<less_equal_condition>(builder.get_where());
            auto indices = m_metadata.find_indices_by_field(le_cond->get_field());
            if (!indices.empty()) {
                return indices[0];
            }
            break;
        }
        case condition_type::and_condition: {
            // 对于AND条件，寻找最优的索引
            auto        and_cond   = std::static_pointer_cast<and_condition>(builder.get_where());
            const auto& conditions = and_cond->get_conditions();

            float                 best_score = 0.0f;
            const index_metadata* best_index = nullptr;

            for (const auto& condition : conditions) {
                std::string field_name;
                bool        is_range = false;

                // 获取条件中的字段名和类型
                switch (condition->get_type()) {
                case condition_type::equal_condition:
                    field_name = std::static_pointer_cast<equal_condition>(condition)->get_field();
                    is_range   = false;
                    break;
                case condition_type::greater_condition:
                    field_name =
                        std::static_pointer_cast<greater_condition>(condition)->get_field();
                    is_range = true;
                    break;
                case condition_type::greater_equal_condition:
                    field_name =
                        std::static_pointer_cast<greater_equal_condition>(condition)->get_field();
                    is_range = true;
                    break;
                case condition_type::less_condition:
                    field_name = std::static_pointer_cast<less_condition>(condition)->get_field();
                    is_range   = true;
                    break;
                case condition_type::less_equal_condition:
                    field_name =
                        std::static_pointer_cast<less_equal_condition>(condition)->get_field();
                    is_range = true;
                    break;
                default:
                    continue;
                }

                // 查找该字段的索引
                auto indices = m_metadata.find_indices_by_field(field_name);
                for (const auto* index : indices) {
                    float score;
                    if (is_range) {
                        score = evaluate_index_for_range(index, field_name);
                    } else {
                        score = evaluate_index_for_eq(index, field_name);
                    }

                    if (score > best_score) {
                        best_score = score;
                        best_index = index;
                    }
                }
            }

            return best_index;
        }
        default:
            break;
        }

        return nullptr;
    }

    /**
     * 检查是否是精确匹配
     */
    bool is_exact_match(const query_builder& builder, const index_metadata* index) const {
        if (!builder.has_where()) {
            return false;
        }

        // 简单实现，只处理equal_condition
        if (builder.get_where()->get_type() == condition_type::equal_condition) {
            auto eq_cond = std::static_pointer_cast<equal_condition>(builder.get_where());
            // 检查字段是否与索引的第一个字段匹配
            return !index->field_names.empty() && eq_cond->get_field() == index->field_names[0];
        }

        return false;
    }

    /**
     * 检查是否是范围查询
     */
    bool is_range_query(const query_builder& builder, const index_metadata* index) const {
        if (!builder.has_where() || index->field_names.empty()) {
            return false;
        }

        // 检查条件类型是否为比较运算符（大于、小于等）
        if (builder.get_where()->get_type() == condition_type::greater_condition ||
            builder.get_where()->get_type() == condition_type::greater_equal_condition ||
            builder.get_where()->get_type() == condition_type::less_condition ||
            builder.get_where()->get_type() == condition_type::less_equal_condition) {
            std::string field_name;

            // 获取条件中的字段名
            switch (builder.get_where()->get_type()) {
            case condition_type::greater_condition:
                field_name =
                    std::static_pointer_cast<greater_condition>(builder.get_where())->get_field();
                break;
            case condition_type::greater_equal_condition:
                field_name = std::static_pointer_cast<greater_equal_condition>(builder.get_where())
                                 ->get_field();
                break;
            case condition_type::less_condition:
                field_name =
                    std::static_pointer_cast<less_condition>(builder.get_where())->get_field();
                break;
            case condition_type::less_equal_condition:
                field_name = std::static_pointer_cast<less_equal_condition>(builder.get_where())
                                 ->get_field();
                break;
            default:
                return false;
            }

            // 检查字段是否与索引的第一个字段匹配
            return field_name == index->field_names[0];
        }

        // 检查AND条件中是否包含范围条件
        if (builder.get_where()->get_type() == condition_type::and_condition) {
            auto        and_cond   = std::static_pointer_cast<and_condition>(builder.get_where());
            const auto& conditions = and_cond->get_conditions();

            bool has_range_condition = false;

            for (const auto& condition : conditions) {
                if (condition->get_type() == condition_type::greater_condition ||
                    condition->get_type() == condition_type::greater_equal_condition ||
                    condition->get_type() == condition_type::less_condition ||
                    condition->get_type() == condition_type::less_equal_condition) {
                    std::string field_name;

                    // 获取条件中的字段名
                    switch (condition->get_type()) {
                    case condition_type::greater_condition:
                        field_name =
                            std::static_pointer_cast<greater_condition>(condition)->get_field();
                        break;
                    case condition_type::greater_equal_condition:
                        field_name = std::static_pointer_cast<greater_equal_condition>(condition)
                                         ->get_field();
                        break;
                    case condition_type::less_condition:
                        field_name =
                            std::static_pointer_cast<less_condition>(condition)->get_field();
                        break;
                    case condition_type::less_equal_condition:
                        field_name =
                            std::static_pointer_cast<less_equal_condition>(condition)->get_field();
                        break;
                    default:
                        continue;
                    }

                    // 检查字段是否与索引的第一个字段匹配
                    if (field_name == index->field_names[0]) {
                        has_range_condition = true;
                        break;
                    }
                }
            }

            return has_range_condition;
        }

        return false;
    }

    /**
     * 检查是否是复合键查询
     */
    bool is_composite_key_query(const query_builder& builder, const index_metadata* index) const {
        // 目前未实现，以后添加
        return false;
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
     * 评估索引对范围条件的适用性
     * @param index 索引元数据
     * @param field 字段名
     * @return 评分，越高表示越适合使用
     */
    float evaluate_index_for_range(const index_metadata* index, std::string_view field) const {
        // 如果字段不在该索引中，不可用
        if (std::find(index->field_names.begin(), index->field_names.end(), field) ==
            index->field_names.end()) {
            return 0.0f;
        }

        // 如果字段是索引的第一个字段，则可用性最高
        if (!index->field_names.empty() && index->field_names[0] == field) {
            return 8.0f;
        }

        // 否则，可用性较低
        return 2.0f;
    }

    /**
     * 评估索引对一组条件的适用性
     * @param index 索引元数据
     * @param field 字段名
     * @param where_condition 条件对象
     * @return 评分，越高表示越适合使用
     */
    float
    evaluate_index_for_conditions(const index_metadata* index, std::string_view field,
                                  const std::shared_ptr<base_condition>& where_condition) const {
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

        // 根据条件类型评估索引适用性
        switch (where_condition->get_type()) {
        case condition_type::equal_condition:
            has_eq = true;
            break;
        case condition_type::greater_condition:
        case condition_type::greater_equal_condition:
        case condition_type::less_condition:
        case condition_type::less_equal_condition:
            has_range = true;
            break;
        case condition_type::and_condition: {
            auto and_cond = std::static_pointer_cast<and_condition>(where_condition);
            for (const auto& sub_cond : and_cond->get_conditions()) {
                switch (sub_cond->get_type()) {
                case condition_type::equal_condition:
                    has_eq = true;
                    break;
                case condition_type::greater_condition:
                case condition_type::greater_equal_condition:
                case condition_type::less_condition:
                case condition_type::less_equal_condition:
                    has_range = true;
                    break;
                default:
                    break;
                }
            }
            break;
        }
        case condition_type::or_condition: {
            auto or_cond = std::static_pointer_cast<or_condition>(where_condition);

            // 检查是否所有条件都是等值条件且针对同一字段
            bool        all_equal_on_same_field = true;
            std::string field_name;

            if (!or_cond->get_conditions().empty() &&
                or_cond->get_conditions()[0]->get_type() == condition_type::equal_condition) {
                auto first_cond =
                    std::static_pointer_cast<equal_condition>(or_cond->get_conditions()[0]);
                field_name = first_cond->get_field();

                for (size_t i = 1; i < or_cond->get_conditions().size(); ++i) {
                    if (or_cond->get_conditions()[i]->get_type() !=
                        condition_type::equal_condition) {
                        all_equal_on_same_field = false;
                        break;
                    }

                    auto equal_cond =
                        std::static_pointer_cast<equal_condition>(or_cond->get_conditions()[i]);
                    if (equal_cond->get_field() != field_name) {
                        all_equal_on_same_field = false;
                        break;
                    }
                }
            } else {
                all_equal_on_same_field = false;
            }

            if (all_equal_on_same_field) {
                has_in = true;
            }
            break;
        }
        default:
            break;
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
     * @param where_condition 条件对象
     * @return 查询计划
     */
    query_plan
    create_best_plan_for_conditions(size_t index_id, std::string_view field,
                                    const std::shared_ptr<base_condition>& where_condition) const {
        const auto* index = m_metadata.get_index_by_id(index_id);
        if (!index) {
            return create_full_scan_plan();
        }

        // 检查是否为复合键索引
        bool is_composite = (index->extractor_type == key_extractor_type::composite_key);

        if (is_composite && index->field_names.size() > 1) {
            return create_composite_key_plan(index_id, where_condition);
        }

        // 处理等值条件
        if (where_condition->get_type() == condition_type::equal_condition) {
            auto eq_cond = std::static_pointer_cast<equal_condition>(where_condition);

            // 创建精确匹配查询计划
            query_plan plan;
            plan.plan_type = query_plan_type::index_exact_match;
            plan.index_id  = index_id;
            plan.fields.push_back(field);
            plan.key_value = eq_cond->get_value();
            plan.use_index = true;

            return plan;
        }

        // 处理AND条件
        if (where_condition->get_type() == condition_type::and_condition) {
            auto and_cond = std::static_pointer_cast<and_condition>(where_condition);

            // 查找等值条件
            for (const auto& condition : and_cond->get_conditions()) {
                if (condition->get_type() == condition_type::equal_condition) {
                    auto eq_cond = std::static_pointer_cast<equal_condition>(condition);

                    // 检查字段是否匹配
                    if (eq_cond->get_field() == field) {
                        // 创建精确匹配查询计划
                        query_plan plan;
                        plan.plan_type = query_plan_type::index_exact_match;
                        plan.index_id  = index_id;
                        plan.fields.push_back(field);
                        plan.key_value = eq_cond->get_value();
                        plan.use_index = true;

                        return plan;
                    }
                }
            }

            // 处理范围条件
            query_range range;

            for (const auto& condition : and_cond->get_conditions()) {
                switch (condition->get_type()) {
                case condition_type::greater_condition: {
                    auto gt_cond = std::static_pointer_cast<greater_condition>(condition);
                    if (gt_cond->get_field() == field) {
                        if (!range.has_lower_bound ||
                            (range.has_lower_bound && range.include_lower &&
                             compare_values(gt_cond->get_value(), range.lower_bound) > 0)) {
                            range.has_lower_bound = true;
                            range.include_lower   = false;
                            range.lower_bound     = gt_cond->get_value();
                        }
                    }
                    break;
                }
                case condition_type::greater_equal_condition: {
                    auto ge_cond = std::static_pointer_cast<greater_equal_condition>(condition);
                    if (ge_cond->get_field() == field) {
                        if (!range.has_lower_bound ||
                            (range.has_lower_bound && !range.include_lower) ||
                            (range.has_lower_bound && range.include_lower &&
                             compare_values(ge_cond->get_value(), range.lower_bound) > 0)) {
                            range.has_lower_bound = true;
                            range.include_lower   = true;
                            range.lower_bound     = ge_cond->get_value();
                        }
                    }
                    break;
                }
                case condition_type::less_condition: {
                    auto lt_cond = std::static_pointer_cast<less_condition>(condition);
                    if (lt_cond->get_field() == field) {
                        if (!range.has_upper_bound ||
                            (range.has_upper_bound && range.include_upper &&
                             compare_values(lt_cond->get_value(), range.upper_bound) < 0)) {
                            range.has_upper_bound = true;
                            range.include_upper   = false;
                            range.upper_bound     = lt_cond->get_value();
                        }
                    }
                    break;
                }
                case condition_type::less_equal_condition: {
                    auto le_cond = std::static_pointer_cast<less_equal_condition>(condition);
                    if (le_cond->get_field() == field) {
                        if (!range.has_upper_bound ||
                            (range.has_upper_bound && !range.include_upper) ||
                            (range.has_upper_bound && range.include_upper &&
                             compare_values(le_cond->get_value(), range.upper_bound) < 0)) {
                            range.has_upper_bound = true;
                            range.include_upper   = true;
                            range.upper_bound     = le_cond->get_value();
                        }
                    }
                    break;
                }
                default:
                    break;
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
        }

        // 处理OR条件（适合IN优化的情况）
        if (where_condition->get_type() == condition_type::or_condition) {
            auto        or_cond    = std::static_pointer_cast<or_condition>(where_condition);
            const auto& conditions = or_cond->get_conditions();

            // 检查是否所有条件都是等值条件且针对同一字段
            bool                     all_equal_on_same_field = true;
            std::string              field_name;
            std::vector<mc::variant> values;

            if (!conditions.empty() &&
                conditions[0]->get_type() == condition_type::equal_condition) {
                auto first_cond = std::static_pointer_cast<equal_condition>(conditions[0]);
                field_name      = first_cond->get_field();
                values.push_back(first_cond->get_value());

                for (size_t i = 1; i < conditions.size(); ++i) {
                    if (conditions[i]->get_type() != condition_type::equal_condition) {
                        all_equal_on_same_field = false;
                        break;
                    }

                    auto equal_cond = std::static_pointer_cast<equal_condition>(conditions[i]);
                    if (equal_cond->get_field() != field_name) {
                        all_equal_on_same_field = false;
                        break;
                    }

                    values.push_back(equal_cond->get_value());
                }
            } else {
                all_equal_on_same_field = false;
            }

            if (all_equal_on_same_field && field_name == field) {
                // 创建IN查询计划
                query_plan plan;
                plan.plan_type = query_plan_type::index_exact_match;
                plan.index_id  = index_id;
                plan.fields.push_back(field);
                plan.values    = values;
                plan.use_index = true;

                return plan;
            }
        }

        // 如果没有找到合适的条件，返回全表扫描计划
        return create_full_scan_plan();
    }

    /**
     * 为复合键创建查询计划
     * @param index_id 索引ID
     * @param where_condition 条件对象
     * @return 查询计划
     */
    query_plan
    create_composite_key_plan(size_t                                 index_id,
                              const std::shared_ptr<base_condition>& where_condition) const {
        const auto* index = m_metadata.get_index_by_id(index_id);
        if (!index || index->field_names.size() <= 1) {
            return create_full_scan_plan();
        }

        // 创建查询计划
        query_plan plan;
        plan.plan_type = query_plan_type::composite_key;
        plan.index_id  = index_id;
        plan.use_index = true;

        // 提取每个字段的等值条件
        std::unordered_map<std::string, mc::variant> field_values;

        if (where_condition->get_type() == condition_type::and_condition) {
            auto and_cond = std::static_pointer_cast<and_condition>(where_condition);

            for (const auto& condition : and_cond->get_conditions()) {
                if (condition->get_type() == condition_type::equal_condition) {
                    auto eq_cond = std::static_pointer_cast<equal_condition>(condition);
                    field_values[eq_cond->get_field()] = eq_cond->get_value();
                }
            }
        } else if (where_condition->get_type() == condition_type::equal_condition) {
            auto eq_cond = std::static_pointer_cast<equal_condition>(where_condition);
            field_values[eq_cond->get_field()] = eq_cond->get_value();
        }

        // 检查是否有足够的条件覆盖复合键的前缀
        bool             has_all_leading_fields = true;
        mc::mutable_dict composite_key;

        for (size_t i = 0; i < index->field_names.size(); ++i) {
            const auto& field = index->field_names[i];

            // 添加字段到查询计划
            plan.fields.push_back(field);

            auto it = field_values.find(std::string(field));
            if (it != field_values.end()) {
                // 有这个字段的条件，添加到复合键
                composite_key[field] = it->second;
            } else {
                // 缺少字段条件，只能使用前面的字段
                has_all_leading_fields = false;
                break;
            }
        }

        if (composite_key.empty()) {
            // 没有匹配的字段条件，无法使用复合键索引
            return create_full_scan_plan();
        }

        // 如果有所有前导字段，设置键值以执行精确匹配
        if (has_all_leading_fields) {
            plan.key_value = composite_key;
            plan.plan_type = query_plan_type::index_exact_match;
        }

        return plan;
    }

    /**
     * 比较两个值
     * @param val1 第一个值
     * @param val2 第二个值
     * @return 比较结果：-1 表示 val1 < val2，0 表示相等，1 表示 val1 > val2
     */
    int compare_values(const mc::variant& val1, const mc::variant& val2) const {
        if (val1 == val2) {
            return 0;
        }

        // 数值比较
        if (val1.is_numeric() && val2.is_numeric()) {
            if (val1.is_integer() && val2.is_integer()) {
                return val1.as<int64_t>() < val2.as<int64_t>() ? -1 : 1;
            } else {
                double d1 =
                    val1.is_integer() ? static_cast<double>(val1.as<int64_t>()) : val1.as<double>();
                double d2 =
                    val2.is_integer() ? static_cast<double>(val2.as<int64_t>()) : val2.as<double>();
                return d1 < d2 ? -1 : 1;
            }
        }

        // 字符串比较
        if (val1.is_string() && val2.is_string()) {
            return val1.as_string() < val2.as_string() ? -1 : 1;
        }

        // 不同类型比较，按类型ID排序
        return val1.get_type() < val2.get_type() ? -1 : 1;
    }

    const table_index_metadata<ObjectType>& m_metadata;
};

} // namespace mc::database::query

#endif // MC_DATABASE_QUERY_PLANNER_H