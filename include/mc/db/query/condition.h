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

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <mc/log.h>
#include <mc/reflect.h>
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
 * 统一条件类，用于表示查询条件
 */
class condition {
public:
    /**
     * 默认构造函数
     */
    condition() : m_op(compare_op::eq), m_is_logical(false) {
    }

    /**
     * 字段条件构造函数
     * @param op 比较操作符
     * @param field 字段名
     * @param value 比较值
     */
    condition(compare_op op, std::string field, const mc::variant& value)
        : m_op(op), m_field(std::move(field)), m_value(value), m_is_logical(false) {
    }

    /**
     * 逻辑条件构造函数
     * @param op 逻辑操作符
     * @param conditions 子条件
     */
    condition(logical_op op, std::vector<condition> conditions)
        : m_logical_op(op), m_conditions(std::move(conditions)), m_is_logical(true) {
    }

    /**
     * 判断对象是否匹配条件
     */
    template <typename T>
    bool matches(const T& obj) const {
        if (is_logical()) {
            return eval_logical(obj);
        } else {
            return eval_object(obj);
        }
    }

    /**
     * 获取比较操作符
     */
    compare_op get_op() const {
        return m_op;
    }

    /**
     * 获取字段名
     */
    std::string_view get_field() const {
        return m_field;
    }

    /**
     * 获取比较值
     */
    const mc::variant& get_value() const {
        return m_value;
    }

    /**
     * 是否是逻辑条件
     */
    bool is_logical() const {
        return m_is_logical;
    }

    /**
     * 获取逻辑操作符
     */
    logical_op get_logical_op() const {
        return m_logical_op;
    }

    /**
     * 获取子条件列表
     */
    const std::vector<condition>& get_conditions() const {
        return m_conditions;
    }

    /**
     * 生成描述字符串（用于调试）
     */
    std::string to_string() const {
        if (m_is_logical) {
            std::string result;
            switch (m_logical_op) {
            case logical_op::AND:
                result = "AND(";
                break;
            case logical_op::OR:
                result = "OR(";
                break;
            case logical_op::NOT:
                result = "NOT(";
                break;
            }

            for (size_t i = 0; i < m_conditions.size(); ++i) {
                if (i > 0) {
                    result += ", ";
                }
                result += m_conditions[i].to_string();
            }
            result += ")";
            return result;
        } else {
            std::string op_str;
            switch (m_op) {
            case compare_op::eq:
                op_str = "==";
                break;
            case compare_op::ne:
                op_str = "!=";
                break;
            case compare_op::gt:
                op_str = ">";
                break;
            case compare_op::ge:
                op_str = ">=";
                break;
            case compare_op::lt:
                op_str = "<";
                break;
            case compare_op::le:
                op_str = "<=";
                break;
            case compare_op::contains:
                op_str = "contains";
                break;
            case compare_op::like:
                op_str = "like";
                break;
            case compare_op::in:
                op_str = "in";
                break;
            case compare_op::between:
                op_str = "between";
                break;
            }

            std::string value_str;
            if (m_value.is_string()) {
                value_str = "\"" + m_value.as_string() + "\"";
            } else {
                // 简单的值转字符串
                value_str = "value";
            }

            return m_field + " " + op_str + " " + value_str;
        }
    }

private:
    /**
     * 评估逻辑条件
     * @tparam T 对象类型
     * @param obj 对象
     * @return 是否匹配
     */
    template <typename T>
    bool eval_logical(const T& obj) const {
        if (m_conditions.empty()) {
            return true; // 空条件集合默认匹配
        }

        switch (m_logical_op) {
        case logical_op::AND: {
            // 所有条件都必须匹配
            for (const auto& cond : m_conditions) {
                if (!cond.matches(obj)) {
                    return false;
                }
            }
            return true;
        }
        case logical_op::OR: {
            // 至少一个条件匹配
            for (const auto& cond : m_conditions) {
                if (cond.matches(obj)) {
                    return true;
                }
            }
            return false;
        }
        case logical_op::NOT: {
            // 条件取反（通常只有一个子条件）
            if (m_conditions.size() >= 1) {
                return !m_conditions[0].matches(obj);
            }
            return true; // 空NOT条件默认匹配
        }
        default:
            return false;
        }
    }

