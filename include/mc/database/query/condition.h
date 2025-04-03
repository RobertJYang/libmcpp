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

#include <string>
#include <string_view>
#include <vector>

#include <mc/dict.h>
#include <mc/variant.h>

namespace mc::database::query {

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
inline bool contains(std::string_view str, std::string_view sub) {
    return str.find(sub) != std::string_view::npos;
}

/**
 * 简单的类SQL LIKE操作 (仅支持%通配符)
 */
inline bool like(std::string_view str, std::string_view pattern) {
    // 简单实现，仅支持前缀匹配、后缀匹配和包含匹配
    if (pattern.empty()) {
        return str.empty();
    }

    // 前缀匹配: "abc%"
    if (pattern.back() == '%' && pattern.find('%') == pattern.size() - 1) {
        return str.substr(0, pattern.size() - 1) == pattern.substr(0, pattern.size() - 1);
    }

    // 后缀匹配: "%abc"
    if (pattern.front() == '%' && pattern.find('%', 1) == std::string_view::npos) {
        return str.size() >= pattern.size() - 1 &&
               str.substr(str.size() - (pattern.size() - 1)) == pattern.substr(1);
    }

    // 包含匹配: "%abc%"
    if (pattern.front() == '%' && pattern.back() == '%' &&
        pattern.find('%', 1) == pattern.size() - 1) {
        auto sub = pattern.substr(1, pattern.size() - 2);
        return contains(str, sub);
    }

    // 精确匹配
    return str == pattern;
}
} // namespace string_ops

/**
 * 查询条件，表示一个字段的比较操作
 */
class condition {
public:
    /**
     * 创建一个简单条件: field op value
     */
    static condition make(std::string_view field, compare_op op, const mc::variant& value) {
        condition cond;
        cond.m_field = field;
        cond.m_op    = op;
        cond.m_value = value;
        return cond;
    }

    /**
     * 创建一个等于条件: field == value
     */
    static condition eq(std::string_view field, const mc::variant& value) {
        return make(field, compare_op::eq, value);
    }

    /**
     * 创建一个不等于条件: field != value
     */
    static condition ne(std::string_view field, const mc::variant& value) {
        return make(field, compare_op::ne, value);
    }

    /**
     * 创建一个大于条件: field > value
     */
    static condition gt(std::string_view field, const mc::variant& value) {
        return make(field, compare_op::gt, value);
    }

    /**
     * 创建一个大于等于条件: field >= value
     */
    static condition ge(std::string_view field, const mc::variant& value) {
        return make(field, compare_op::ge, value);
    }

    /**
     * 创建一个小于条件: field < value
     */
    static condition lt(std::string_view field, const mc::variant& value) {
        return make(field, compare_op::lt, value);
    }

    /**
     * 创建一个小于等于条件: field <= value
     */
    static condition le(std::string_view field, const mc::variant& value) {
        return make(field, compare_op::le, value);
    }

    /**
     * 创建一个包含条件: field contains value
     */
    static condition contains(std::string_view field, const mc::variant& value) {
        return make(field, compare_op::contains, value);
    }

    /**
     * 创建一个模糊匹配条件: field LIKE pattern
     */
    static condition like(std::string_view field, const std::string& pattern) {
        return make(field, compare_op::like, pattern);
    }

    /**
     * 创建一个IN条件: field IN (values...)
     */
    static condition in(std::string_view field, const std::vector<mc::variant>& values) {
        return make(field, compare_op::in, mc::variant(values));
    }

    /**
     * 创建一个范围条件: field BETWEEN min AND max
     */
    static condition between(std::string_view field, const mc::variant& min_val,
                             const mc::variant& max_val) {
        mc::mutable_dict range;
        range["min"] = min_val;
        range["max"] = max_val;
        return make(field, compare_op::between, range);
    }

    /**
     * 获取字段名
     */
    std::string_view get_field() const {
        return m_field;
    }

    /**
     * 获取比较操作符
     */
    compare_op get_op() const {
        return m_op;
    }

    /**
     * 获取比较值
     */
    const mc::variant& get_value() const {
        return m_value;
    }

