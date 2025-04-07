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
 * @file table_query.h
 * @brief 提供通用的表查询功能，支持索引优化
 */
#ifndef MC_DATABASE_QUERY_TABLE_QUERY_H
#define MC_DATABASE_QUERY_TABLE_QUERY_H

#include <functional>
#include <iostream>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <mc/database/query/builder.h>
#include <mc/database/query/condition.h>
#include <mc/database/query/field_accessor.h>
#include <mc/database/query/metadata.h>
#include <mc/variant.h>

namespace mc::database::query {

/**
 * 表查询助手类
 *
 * 提供通用的表查询功能，支持索引优化
 *
 * @tparam TableType 表类型
 */
template <typename TableType>
class table_query {
public:
    using object_type     = typename TableType::object_type;
    using object_ptr_type = typename TableType::object_ptr_type;
    using raw_iterator    = typename TableType::raw_iterator;
    using object_id_type  = typename object_type::object_id_type;

    /**
     * 构造函数
     *
     * @param table 表引用
     */
    explicit table_query(TableType& table) : m_table(table) {
    }

    static const auto& get_metadata() {
        static const auto& metadata = build_table_metadata<TableType>();
        return metadata;
    }

    /**
     * 查询对象并收集所有匹配的结果
     *
     * @param builder 查询构建器
     * @return 匹配对象列表
     */
    std::vector<object_type> query_all(const query_builder& builder) {
        std::vector<object_type> result;

        // 执行查询，收集所有匹配对象
        query_impl(builder, [&result](const object_type& obj) {
            result.push_back(obj);
            return true;
        });

        return result;
    }

    /**
     * 查询对象，限制返回数量
     *
     * @param builder 查询构建器
     * @param limit 最大返回数量
     * @return 匹配对象列表
     */
    std::vector<object_type> query_limit(const query_builder& builder, size_t limit) {
        std::vector<object_type> result;

        // 执行查询，收集指定数量的匹配对象
        query_impl(builder, [&result, limit](const object_type& obj) {
            result.push_back(obj);
            return result.size() < limit;
        });

        return result;
    }

    /**
     * 查询单个对象
     *
     * @param builder 查询构建器
     * @return 匹配的第一个对象的可选包装
     */
    std::optional<object_type> query_one(const query_builder& builder) {
        auto results = query_limit(builder, 1);
        if (results.empty()) {
            return std::nullopt;
        }
        return results[0];
    }

    /**
     * 查询对象并使用自定义处理函数处理结果
     *
     * @param builder 查询构建器
     * @param handler 结果处理函数，返回false表示停止查询
     */
    template <typename Handler>
    void query(const query_builder& builder, Handler&& handler) {
        query_impl(builder, std::forward<Handler>(handler));
    }

private:
    /**
     * 利用元数据查找索引
     * @param field 字段名
     * @return 索引ID，如果没有匹配的索引则返回-1
     */
    int find_index_by_metadata(std::string_view field) const {
        const auto& metadata = get_metadata();
        auto        indices  = metadata.find_indices_by_field(field);
        if (indices.empty()) {
            return -1;
        }

        // 如果有多个索引，优先选择唯一索引
        for (const auto* index : indices) {
            if (index->is_unique && index->index_id > 0) {
                return static_cast<int>(index->index_id);
            }
        }

        // 如果没有唯一索引，则选择第一个ID大于0的索引
        for (const auto* index : indices) {
            if (index->index_id > 0) {
                return static_cast<int>(index->index_id);
            }
        }

        return -1;
    }

    /**
     * 查找可用的索引
     *
     * @param field 字段名
     * @return 索引ID，如果没有匹配的索引则返回-1
     */
    int find_index(std::string_view field) const {
        int metadata_index = find_index_by_metadata(field);
        if (metadata_index >= 0) {
            return metadata_index;
        }

        return -1;
    }

    /**
     * 通过索引执行查询
     * @param builder 查询构建器
     * @param plan 查询计划
     * @param handler 结果处理器
     * @return 是否成功
     */
    template <typename Handler>
    bool query_by_index(const query_builder& builder, const query_plan& plan, Handler&& handler) {
        switch (plan.plan_type) {
        case query_plan_type::index_exact_match:
            return query_by_exact_match(builder, plan, std::forward<Handler>(handler));
        case query_plan_type::index_range:
            return query_by_range(builder, plan, std::forward<Handler>(handler));
        case query_plan_type::composite_key:
            return query_by_composite_key(builder, plan, std::forward<Handler>(handler));
        case query_plan_type::empty_result:
            // 直接返回空结果，无需处理
            return true;
        default:
            return false;
        }
    }

