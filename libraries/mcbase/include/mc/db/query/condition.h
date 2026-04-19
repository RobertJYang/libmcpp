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

#ifndef MC_DATABASE_QUERY_CONDITION_H
#define MC_DATABASE_QUERY_CONDITION_H

#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

#include <mc/array.h>
#include <mc/db/index_tag.h>
#include <mc/exception.h>
#include <mc/pretty_name.h>
#include <mc/reflect/method.h>
#include <mc/reflect/property.h>
#include <mc/variant.h>

namespace mc::db::query {

/**
 * 比较操作符枚举
 */
enum class compare_op {
    eq,       // 等于 (==)
    ne,       // 不等于 (!=)
    gt,       // 大于 (>)
    ge,       // 大于等于 (>=)
    lt,       // 小于 (<)
    le,       // 小于等于 (<=)
    contains, // 包含 (字符串或容器)
    like,     // 模糊匹配 (类SQL LIKE)
    in,       // 在集合中 (IN)
    between   // 在范围内 (BETWEEN)
};

/**
 * 逻辑操作符枚举
 */
enum class logical_op {
    AND, // 逻辑与
    OR,  // 逻辑或
    NOT  // 逻辑非
};

/**
 * 字符串操作辅助函数
 */
namespace string_ops {
/**
 * 检查字符串是否包含子串
 */
MC_API bool contains(mc::string_view str, mc::string_view sub);

/**
 * 简单的类SQL LIKE操作 (仅支持%通配符)
 */
MC_API bool like(mc::string_view str, mc::string_view pattern);
} // namespace string_ops

/**
 * 统一条件类，用于表示查询条件
 */
class condition {
public:
    /**
     * 默认构造函数
     */
    condition() : m_op(compare_op::eq), m_is_logical(false)
    {}

    /**
     * 字段条件构造函数
     * @param op 比较操作符
     * @param field 字段名
     * @param value 比较值
     */
    condition(compare_op op, mc::string field, const mc::variant& value)
        : m_op(op), m_field(std::move(field)), m_value(value), m_is_logical(false)
    {}

    /**
     * 逻辑条件构造函数
     * @param op 逻辑操作符
     * @param conditions 子条件
     */
    condition(logical_op op, std::vector<condition> conditions)
        : m_logical_op(op), m_conditions(std::move(conditions)), m_is_logical(true)
    {}

    /**
     * 判断对象是否匹配条件
     */
    template <typename T>
    bool matches(const T& obj) const
    {
        if (is_logical()) {
            return eval_logical(obj);
        } else {
            return eval_object(obj);
        }
    }

    /**
     * 获取比较操作符
     */
    compare_op get_op() const
    {
        return m_op;
    }

    /**
     * 获取字段名
     */
    mc::string_view get_field() const
    {
        return m_field;
    }

    /**
     * 获取比较值
     */
    const mc::variant& get_value() const
    {
        return m_value;
    }

    /**
     * 是否是逻辑条件
     */
    bool is_logical() const
    {
        return m_is_logical;
    }

    /**
     * 获取逻辑操作符
     */
    logical_op get_logical_op() const
    {
        return m_logical_op;
    }

    /**
     * 获取子条件列表
     */
    const std::vector<condition>& get_conditions() const
    {
        return m_conditions;
    }

    /**
     * 生成描述字符串（用于调试）
     */
    MC_API mc::string to_string() const;

private:
    using erased_match_fn = bool (*)(const condition& cond, const void* obj);

    template <typename T>
    static bool match_erased(const condition& cond, const void* obj)
    {
        return cond.matches(*static_cast<const T*>(obj));
    }

    /**
     * 评估逻辑条件
     * @tparam T 对象类型
     * @param obj 对象
     * @return 是否匹配
     */
    template <typename T>
    bool eval_logical(const T& obj) const
    {
        return eval_logical_erased(&obj, &condition::match_erased<T>);
    }

    /**
     * 评估对象
     */
    template <typename T>
    bool eval_object(const T& obj) const
    {
        return eval_object_erased(const_cast<T*>(&obj), mc::pretty_name<T>(),
                                  mc::reflect::get_property_info<T>(m_field), mc::reflect::get_method_info<T>(m_field));
    }