    /**
     * 评估对象
     */
    template <typename T>
    bool eval_object(const T& obj) const {
        auto property_info = mc::reflect::get_property_info<T>(m_field);
        if (property_info != nullptr) {
            auto field_value = property_info->get_value(obj);
            return compare_values(field_value, m_value, m_op);
        }

        auto method_info = mc::reflect::get_method_info<T>(m_field);
        if (method_info != nullptr) {
            try {
                auto field_value = method_info->invoke(const_cast<T&>(obj), {});
                return compare_values(field_value, m_value, m_op);
            } catch (const std::exception& e) {
                dlog("eval_object error: type: ${type}, field: ${field}, error: ${error}",
                     ("type", mc::pretty_name<T>())("field", m_field)("error", e.what()));
                return false;
            }
        }

        return false;
    }

    /**
     * 比较两个值
     */
    bool compare_values(const mc::variant& field_value, const mc::variant& value,
                        compare_op op) const {
        switch (op) {
        case compare_op::eq:
            return field_value == value;
        case compare_op::ne:
            return field_value != value;
        case compare_op::gt:
            return field_value > value;
        case compare_op::ge:
            return field_value >= value;
        case compare_op::lt:
            return field_value < value;
        case compare_op::le:
            return field_value <= value;
        case compare_op::contains:
            if (field_value.is_string() && value.is_string()) {
                return string_ops::contains(field_value.get_string(), value.get_string());
            }
            return false;
        case compare_op::like:
            if (field_value.is_string() && value.is_string()) {
                return string_ops::like(field_value.get_string(), value.get_string());
            }
            return false;
        case compare_op::in:
            if (value.is_array()) {
                for (const auto& item : value.get_array()) {
                    if (field_value == item) {
                        return true;
                    }
                }
            }
            return false;
        case compare_op::between:
            if (value.is_array() && value.size() >= 2) {
                const auto& arr = value.get_array();
                return field_value >= arr[0] && field_value <= arr[1];
            }
            return false;
        default:
            return false;
        }
    }

    compare_op             m_op         = compare_op::eq;  // 比较操作符
    logical_op             m_logical_op = logical_op::AND; // 逻辑操作符
    std::string            m_field;                        // 字段名
    mc::variant            m_value;                        // 比较值
    std::vector<condition> m_conditions;                   // 子条件（用于逻辑条件）
    bool                   m_is_logical = false;           // 是否是逻辑条件
};

// 条件工厂函数
namespace conditions {

/**
 * 创建等值条件
 */
inline condition eq(std::string field, mc::variant value) {
    return condition(compare_op::eq, std::move(field), std::move(value));
}

/**
 * 创建不等条件
 */
inline condition ne(std::string field, mc::variant value) {
    return condition(compare_op::ne, std::move(field), std::move(value));
}

/**
 * 创建大于条件
 */
inline condition gt(std::string field, mc::variant value) {
    return condition(compare_op::gt, std::move(field), std::move(value));
}

/**
 * 创建大于等于条件
 */
inline condition ge(std::string field, mc::variant value) {
    return condition(compare_op::ge, std::move(field), std::move(value));
}

/**
 * 创建小于条件
 */
inline condition lt(std::string field, mc::variant value) {
    return condition(compare_op::lt, std::move(field), std::move(value));
}

/**
 * 创建小于等于条件
 */
inline condition le(std::string field, mc::variant value) {
    return condition(compare_op::le, std::move(field), std::move(value));
}

/**
 * 创建LIKE条件
 */
inline condition like(std::string field, std::string pattern) {
    return condition(compare_op::like, std::move(field), mc::variant(std::move(pattern)));
}

/**
 * 创建IN条件
 */
template <typename T>
inline condition in(std::string field, std::vector<T> values) {
    mc::variants variants;
    for (const auto& value : values) {
        variants.push_back(mc::variant(value));
    }
    return condition(compare_op::in, std::move(field), mc::variant(variants));
}

/**
 * 创建BETWEEN条件
 */
template <typename T>
inline condition between(std::string field, T lower, T upper) {
    mc::variants range = {mc::variant(lower), mc::variant(upper)};
    return condition(compare_op::between, std::move(field), mc::variant(range));
}

/**
 * 创建AND条件
 */
inline condition and_cond(std::vector<condition> conditions) {
    return condition(logical_op::AND, std::move(conditions));
}

/**
 * 创建OR条件
 */
inline condition or_cond(std::vector<condition> conditions) {
    return condition(logical_op::OR, std::move(conditions));
}

/**
 * 创建NOT条件
 */
inline condition not_cond(condition cond) {
    return condition(logical_op::NOT, std::vector<condition>{std::move(cond)});
}

} // namespace conditions

} // namespace mc::db::query

#endif // MC_DATABASE_QUERY_CONDITION_H