    /**
     * 评估条件是否匹配值
     */
    bool eval(const mc::variant& field_value) const {
        switch (m_op) {
        case compare_op::eq:
            return field_value == m_value;

        case compare_op::ne:
            return field_value != m_value;

        case compare_op::gt:
            // 根据类型进行比较
            if (field_value.is_numeric() && m_value.is_numeric()) {
                if (field_value.is_integer() && m_value.is_integer()) {
                    return field_value.as<int64_t>() > m_value.as<int64_t>();
                } else if (field_value.is_double() && m_value.is_double()) {
                    return field_value.as<double>() > m_value.as<double>();
                } else if (field_value.is_integer() && m_value.is_double()) {
                    return static_cast<double>(field_value.as<int64_t>()) > m_value.as<double>();
                } else if (field_value.is_double() && m_value.is_integer()) {
                    return field_value.as<double>() > static_cast<double>(m_value.as<int64_t>());
                }
            } else if (field_value.is_string() && m_value.is_string()) {
                return field_value.as_string() > m_value.as_string();
            }
            return false;

        case compare_op::ge:
            // 根据类型进行比较
            if (field_value.is_numeric() && m_value.is_numeric()) {
                if (field_value.is_integer() && m_value.is_integer()) {
                    return field_value.as<int64_t>() >= m_value.as<int64_t>();
                } else if (field_value.is_double() && m_value.is_double()) {
                    return field_value.as<double>() >= m_value.as<double>();
                } else if (field_value.is_integer() && m_value.is_double()) {
                    return static_cast<double>(field_value.as<int64_t>()) >= m_value.as<double>();
                } else if (field_value.is_double() && m_value.is_integer()) {
                    return field_value.as<double>() >= static_cast<double>(m_value.as<int64_t>());
                }
            } else if (field_value.is_string() && m_value.is_string()) {
                return field_value.as_string() >= m_value.as_string();
            }
            return field_value == m_value;

        case compare_op::lt:
            // 根据类型进行比较
            if (field_value.is_numeric() && m_value.is_numeric()) {
                if (field_value.is_integer() && m_value.is_integer()) {
                    return field_value.as<int64_t>() < m_value.as<int64_t>();
                } else if (field_value.is_double() && m_value.is_double()) {
                    return field_value.as<double>() < m_value.as<double>();
                } else if (field_value.is_integer() && m_value.is_double()) {
                    return static_cast<double>(field_value.as<int64_t>()) < m_value.as<double>();
                } else if (field_value.is_double() && m_value.is_integer()) {
                    return field_value.as<double>() < static_cast<double>(m_value.as<int64_t>());
                }
            } else if (field_value.is_string() && m_value.is_string()) {
                return field_value.as_string() < m_value.as_string();
            }
            return false;

        case compare_op::le:
            // 根据类型进行比较
            if (field_value.is_numeric() && m_value.is_numeric()) {
                if (field_value.is_integer() && m_value.is_integer()) {
                    return field_value.as<int64_t>() <= m_value.as<int64_t>();
                } else if (field_value.is_double() && m_value.is_double()) {
                    return field_value.as<double>() <= m_value.as<double>();
                } else if (field_value.is_integer() && m_value.is_double()) {
                    return static_cast<double>(field_value.as<int64_t>()) <= m_value.as<double>();
                } else if (field_value.is_double() && m_value.is_integer()) {
                    return field_value.as<double>() <= static_cast<double>(m_value.as<int64_t>());
                }
            } else if (field_value.is_string() && m_value.is_string()) {
                return field_value.as_string() <= m_value.as_string();
            }
            return field_value == m_value;

        case compare_op::contains:
            // 仅支持字符串包含
            if (field_value.is_string() && m_value.is_string()) {
                return string_ops::contains(field_value.as_string(), m_value.as_string());
            }
            return false;

        case compare_op::like:
            // 模糊匹配，仅支持字符串
            if (field_value.is_string() && m_value.is_string()) {
                return string_ops::like(field_value.as_string(), m_value.as_string());
            }
            return false;

        case compare_op::in:
            // IN操作，检查值是否在列表中
            if (m_value.is_array()) {
                const auto& array = m_value.as_array();
                for (const auto& item : array) {
                    if (field_value == item) {
                        return true;
                    }
                }
            }
            return false;

        case compare_op::between:
            // BETWEEN操作，检查值是否在范围内
            if (m_value.is_dict()) {
                const auto& range = m_value.as_dict();
                if (range.contains("min") && range.contains("max")) {
                    const auto& min_val = range.at("min");
                    const auto& max_val = range.at("max");

                    // 数值比较
                    if (field_value.is_numeric() && min_val.is_numeric() && max_val.is_numeric()) {
                        if (field_value.is_integer() && min_val.is_integer() &&
                            max_val.is_integer()) {
                            auto val = field_value.as<int64_t>();
                            return val >= min_val.as<int64_t>() && val <= max_val.as<int64_t>();
                        } else {
                            double val = field_value.is_integer()
                                             ? static_cast<double>(field_value.as<int64_t>())
                                             : field_value.as<double>();
                            double min = min_val.is_integer()
                                             ? static_cast<double>(min_val.as<int64_t>())
                                             : min_val.as<double>();
                            double max = max_val.is_integer()
                                             ? static_cast<double>(max_val.as<int64_t>())
                                             : max_val.as<double>();
                            return val >= min && val <= max;
                        }
                    }

                    // 字符串比较
                    if (field_value.is_string() && min_val.is_string() && max_val.is_string()) {
                        auto str = field_value.as_string();
                        return str >= min_val.as_string() && str <= max_val.as_string();
                    }
                }
            }
            return false;
        }

        return false;
    }