    /**
     * 通过精确匹配索引执行查询
     * @param builder 查询构建器
     * @param plan 查询计划
     * @param handler 结果处理器
     * @return 是否成功
     */
    template <typename Handler>
    bool query_by_exact_match(const query_builder& builder, const query_plan& plan,
                              Handler&& handler) {
        // 直接使用索引ID
        size_t index_id = plan.index_id;

        // 使用新条件系统
        if (builder.has_where()) {
            if (builder.get_where()->get_type() == condition_type::equal_condition) {
                // 处理单一等值条件
                auto eq_cond = std::static_pointer_cast<equal_condition>(builder.get_where());

                // 执行索引查找
                auto result = m_table.find_by_index(index_id, eq_cond->get_value());
                if (result) {
                    if (!builder.matches(*result)) {
                        return true;
                    }

                    handler(*result);
                }

                return true;
            } else if (builder.get_where()->get_type() == condition_type::and_condition) {
                // 处理AND条件
                auto and_cond = std::static_pointer_cast<and_condition>(builder.get_where());
                for (const auto& condition : and_cond->get_conditions()) {
                    if (condition->get_type() == condition_type::equal_condition) {
                        auto eq_cond = std::static_pointer_cast<equal_condition>(condition);

                        // 检查字段是否匹配索引
                        const auto& metadata = get_metadata();
                        auto        indices  = metadata.find_indices_by_field(eq_cond->get_field());
                        bool        field_matches_index = false;
                        for (const auto* idx : indices) {
                            if (idx->index_id == index_id) {
                                field_matches_index = true;
                                break;
                            }
                        }

                        if (field_matches_index) {
                            // 执行索引查找
                            auto result = m_table.find_by_index(index_id, eq_cond->get_value());
                            if (result) {
                                if (!builder.matches(*result)) {
                                    continue;
                                }

                                if (!handler(*result)) {
                                    break;
                                }
                            }
                            return true;
                        }
                    }
                }
            } else if (builder.get_where()->get_type() == condition_type::or_condition) {
                // 处理OR条件（IN查询）
                auto        or_cond = std::static_pointer_cast<or_condition>(builder.get_where());
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

                if (all_equal_on_same_field) {
                    // 检查字段是否匹配索引
                    const auto& metadata            = get_metadata();
                    auto        indices             = metadata.find_indices_by_field(field_name);
                    bool        field_matches_index = false;
                    for (const auto* idx : indices) {
                        if (idx->index_id == index_id) {
                            field_matches_index = true;
                            break;
                        }
                    }

                    if (field_matches_index) {
                        // 使用IN优化
                        for (const auto& value : values) {
                            auto result = m_table.find_by_index(index_id, value);
                            if (result) {
                                if (!builder.matches(*result)) {
                                    continue;
                                }

                                if (!handler(*result)) {
                                    break;
                                }
                            }
                        }
                        return true;
                    }
                }
            }
        }

        // 如果不符合优化条件，回退到全表扫描
        return false;
    }

