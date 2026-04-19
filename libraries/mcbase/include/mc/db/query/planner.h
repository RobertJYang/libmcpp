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

#include <memory>
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
    query_plan_type         plan_type = query_plan_type::full_scan; // 计划类型
    bool                    use_index = false;                      // 是否使用索引
    size_t                  index_id  = 0;                          // 索引ID
    mc::variant             key_value;                              // 用于精确匹配的键值
    mc::variants            values;                                 // 用于IN查询的多个值
    query_range             range;                                  // 用于范围查询的范围
    std::vector<mc::string> fields;                                 // 涉及的字段
};

/**
 * 查询计划运行时实现，负责生成优化的查询执行计划
 */
class query_planner_runtime {
public:
    /**
     * 构造函数 - 接受元数据引用
     *
     * @param metadata 表元数据
     */
    explicit MC_API query_planner_runtime(const table_index_metadata_base& metadata);

    /**
     * 为查询生成执行计划
     *
     * @param builder 查询构建器
     * @return 查询计划
     */
    MC_API query_plan plan_for_query(const query_builder& builder) const;

    /**
     * 为条件生成执行计划
     *
     * @param cond 查询条件
     * @return 查询计划
     */
    MC_API query_plan generate_plan(const condition& cond) const;

private:
    /**
     * 为条件找到合适的索引
     *
     * @param condition 条件
     * @return 查询计划
     */
    MC_API query_plan find_matching_index(const condition& condition) const;

    /**
     * 为字段条件创建查询计划
     *
     * @param cond 字段条件
     * @return 查询计划
     */
    MC_API query_plan plan_for_field_condition(const condition& cond) const;

    /**
     * 为AND条件寻找最佳索引
     *
     * @param conditions 条件列表
     * @return 查询计划
     */
    MC_API query_plan find_best_index_for_and(const std::vector<condition>& conditions) const;

    /**
     * 为OR条件创建查询计划
     *
     * @param conditions 条件列表
     * @return 查询计划
     */
    MC_API query_plan plan_for_or_condition(const std::vector<condition>& conditions) const;

    /**
     * 查找OR条件中的公共字段
     *
     * @param conditions 条件列表
     * @param out_field 输出参数，找到的公共字段
     * @return 是否找到公共字段
     */
    MC_API bool find_common_field_for_or(const std::vector<condition>& conditions, mc::string& out_field) const;

    /**
     * 判断计划A是否优于计划B
     *
     * @param a 计划A
     * @param b 计划B
     * @return 是否A优于B
     */
    MC_API bool is_better_plan(const query_plan& a, const query_plan& b) const;

    const table_index_metadata_base& m_metadata;
};

/**
 * 查询计划生成器，负责生成优化的查询执行计划
 *
 * @tparam ObjectType 对象类型
 */
template <typename ObjectType>
class query_planner : public query_planner_runtime {
public:
    explicit query_planner(const table_index_metadata<ObjectType>& metadata) : query_planner_runtime(metadata)
    {}

    explicit query_planner(const std::shared_ptr<table_index_metadata<ObjectType>>& reflection_ptr)
        : query_planner_runtime(*reflection_ptr)
    {}
};

} // namespace mc::db::query

#endif // MC_DATABASE_QUERY_PLANNER_H