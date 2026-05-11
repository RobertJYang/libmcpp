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

#ifndef MC_DATABASE_QUERY_BUILDER_H
#define MC_DATABASE_QUERY_BUILDER_H

#include <initializer_list>
#include <optional>
#include <type_traits>

#include <mc/db/query/condition.h>
#include <mc/variant.h>

namespace mc::db::query {

/**
 * 查询构建器，用于构建复杂查询条件
 */
class query_builder {
public:
    /**
     * 构造函数，创建一个空的查询构建器
     */
    query_builder() = default;

    /**
     * 从条件对象构造查询构建器
     *
     * @param cond 条件对象
     */
    MC_API query_builder(const condition& cond);

    /**
     * 从 dict AST 构造查询构建器
     */
    explicit MC_API query_builder(const mc::dict& spec);

    /**
     * 从 variant AST 构造查询构建器
     */
    explicit MC_API query_builder(const mc::variant& spec);

    /**
     * 添加一个等值条件
     * 如果已有条件则使用AND连接
     *
     * @param field 字段名
     * @param value 比较值
     * @return 查询构建器引用，用于链式调用
     */
    MC_API query_builder& where(mc::string_view field, const mc::variant& value);

    /**
     * 添加一个比较条件
     * 如果已有条件则使用AND连接
     *
     * @param field 字段名
     * @param op 比较操作符
     * @param value 比较值
     * @return 查询构建器引用，用于链式调用
     */
    MC_API query_builder& where(mc::string_view field, compare_op op, const mc::variant& value);

    /**
     * 添加一个OR条件
     *
     * @param field 字段名
     * @param value 比较值
     * @return 查询构建器引用，用于链式调用
     */
    MC_API query_builder& or_where(mc::string_view field, const mc::variant& value);

    /**
     * 添加一个OR条件
     *
     * @param field 字段名
     * @param op 比较操作符
     * @param value 比较值
     * @return 查询构建器引用，用于链式调用
     */
    MC_API query_builder& or_where(mc::string_view field, compare_op op, const mc::variant& value);

    /**
     * 添加一个条件
     *
     * @param cond 条件
     * @return 查询构建器引用，用于链式调用
     */
    MC_API query_builder& where(const condition& cond);

    /**
     * 添加一个IN条件（检查字段值是否在给定列表中）
     *
     * @param field 字段名
     * @param values 可能值的列表
     * @return 查询构建器引用，用于链式调用
     */
    template <typename T>
    query_builder& where_in(mc::string_view field, std::initializer_list<T> values)
    {
        auto variants = detail::to_variants(values);
        if (variants.empty()) {
            // 空值列表总是返回空结果
            // 添加一个永假条件
            condition false_cond(compare_op::eq, mc::string(field), mc::variant{});
            return add_condition(conditions::not_cond(false_cond));
        }

        condition in_cond = condition(compare_op::in, mc::string(field), mc::variant(std::move(variants)));
        return add_condition(in_cond);
    }

    template <typename Range, std::enable_if_t<detail::is_in_range<Range>::value, int> = 0>
    query_builder& where_in(mc::string_view field, const Range& values)
    {
        auto variants = detail::to_variants(values);
        if (variants.empty()) {
            condition false_cond(compare_op::eq, mc::string(field), mc::variant{});
            return add_condition(conditions::not_cond(false_cond));
        }

        condition in_cond = condition(compare_op::in, mc::string(field), mc::variant(std::move(variants)));
        return add_condition(in_cond);
    }

    /**
     * 添加一个BETWEEN条件（检查字段值是否在指定范围内）
     *
     * @param field 字段名
     * @param lower 下界值
     * @param upper 上界值
     * @return 查询构建器引用，用于链式调用
     */
    template <typename T>
    query_builder& where_between(mc::string_view field, const T& lower, const T& upper)
    {
        condition between_cond = conditions::between(mc::string(field), lower, upper);
        return add_condition(between_cond);
    }

    /**
     * 添加一个大于条件
     *
     * @param field 字段名
     * @param value 比较值
     * @return 查询构建器引用，用于链式调用
     */
    template <typename T>
    query_builder& where_gt(mc::string_view field, const T& value)
    {
        return where(field, compare_op::gt, mc::variant(value));
    }

    /**
     * 添加一个大于等于条件
     *
     * @param field 字段名
     * @param value 比较值
     * @return 查询构建器引用，用于链式调用
     */
    template <typename T>
    query_builder& where_ge(mc::string_view field, const T& value)
    {
        return where(field, compare_op::ge, mc::variant(value));
    }

    /**
     * 添加一个小于条件
     *
     * @param field 字段名
     * @param value 比较值
     * @return 查询构建器引用，用于链式调用
     */
    template <typename T>
    query_builder& where_lt(mc::string_view field, const T& value)
    {
        return where(field, compare_op::lt, mc::variant(value));
    }

    /**
     * 添加一个小于等于条件
     *
     * @param field 字段名
     * @param value 比较值
     * @return 查询构建器引用，用于链式调用
     */
    template <typename T>
    query_builder& where_le(mc::string_view field, const T& value)
    {
        return where(field, compare_op::le, mc::variant(value));
    }

    /**
     * 添加一个模糊匹配条件
     *
     * @param field 字段名
     * @param pattern 模式字符串 (包含%通配符)
     * @return 查询构建器引用，用于链式调用
     */
    MC_API query_builder& where_like(mc::string_view field, mc::string_view pattern);

    /**
     * 检查查询构建器是否为空（没有条件）
     * @return 是否为空
     */
    MC_API bool is_empty() const;

    /**
     * 检查是否有条件
     * @return 是否有条件
     */
    MC_API bool has_condition() const;

    /**
     * 获取查询条件
     * @return 条件对象
     */
    MC_API const condition& get_condition() const;

    /**
     * 检查对象是否满足条件
     *
     * @tparam T 对象类型
     * @param obj 被检查的对象
     * @return 是否满足
     */
    template <typename T>
    bool matches(const T& obj) const
    {
        if (!has_condition()) {
            return true;
        }
        return m_condition->matches(obj);
    }

    /**
     * 清空所有条件
     */
    MC_API void clear();

    /**
     * 获取条件的字符串表示（用于调试）
     */
    MC_API mc::string to_string() const;

    /**
     * 设置查询结果的最大数量
     * @param limit_value 最大数量
     * @return 查询构建器引用，用于链式调用
     */
    MC_API query_builder& limit(size_t limit_value);

    /**
     * 获取结果限制数量
     * @return 限制数量，如果未设置则为 0（表示无限制）
     */
    MC_API size_t get_limit() const;

    /**
     * 是否设置了结果限制
     * @return 是否设置限制
     */
    MC_API bool has_limit() const;

private:
    /**
     * 添加条件，与现有条件使用AND连接
     */
    MC_API query_builder& add_condition(const condition& cond);

    std::optional<condition> m_condition; // 查询条件
    size_t                   m_limit = 0; // 结果限制数量，0表示无限制
};

} // namespace mc::db::query

#endif // MC_DATABASE_QUERY_BUILDER_H