    /**
     * 通过范围查询索引执行查询
     * @param builder 查询构建器
     * @param plan 查询计划
     * @param handler 结果处理器
     * @return 是否成功
     */
    template <typename Handler>
    bool query_by_range(const query_builder& builder, const query_plan& plan, Handler&& handler) {
        // 直接使用索引ID
        size_t index_id = plan.index_id;

        // 构建范围查询
        query_range range;

        // 处理条件系统
        if (builder.has_where()) {
            switch (builder.get_where()->get_type()) {
            case condition_type::greater_condition: {
                auto gt_cond = std::static_pointer_cast<greater_condition>(builder.get_where());
                range.has_lower_bound = true;
                range.include_lower   = false;
                range.lower_bound     = gt_cond->get_value();
                break;
            }
            case condition_type::greater_equal_condition: {
                auto ge_cond =
                    std::static_pointer_cast<greater_equal_condition>(builder.get_where());
                range.has_lower_bound = true;
                range.include_lower   = true;
                range.lower_bound     = ge_cond->get_value();
                break;
            }
            case condition_type::less_condition: {
                auto lt_cond = std::static_pointer_cast<less_condition>(builder.get_where());
                range.has_upper_bound = true;
                range.include_upper   = false;
                range.upper_bound     = lt_cond->get_value();
                break;
            }
            case condition_type::less_equal_condition: {
                auto le_cond = std::static_pointer_cast<less_equal_condition>(builder.get_where());
                range.has_upper_bound = true;
                range.include_upper   = true;
                range.upper_bound     = le_cond->get_value();
                break;
            }
            case condition_type::and_condition: {
                // 处理AND条件中的范围条件
                auto        and_cond = std::static_pointer_cast<and_condition>(builder.get_where());
                const auto& conditions = and_cond->get_conditions();

                for (const auto& condition : conditions) {
                    switch (condition->get_type()) {
                    case condition_type::greater_condition: {
                        auto gt_cond = std::static_pointer_cast<greater_condition>(condition);
                        range.has_lower_bound = true;
                        range.include_lower   = false;
                        range.lower_bound     = gt_cond->get_value();
                        break;
                    }
                    case condition_type::greater_equal_condition: {
                        auto ge_cond = std::static_pointer_cast<greater_equal_condition>(condition);
                        range.has_lower_bound = true;
                        range.include_lower   = true;
                        range.lower_bound     = ge_cond->get_value();
                        break;
                    }
                    case condition_type::less_condition: {
                        auto lt_cond          = std::static_pointer_cast<less_condition>(condition);
                        range.has_upper_bound = true;
                        range.include_upper   = false;
                        range.upper_bound     = lt_cond->get_value();
                        break;
                    }
                    case condition_type::less_equal_condition: {
                        auto le_cond = std::static_pointer_cast<less_equal_condition>(condition);
                        range.has_upper_bound = true;
                        range.include_upper   = true;
                        range.upper_bound     = le_cond->get_value();
                        break;
                    }
                    default:
                        break;
                    }
                }
                break;
            }
            default:
                break;
            }
        }

        // 执行范围查询
        auto begin_it =
            range.has_lower_bound
                ? (range.include_lower ? m_table.lower_bound_by_index(index_id, range.lower_bound)
                                       : m_table.upper_bound_by_index(index_id, range.lower_bound))
                : m_table.begin();

        auto end_it =
            range.has_upper_bound
                ? (range.include_upper ? m_table.upper_bound_by_index(index_id, range.upper_bound)
                                       : m_table.lower_bound_by_index(index_id, range.upper_bound))
                : m_table.end();

        // 遍历范围内的所有对象
        bool found = false;
        for (auto it = begin_it; it != end_it && !it.is_end(); ++it) {
            const auto& obj = *it->second;
            if (!builder.matches(obj)) {
                continue;
            }

            found = true;
            if (!handler(obj)) {
                break;
            }
        }

        return found;
    }

    /**
     * 通过复合键索引执行查询
     * @param builder 查询构建器
     * @param plan 查询计划
     * @param handler 结果处理器
     * @return 是否成功
     */
    template <typename Handler>
    bool query_by_composite_key(const query_builder& builder, const query_plan& plan,
                                Handler&& handler) {
        // 复合键查询实现
        // 此处需要结合具体索引类型进行实现
        return false; // 暂不支持
    }

    /**
     * 执行全表扫描
     * @param builder 查询构建器
     * @param handler 结果处理器
     */
    template <typename Handler>
    void full_table_scan(const query_builder& builder, Handler&& handler) {
        // 调试信息已移除，但保持功能完整性

        for (auto it = m_table.begin(); !it.is_end(); ++it) {
            const auto& obj     = *it->second;
            bool        matches = builder.matches(obj);

            if (!matches) {
                continue;
            }

            if (!handler(obj)) {
                break;
            }
        }
    }

    /**
     * 内部查询实现
     *
     * @param builder 查询构建器
     * @param handler, 结果处理函数
     */
    template <typename Handler>
    void query_impl(const query_builder& builder, Handler&& handler) {
        if (builder.is_empty()) {
            return;
        }

        // 创建查询规划器并生成查询计划
        const auto& metadata = get_metadata();
        auto        planner  = query_planner<object_type>(metadata);
        auto        plan     = planner.plan_for_query(builder);

        // 根据查询计划执行不同的查询策略
        if (plan.use_index) {
            switch (plan.plan_type) {
            case query_plan_type::index_exact_match:
                if (query_by_exact_match(builder, plan, std::forward<Handler>(handler))) {
                    return;
                }
                break;

            case query_plan_type::index_range:
                if (query_by_range(builder, plan, std::forward<Handler>(handler))) {
                    return;
                }
                break;

            case query_plan_type::composite_key:
                if (query_by_composite_key(builder, plan, std::forward<Handler>(handler))) {
                    return;
                }
                break;

            case query_plan_type::empty_result:
                return; // 空结果优化，直接返回

            default:
                break;
            }
        }

        // 如果无法使用索引或索引查询失败，则回退到全表扫描
        full_table_scan(builder, std::forward<Handler>(handler));
    }

    TableType& m_table;
};

/**
 * 创建表查询助手的工厂函数
 *
 * @param table 表对象
 * @return 表查询助手对象
 */
template <typename TableType>
auto make_table_query(TableType& table) {
    return table_query<TableType>(table);
}

} // namespace mc::database::query

#endif // MC_DATABASE_QUERY_TABLE_QUERY_H