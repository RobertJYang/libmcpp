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
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <mc/database/query/builder.h>
#include <mc/database/query/condition.h>
#include <mc/database/query/metadata.h>
#include <mc/database/query/planner.h>
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

    /**
     * 获取表元数据
     *
     * @return 表元数据的共享指针
     */
    static const auto& get_metadata() {
        static const auto metadata = build_table_metadata<TableType>();
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
        // 创建一个带有限制的查询构建器
        query_builder limited_builder = builder;
        limited_builder.limit(limit);

        std::vector<object_type> result;

        // 执行查询，收集匹配对象
        query_impl(limited_builder, [&result](const object_type& obj) {
            result.push_back(obj);
            return true;
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
     * 查询记录
     * @param builder 查询构建器
     * @param limit 限制返回的记录数量，0表示不限制
     * @return 查询结果
     */
    std::vector<object_type> query(const query_builder& builder, size_t limit = 0) {
        std::vector<object_type> results;
        size_t                   count = 0;
        query(builder, [&](const object_type& obj) -> bool {
            if (limit > 0 && count >= limit) {
                return false;
            }
            results.push_back(obj);
            count++;
            return true;
        });
        return results;
    }

    /**
     * 查询记录
     * @param builder 查询构建器
     * @param handler 处理函数，返回false表示停止查询
     * @return 是否查询完成
     */
    template <typename Handler,
              typename = std::enable_if_t<std::is_invocable_r_v<bool, Handler, const object_type&>>>
    bool query(const query_builder& builder, Handler&& handler) {
        query_impl(builder, std::forward<Handler>(handler));
        return true;
    }

private:
    /**
     * 检查字段是否为非唯一字段
     */
    bool is_field_non_unique(std::string_view field_name) const {
        const auto& metadata = get_metadata();
        return metadata.has_non_unique_index(field_name);
    }

    /**
     * 利用元数据查找索引
     * @param field 字段名
     * @return 索引ID，如果没有匹配的索引则返回-1
     */
    int find_index_by_metadata(std::string_view field) const {
        const auto& metadata = get_metadata();
        return metadata.find_best_index_id(field);
    }

    /**
     * 查找可用的索引
     *
     * @param field 字段名
     * @return 索引ID，如果没有匹配的索引则返回-1
     */
    int find_index(std::string_view field) const {
        const auto& metadata = get_metadata();
        return metadata.find_best_index_id(field);
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

        // 如果查询计划包含特定值
        if (!plan.key_value.is_null()) {
            return query_by_single_value(builder, index_id, plan.fields, plan.key_value,
                                         std::forward<Handler>(handler));
        }
        // 处理IN查询（多个值）
        else if (!plan.values.empty()) {
            return query_by_multiple_values(builder, index_id, plan.fields, plan.values,
                                            std::forward<Handler>(handler));
        }

        // 没有具体的值，回退到全表扫描
        return false;
    }

    /**
     * 使用单个值查询
     * @param builder 查询构建器
     * @param index_id 索引ID
     * @param fields 字段列表
     * @param value 查询值
     * @param handler 结果处理器
     * @return 是否成功
     */
    template <typename Handler>
    bool query_by_single_value(const query_builder& builder, size_t index_id,
                               const std::vector<std::string_view>& fields,
                               const mc::variant& value, Handler&& handler) {
        std::string_view field_name          = fields.empty() ? std::string_view() : fields[0];
        bool             is_non_unique_field = is_field_non_unique(field_name);

        if (is_non_unique_field) {
            return query_by_non_unique_index(builder, index_id, value,
                                             std::forward<Handler>(handler));
        } else {
            return query_by_unique_index(builder, index_id, value, std::forward<Handler>(handler));
        }
    }

    /**
     * 使用非唯一索引查询
     * @param builder 查询构建器
     * @param index_id 索引ID
     * @param value 查询值
     * @param found 是否找到的引用，用于 IN 查询
     * @param handler 结果处理器
     * @return 是否中断查询/是否成功
     */
    template <typename Handler>
    bool query_by_non_unique_index(const query_builder& builder, size_t index_id,
                                   const mc::variant& value, bool& found, Handler&& handler) {
        // 对于非唯一字段，使用索引范围查询
        auto begin_it = m_table.lower_bound_by_index(index_id, value);
        auto end_it   = m_table.upper_bound_by_index(index_id, value);

        size_t count = 0;
        size_t limit = builder.has_limit() ? builder.get_limit() : 0;

        typename TableType::lock_guard lock(m_table);
        for (auto it = begin_it; it != end_it && !it.is_end(); ++it) {
            const auto& obj = *it->second;
            // 检查对象是否满足所有条件
            if (builder.has_condition() && !builder.matches(obj)) {
                continue;
            }

            found = true;
            if (!handler(obj)) {
                return true; // 中断查询/查询成功
            }

            // 检查是否达到限制
            if (limit > 0) {
                count++;
                if (count >= limit) {
                    return true;
                }
            }
        }
        return false; // 继续查询/没有找到匹配项
    }

    /**
     * 使用非唯一索引查询（单值查询简化接口）
     */
    template <typename Handler>
    bool query_by_non_unique_index(const query_builder& builder, size_t index_id,
                                   const mc::variant& value, Handler&& handler) {
        bool found = false;
        query_by_non_unique_index(builder, index_id, value, found, std::forward<Handler>(handler));
        return found;
    }

    /**
     * 使用唯一索引查询
     * @param builder 查询构建器
     * @param index_id 索引ID
     * @param value 查询值
     * @param found 是否找到的引用，用于 IN 查询
     * @param handler 结果处理器
     * @return 是否中断查询
     */
    template <typename Handler>
    bool query_by_unique_index(const query_builder& builder, size_t index_id,
                               const mc::variant& value, bool& found, Handler&& handler) {
        // 执行唯一索引查找
        auto result = m_table.find_by_index(index_id, value);
        if (result) {
            // 检查对象是否满足所有条件
            if (builder.has_condition() && !builder.matches(*result)) {
                return false;
            }

            found = true;
            if (!handler(*result)) {
                return true; // 中断查询
            }
        }
        return false; // 继续查询
    }

    /**
     * 使用唯一索引查询（单值查询简化接口）
     */
    template <typename Handler>
    bool query_by_unique_index(const query_builder& builder, size_t index_id,
                               const mc::variant& value, Handler&& handler) {
        bool found = false;
        query_by_unique_index(builder, index_id, value, found, std::forward<Handler>(handler));
        return true; // 对于唯一索引，始终返回true表示查询成功完成
    }

    /**
     * 使用多个值查询(IN查询)
     * @param builder 查询构建器
     * @param index_id 索引ID
     * @param fields 字段列表
     * @param values 查询值列表
     * @param handler 结果处理器
     * @return 是否成功
     */
    template <typename Handler>
    bool query_by_multiple_values(const query_builder& builder, size_t index_id,
                                  const std::vector<std::string_view>& fields,
                                  const mc::variants& values, Handler&& handler) {
        std::string_view field_name          = fields.empty() ? std::string_view() : fields[0];
        bool             is_non_unique_field = is_field_non_unique(field_name);

        bool found = false;
        for (const auto& value : values) {
            if (is_non_unique_field) {
                if (query_by_non_unique_index(builder, index_id, value, found,
                                              std::forward<Handler>(handler))) {
                    return true;
                }
            } else {
                if (query_by_unique_index(builder, index_id, value, found,
                                          std::forward<Handler>(handler))) {
                    return true;
                }
            }
        }
        return found;
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

        // 使用计划中的范围
        query_range range = plan.range;

        // 获取开始和结束迭代器
        auto begin_it = get_range_begin_iterator(index_id, range);
        auto end_it   = get_range_end_iterator(index_id, range);

        // 遍历范围内的所有对象
        return iterate_range(builder, begin_it, end_it, std::forward<Handler>(handler));
    }

    /**
     * 获取范围查询的开始迭代器
     * @param index_id 索引ID
     * @param range 查询范围
     * @return 开始迭代器
     */
    raw_iterator get_range_begin_iterator(size_t index_id, const query_range& range) {
        if (range.has_lower_bound) {
            if (range.include_lower) {
                return m_table.lower_bound_by_index(index_id, range.lower_bound);
            } else {
                return m_table.upper_bound_by_index(index_id, range.lower_bound);
            }
        } else {
            return m_table.begin();
        }
    }

    /**
     * 获取范围查询的结束迭代器
     * @param index_id 索引ID
     * @param range 查询范围
     * @return 结束迭代器
     */
    raw_iterator get_range_end_iterator(size_t index_id, const query_range& range) {
        if (range.has_upper_bound) {
            if (range.include_upper) {
                return m_table.upper_bound_by_index(index_id, range.upper_bound);
            } else {
                return m_table.lower_bound_by_index(index_id, range.upper_bound);
            }
        } else {
            return m_table.end();
        }
    }

    /**
     * 迭代指定范围的对象
     * @param builder 查询构建器
     * @param begin_it 开始迭代器
     * @param end_it 结束迭代器
     * @param handler 结果处理器
     * @return 是否找到匹配对象
     */
    template <typename Handler>
    bool iterate_range(const query_builder& builder, raw_iterator begin_it, raw_iterator end_it,
                       Handler&& handler) {
        bool   found = false;
        size_t count = 0;
        size_t limit = builder.has_limit() ? builder.get_limit() : 0;

        typename TableType::lock_guard lock(m_table);
        for (auto it = begin_it; it != end_it && !it.is_end(); ++it) {
            const auto& obj = *it->second;
            if (!builder.matches(obj)) {
                continue;
            }

            found = true;
            if (!handler(obj)) {
                break;
            }

            // 检查是否达到限制
            if (limit > 0) {
                count++;
                if (count >= limit) {
                    break;
                }
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
     * 查询实现
     * @param builder 查询构建器
     * @param handler 结果处理函数
     */
    template <typename Handler>
    void query_impl(const query_builder& builder, Handler&& handler) {
        // 无条件时，执行全表扫描
        if (!builder.has_condition()) {
            full_table_scan(std::forward<Handler>(handler));
            return;
        }

        // 生成查询计划
        const query_plan plan = generate_query_plan(builder);

        // 执行查询
        execute_query_plan(builder, plan, std::forward<Handler>(handler));
    }

    /**
     * 全表扫描
     * @param handler
     */
    template <typename Handler>
    void full_table_scan(Handler&& handler) {
        typename TableType::lock_guard lock(m_table);
        for (auto it = m_table.begin(); !it.is_end(); ++it) {
            const auto& obj = *it->second;
            if (!handler(obj)) {
                break;
            }
        }
    }

    /**
     * 带过滤的全表扫描
     * @param builder
     * @param handler
     */
    template <typename Handler>
    void full_table_scan_with_filter(const query_builder& builder, Handler&& handler) {
        size_t count = 0;
        size_t limit = builder.has_limit() ? builder.get_limit() : 0;

        typename TableType::lock_guard lock(m_table);
        for (auto it = m_table.begin(); !it.is_end(); ++it) {
            const auto& obj = *it->second;

            if (!builder.matches(obj)) {
                continue;
            }

            if (!handler(obj)) {
                break;
            }

            if (limit == 0) {
                continue;
            }

            if (count++ >= limit) {
                break;
            }
        }
    }

    /**
     * 生成查询计划
     * @param builder 查询构建器
     * @return 查询计划
     */
    query_plan generate_query_plan(const query_builder& builder) {
        const auto& cond     = builder.get_condition();
        const auto& metadata = get_metadata();
        auto        planner  = query_planner<object_type>(metadata);
        return planner.generate_plan(cond);
    }

    /**
     * 根据查询计划执行查询
     * @param builder 查询构建器
     * @param plan 查询计划
     * @param handler 结果处理函数
     */
    template <typename Handler>
    void execute_query_plan(const query_builder& builder, const query_plan& plan,
                            Handler&& handler) {
        // 根据查询计划执行查询
        bool use_index = plan.plan_type != query_plan_type::full_scan &&
                         plan.plan_type != query_plan_type::empty_result;

        if (use_index) {
            query_by_index(builder, plan, std::forward<Handler>(handler));
        } else if (plan.plan_type == query_plan_type::empty_result) {
            // 空结果，不执行任何操作
            return;
        } else {
            // 全表扫描
            full_table_scan_with_filter(builder, std::forward<Handler>(handler));
        }
    }

    TableType& m_table;
};

} // namespace mc::database::query

#endif // MC_DATABASE_QUERY_TABLE_QUERY_H