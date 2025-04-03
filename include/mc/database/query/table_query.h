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
     * 尝试使用索引查询
     *
     * @param field 字段名
     * @param op 比较操作符
     * @param value 比较值
     * @param handler 结果处理函数
     * @return 是否成功使用索引
     */
    template <typename Handler>
    bool query_by_index(const query_builder& builder, query_plan& plan, Handler&& handler) {
        if (plan.fields.empty() || plan.fields[0].empty()) {
            return false;
        }

        int index_id = find_index(plan.fields[0]);
        if (index_id < 0) {
            return false;
        }

        auto result = m_table.find_by_index(index_id, plan.key_value);
        if (result == nullptr ||
            !builder.eval<const object_type&>(*result, mc::reflect::get_property<object_type>)) {
            return false;
        }

        handler(*result);
        return true;
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
                if (query_by_index(builder, plan, std::forward<Handler>(handler))) {
                    return;
                }
                break;

            case query_plan_type::index_range:
                if (execute_range_query(builder, plan, std::forward<Handler>(handler))) {
                    return;
                }
                break;

            case query_plan_type::composite_key:
                if (execute_composite_key_query(plan, std::forward<Handler>(handler))) {
                    return;
                }
                break;

            case query_plan_type::empty_result:
                return;

            default:
                break;
            }
        }

        // 如果无法使用索引或索引查询失败，则回退到全表扫描
        full_table_scan(builder, handler);
    }

    /**
     * 执行范围查询
     *
     * @param plan 查询计划
     * @param handler 结果处理函数
     * @return 是否成功执行
     */
    template <typename Handler>
    bool execute_range_query(const query_builder& builder, const query_plan& plan,
                             Handler&& handler) {
        if (plan.fields.empty() || plan.fields[0].empty()) {
            return false;
        }

        // 根据字段名找到索引ID
        int index_id = find_index(plan.fields[0]);
        if (index_id < 0) {
            return false;
        }

        return range_query_by_index(index_id, builder, plan.range, std::forward<Handler>(handler));
    }

    /**
     * 通过索引执行范围查询
     *
     * @tparam IndexId 索引ID
     * @param range 范围条件
     * @param handler 结果处理函数
     * @return 是否成功执行
     */
    template <typename Handler>
    bool range_query_by_index(size_t index_id, const query_builder& builder,
                              const query_range& range, Handler&& handler) {
        if (range.has_lower_bound && range.has_upper_bound) {
            return lower_to_upper(index_id, builder, range, std::forward<Handler>(handler));
        } else if (range.has_lower_bound) {
            return lower_to_end(index_id, builder, range, std::forward<Handler>(handler));
        } else if (range.has_upper_bound) {
            return begin_to_upper(index_id, builder, range, std::forward<Handler>(handler));
        } else {
            return false; // 没有边界，相当于全表扫描
        }
    }

    template <typename Handler>
    bool lower_to_end(size_t index_id, const query_builder& builder, const query_range& range,
                      Handler&& handler) {
        raw_iterator it;
        if (range.include_lower) {
            it = m_table.lower_bound_by_index(index_id, range.lower_bound);
        } else {
            it = m_table.upper_bound_by_index(index_id, range.lower_bound);
        }

        bool found = false;
        for (; !it.is_end(); ++it) {
            if (!builder.eval<const object_type>(*it->second,
                                                 mc::reflect::get_property<object_type>)) {
                continue;
            }

            found = true;
            if (!handler(*it->second)) {
                break;
            }
        }
        return found;
    }

    template <typename Handler>
    bool lower_to_upper(size_t index_id, const query_builder& builder, const query_range& range,
                        Handler&& handler) {
        raw_iterator low_it;
        raw_iterator upper_it;
        if (range.include_lower && range.include_upper) {
            // 闭区间 [lower, upper]
            low_it   = m_table.lower_bound_by_index(index_id, range.lower_bound);
            upper_it = m_table.upper_bound_by_index(index_id, range.upper_bound);
        } else if (range.include_lower) {
            // 左闭右开 [lower, upper)
            low_it   = m_table.lower_bound_by_index(index_id, range.lower_bound);
            upper_it = m_table.lower_bound_by_index(index_id, range.upper_bound);
        } else if (range.include_upper) {
            // 左开右闭 (lower, upper]
            low_it   = m_table.lower_bound_by_index(index_id, range.lower_bound);
            upper_it = m_table.upper_bound_by_index(index_id, range.upper_bound);
        } else {
            // 左开右开 (lower, upper)
            low_it   = m_table.lower_bound_by_index(index_id, range.lower_bound);
            upper_it = m_table.lower_bound_by_index(index_id, range.upper_bound);
        }

        return handle_range(low_it, upper_it, builder, std::forward<Handler>(handler));
    }

    template <typename Handler>
    bool begin_to_upper(size_t index_id, const query_builder& builder, const query_range& range,
                        Handler&& handler) {
        raw_iterator low_it = m_table.begin();
        raw_iterator upper_it;
        if (range.include_upper) {
            // 右闭 [begin, upper]
            upper_it = m_table.upper_bound_by_index(index_id, range.upper_bound);
        } else {
            // 右开 [begin, upper)
            upper_it = m_table.lower_bound_by_index(index_id, range.upper_bound);
        }

        return handle_range(low_it, upper_it, builder, std::forward<Handler>(handler));
    }

    template <typename Handler>
    bool handle_range(raw_iterator l_it, raw_iterator r_it, const query_builder& builder,
                      Handler&& handler) {
        bool found = false;
        while (!l_it.is_end()) {
            if (!builder.eval<const object_type>(*l_it->second,
                                                 mc::reflect::get_property<object_type>)) {
                if (found || l_it == r_it) {
                    break;
                }

                // 左开区间需要右移到第一个位置
                ++l_it;
                continue;
            }

            found = true;
            if (!handler(*l_it->second)) {
                break;
            }

            if (l_it == r_it) {
                break;
            }

            ++l_it;
        }
        return found;
    }

    /**
     * 执行复合键查询
     *
     * @param plan 查询计划
     * @param handler 结果处理函数
     * @return 是否成功执行
     */
    template <typename Handler>
    bool execute_composite_key_query(const query_plan& plan, Handler&& handler) {
        // 复合键查询实现
        // 此处需要结合具体索引类型进行实现
        // 简单起见，目前返回false，回退到全表扫描
        return false;
    }

    /**
     * 全表扫描实现
     *
     * @param builder 查询构建器
     * @param handler 结果处理函数
     */
    template <typename Handler>
    void full_table_scan(const query_builder& builder, Handler handler) {
        auto end = m_table.end();
        for (auto it = m_table.begin(); it != end; ++it) {
            const auto& obj = *it->second;
            if (!builder.eval<const object_type&>(obj, mc::reflect::get_property<object_type>)) {
                continue;
            }

            if (!handler(obj)) {
                break;
            }
        }
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