    /**
     * 比较两个值
     */
    MC_API bool eval_logical_erased(const void* obj, erased_match_fn match_fn) const;
    MC_API bool eval_object_erased(void* obj, mc::string_view type_name,
                                   const mc::reflect::property_type_info* property_info,
                                   const mc::reflect::method_type_info*   method_info) const;
    MC_API bool compare_values(const mc::variant& field_value, const mc::variant& value, compare_op op) const;

    compare_op             m_op         = compare_op::eq;  // 比较操作符
    logical_op             m_logical_op = logical_op::AND; // 逻辑操作符
    mc::string             m_field;                        // 字段名
    mc::variant            m_value;                        // 比较值
    std::vector<condition> m_conditions;                   // 子条件（用于逻辑条件）
    bool                   m_is_logical = false;           // 是否是逻辑条件
};

namespace detail {
template <typename T>
struct is_initializer_list : std::false_type {};

template <typename T>
struct is_initializer_list<std::initializer_list<T>> : std::true_type {};

template <typename T, typename = void>
struct has_begin_end : std::false_type {};

template <typename T>
struct has_begin_end<
    T, std::void_t<decltype(std::begin(std::declval<const T&>())), decltype(std::end(std::declval<const T&>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_size : std::false_type {};

template <typename T>
struct has_size<T, std::void_t<decltype(std::size(std::declval<const T&>()))>> : std::true_type {};

template <typename T, typename = void>
struct range_value_type {};

template <typename T>
struct range_value_type<T, std::enable_if_t<has_begin_end<T>::value>> {
    using type = std::decay_t<decltype(*std::begin(std::declval<const T&>()))>;
};

template <typename T, typename = void>
struct is_in_range : std::false_type {};

template <typename T>
struct is_in_range<T, std::enable_if_t<has_begin_end<T>::value && !is_initializer_list<std::decay_t<T>>::value &&
                                       !std::is_convertible_v<const std::decay_t<T>&, mc::string_view> &&
                                       (mc::is_variant_constructible_v<typename range_value_type<T>::type> ||
                                        mc::is_variant_v<typename range_value_type<T>::type> ||
                                        mc::is_variant_reference_v<typename range_value_type<T>::type>)>>
    : std::true_type {};

template <typename Range, std::enable_if_t<is_in_range<Range>::value, int> = 0>
mc::variants to_variants(const Range& values)
{
    mc::variants variants;
    if constexpr (has_size<Range>::value) {
        variants.reserve(std::size(values));
    }
    for (const auto& value : values) {
        variants.push_back(mc::variant(value));
    }
    return variants;
}

template <typename T>
mc::variants to_variants(std::initializer_list<T> values)
{
    mc::variants variants;
    variants.reserve(values.size());
    for (const auto& value : values) {
        variants.push_back(mc::variant(value));
    }
    return variants;
}

} // namespace detail

class field_expression {
public:
    explicit field_expression(mc::string field) : m_field(std::move(field))
    {}

    template <typename T>
    condition operator==(T&& value) const
    {
        return make_compare(compare_op::eq, std::forward<T>(value));
    }

    template <typename T>
    condition operator!=(T&& value) const
    {
        return make_compare(compare_op::ne, std::forward<T>(value));
    }

    template <typename T>
    condition operator>(T&& value) const
    {
        return make_compare(compare_op::gt, std::forward<T>(value));
    }

    template <typename T>
    condition operator>=(T&& value) const
    {
        return make_compare(compare_op::ge, std::forward<T>(value));
    }

    template <typename T>
    condition operator<(T&& value) const
    {
        return make_compare(compare_op::lt, std::forward<T>(value));
    }

    template <typename T>
    condition operator<=(T&& value) const
    {
        return make_compare(compare_op::le, std::forward<T>(value));
    }

    condition contains(mc::string_view sub) const
    {
        return condition(compare_op::contains, m_field, mc::variant(mc::string(sub)));
    }

    condition like(mc::string_view pattern) const
    {
        return condition(compare_op::like, m_field, mc::variant(mc::string(pattern)));
    }

    template <typename T>
    condition in(std::initializer_list<T> values) const
    {
        return condition(compare_op::in, m_field, mc::variant(detail::to_variants(values)));
    }

    template <typename Range, std::enable_if_t<detail::is_in_range<Range>::value, int> = 0>
    condition in(const Range& values) const
    {
        return condition(compare_op::in, m_field, mc::variant(detail::to_variants(values)));
    }

    template <typename T>
    condition between(const T& lower, const T& upper) const
    {
        mc::variants range;
        range.reserve(2);
        range.push_back(mc::variant(lower));
        range.push_back(mc::variant(upper));
        return condition(compare_op::between, m_field, mc::variant(std::move(range)));
    }

private:
    template <typename T>
    condition make_compare(compare_op op, T&& value) const
    {
        return condition(op, m_field, mc::variant(std::forward<T>(value)));
    }

    mc::string m_field;
};

inline field_expression field(mc::string_view field_name)
{
    return field_expression(mc::string(field_name));
}

} // namespace mc::db::query

namespace mc::db {

template <const char* FieldName>
inline const query::field_expression field_tag<FieldName>::field = query::field_expression(mc::string(FieldName));

} // namespace mc::db

// 条件工厂函数
namespace mc::db::query {

namespace conditions {

/**
 * 创建等值条件
 */
inline condition eq(mc::string field, mc::variant value)
{
    return condition(compare_op::eq, std::move(field), std::move(value));
}

/**
 * 创建不等条件
 */
inline condition ne(mc::string field, mc::variant value)
{
    return condition(compare_op::ne, std::move(field), std::move(value));
}

/**
 * 创建大于条件
 */
inline condition gt(mc::string field, mc::variant value)
{
    return condition(compare_op::gt, std::move(field), std::move(value));
}

/**
 * 创建大于等于条件
 */
inline condition ge(mc::string field, mc::variant value)
{
    return condition(compare_op::ge, std::move(field), std::move(value));
}

/**
 * 创建小于条件
 */
inline condition lt(mc::string field, mc::variant value)
{
    return condition(compare_op::lt, std::move(field), std::move(value));
}

/**
 * 创建小于等于条件
 */
inline condition le(mc::string field, mc::variant value)
{
    return condition(compare_op::le, std::move(field), std::move(value));
}

/**
 * 创建LIKE条件
 */
inline condition like(mc::string field, mc::string pattern)
{
    return condition(compare_op::like, std::move(field), mc::variant(std::move(pattern)));
}

/**
 * 创建IN条件
 */
template <typename T>
inline condition in(mc::string field, std::initializer_list<T> values)
{
    return condition(compare_op::in, std::move(field), mc::variant(detail::to_variants(values)));
}

template <typename Range, std::enable_if_t<detail::is_in_range<Range>::value, int> = 0>
inline condition in(mc::string field, const Range& values)
{
    return condition(compare_op::in, std::move(field), mc::variant(detail::to_variants(values)));
}

/**
 * 创建BETWEEN条件
 */
template <typename T>
inline condition between(mc::string field, T lower, T upper)
{
    mc::variants range;
    range.reserve(2);
    range.push_back(mc::variant(lower));
    range.push_back(mc::variant(upper));
    return condition(compare_op::between, std::move(field), mc::variant(std::move(range)));
}

/**
 * 创建AND条件
 */
inline condition and_cond(std::vector<condition> conditions)
{
    return condition(logical_op::AND, std::move(conditions));
}

/**
 * 创建OR条件
 */
inline condition or_cond(std::vector<condition> conditions)
{
    return condition(logical_op::OR, std::move(conditions));
}

/**
 * 创建NOT条件
 */
inline condition not_cond(condition cond)
{
    return condition(logical_op::NOT, std::vector<condition>{std::move(cond)});
}

} // namespace conditions

inline condition operator&&(condition lhs, condition rhs)
{
    return conditions::and_cond({std::move(lhs), std::move(rhs)});
}

inline condition operator||(condition lhs, condition rhs)
{
    return conditions::or_cond({std::move(lhs), std::move(rhs)});
}

inline condition operator!(condition cond)
{
    return conditions::not_cond(std::move(cond));
}

MC_API condition to_condition(const mc::dict& spec);
MC_API condition to_condition(const mc::variant& spec);

} // namespace mc::db::query

#endif // MC_DATABASE_QUERY_CONDITION_H