    /**
     * 条件是否可以用索引优化
     */
    bool can_use_index() const {
        // 等于条件和IN条件通常可以直接利用索引
        if (m_op == compare_op::eq || m_op == compare_op::in) {
            return true;
        }

        // 范围条件通常可以部分利用索引
        if (m_op == compare_op::gt || m_op == compare_op::ge || m_op == compare_op::lt ||
            m_op == compare_op::le || m_op == compare_op::between) {
            return true;
        }

        // 其他操作通常不能直接利用索引
        return false;
    }

private:
    condition() = default;

    std::string_view m_field;               // 字段名
    compare_op       m_op = compare_op::eq; // 比较操作符
    mc::variant      m_value;               // 比较值
};

/**
 * 字段构建器，用于方便地创建字段条件
 */
class field_builder {
public:
    explicit field_builder(std::string_view field_name) : m_field(field_name) {
    }

    // 等于条件
    condition eq(const mc::variant& value) const {
        return condition::eq(m_field, value);
    }

    // 不等于条件
    condition ne(const mc::variant& value) const {
        return condition::ne(m_field, value);
    }

    // 大于条件
    condition gt(const mc::variant& value) const {
        return condition::gt(m_field, value);
    }

    // 大于等于条件
    condition ge(const mc::variant& value) const {
        return condition::ge(m_field, value);
    }

    // 小于条件
    condition lt(const mc::variant& value) const {
        return condition::lt(m_field, value);
    }

    // 小于等于条件
    condition le(const mc::variant& value) const {
        return condition::le(m_field, value);
    }

    // 包含条件
    condition contains(const mc::variant& value) const {
        return condition::contains(m_field, value);
    }

    // 模糊匹配条件
    condition like(const std::string& pattern) const {
        return condition::like(m_field, pattern);
    }

    // IN条件
    condition in(const std::vector<mc::variant>& values) const {
        return condition::in(m_field, values);
    }

    // BETWEEN条件
    condition between(const mc::variant& min_val, const mc::variant& max_val) const {
        return condition::between(m_field, min_val, max_val);
    }

    // 链式操作，用于OR条件
    field_builder or_field(std::string_view field_name) const {
        return field_builder(field_name);
    }

private:
    std::string_view m_field;
};

/**
 * 创建字段构建器的辅助函数
 */
inline field_builder field(std::string_view field_name) {
    return field_builder(field_name);
}

} // namespace mc::database::query

#endif // MC_DATABASE_QUERY_CONDITION_H