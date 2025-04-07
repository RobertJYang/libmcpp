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
#include <string>
#include <string_view>
#include <vector>

#include <mc/dict.h>
#include <mc/reflect.h>
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
 * 条件类型枚举
 */
enum class condition_type {
    equal_condition,         // 等值条件
    range_condition,         // 范围条件
    and_condition,           // AND条件
    or_condition,            // OR条件
    not_condition,           // NOT条件
    greater_condition,       // 大于条件
    greater_equal_condition, // 大于等于条件
    less_condition,          // 小于条件
    less_equal_condition     // 小于等于条件
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
 * 条件基类，表示一个查询条件
 */
class base_condition {
public:
    virtual ~base_condition() = default;

    /**
     * 获取条件类型
     */
    virtual condition_type get_type() const = 0;

    /**
     * 评估对象是否匹配条件
     */
    template <typename T>
    bool matches(const T& obj) const {
        return eval_impl(obj);
    }

protected:
    /**
     * 评估对象是否匹配条件的实现
     */
    virtual bool eval_impl(const mc::variant& obj) const = 0;

    /**
     * 对象特化版本的评估
     */
    template <typename T>
    bool eval_impl(const T& obj) const {
        // 使用反射获取对象字段值
        // 默认实现，子类可以重写此方法
        return false;
    }
};

/**
 * 等值条件，表示 field == value
 */
class equal_condition : public base_condition {
public:
    equal_condition(std::string field, mc::variant value)
        : m_field(std::move(field)), m_value(std::move(value)) {
    }

    condition_type get_type() const override {
        return condition_type::equal_condition;
    }

    const std::string& get_field() const {
        return m_field;
    }

    const mc::variant& get_value() const {
        return m_value;
    }

protected:
    bool eval_impl(const mc::variant& obj) const override {
        if (obj.is_dict()) {
            const auto& dict = obj.as<mc::dict>();
            std::string key(m_field);
            if (dict.contains(key)) {
                return dict[key] == m_value;
            }
        }
        return false;
    }

private:
    // 对不同类型进行特化的eval_impl方法
    template <typename Dict, typename std::enable_if_t<std::is_same_v<Dict, mc::dict> ||
                                                           std::is_same_v<Dict, mc::mutable_dict>,
                                                       int> = 0>
    bool eval_impl(const Dict& obj) const {
        std::string key(m_field);
        if (obj.contains(key)) {
            return obj[key] == m_value;
        }
        return false;
    }

    // 通用对象处理方法
    template <typename T, typename std::enable_if_t<!std::is_same_v<T, mc::dict> &&
                                                        !std::is_same_v<T, mc::mutable_dict> &&
                                                        !std::is_same_v<T, mc::variant>,
                                                    int> = 0>
    bool eval_impl(const T& obj) const {
        // 使用反射获取对象字段值
        auto field_value =
            mc::reflect::get_property<std::remove_cv_t<std::remove_reference_t<T>>>(obj, m_field);
        return field_value == m_value;
    }

    std::string m_field;
    mc::variant m_value;
};

/**
 * AND条件，表示所有子条件都必须满足
 */
class and_condition : public base_condition {
public:
    condition_type get_type() const override {
        return condition_type::and_condition;
    }

    void add_condition(std::shared_ptr<base_condition> condition) {
        m_conditions.push_back(std::move(condition));
    }

    const std::vector<std::shared_ptr<base_condition>>& get_conditions() const {
        return m_conditions;
    }

protected:
    bool eval_impl(const mc::variant& obj) const override {
        for (const auto& condition : m_conditions) {
            if (!condition->matches(obj)) {
                return false;
            }
        }
        return true;
    }

private:
    // 字典特化
    template <typename Dict, typename std::enable_if_t<std::is_same_v<Dict, mc::dict> ||
                                                           std::is_same_v<Dict, mc::mutable_dict>,
                                                       int> = 0>
    bool eval_impl(const Dict& obj) const {
        for (const auto& condition : m_conditions) {
            if (!condition->matches(obj)) {
                return false;
            }
        }
        return true;
    }

    std::vector<std::shared_ptr<base_condition>> m_conditions;
};

/**
 * OR条件，表示至少一个子条件必须满足
 */
class or_condition : public base_condition {
public:
    condition_type get_type() const override {
        return condition_type::or_condition;
    }

    void add_condition(std::shared_ptr<base_condition> condition) {
        m_conditions.push_back(std::move(condition));
    }

    const std::vector<std::shared_ptr<base_condition>>& get_conditions() const {
        return m_conditions;
    }

protected:
    bool eval_impl(const mc::variant& obj) const override {
        if (m_conditions.empty()) {
            return true;
        }

        for (const auto& condition : m_conditions) {
            if (condition->matches(obj)) {
                return true;
            }
        }
        return false;
    }

private:
    // 字典特化
    template <typename Dict, typename std::enable_if_t<std::is_same_v<Dict, mc::dict> ||
                                                           std::is_same_v<Dict, mc::mutable_dict>,
                                                       int> = 0>
    bool eval_impl(const Dict& obj) const {
        if (m_conditions.empty()) {
            return true;
        }

        for (const auto& condition : m_conditions) {
            if (condition->matches(obj)) {
                return true;
            }
        }
        return false;
    }

    std::vector<std::shared_ptr<base_condition>> m_conditions;
};

/**
 * 大于条件，表示 field > value
 */
class greater_condition : public base_condition {
public:
    greater_condition(std::string field, mc::variant value)
        : m_field(std::move(field)), m_value(std::move(value)) {
    }

    condition_type get_type() const override {
        return condition_type::greater_condition;
    }

    const std::string& get_field() const {
        return m_field;
    }

    const mc::variant& get_value() const {
        return m_value;
    }

protected:
    bool eval_impl(const mc::variant& obj) const override {
        if (obj.is_dict()) {
            const auto& dict = obj.as<mc::dict>();
            std::string key(m_field);
            if (dict.contains(key)) {
                auto field_value = dict[key];
                // 根据类型进行比较
                if (field_value.is_numeric() && m_value.is_numeric()) {
                    if (field_value.is_integer() && m_value.is_integer()) {
                        return field_value.template as<int64_t>() > m_value.template as<int64_t>();
                    } else if (field_value.is_double() && m_value.is_double()) {
                        return field_value.template as<double>() > m_value.template as<double>();
                    } else if (field_value.is_integer() && m_value.is_double()) {
                        return static_cast<double>(field_value.template as<int64_t>()) >
                               m_value.template as<double>();
                    } else if (field_value.is_double() && m_value.is_integer()) {
                        return field_value.template as<double>() >
                               static_cast<double>(m_value.template as<int64_t>());
                    }
                } else if (field_value.is_string() && m_value.is_string()) {
                    return field_value.as_string() > m_value.as_string();
                }
            }
        }
        return false;
    }

private:
    // 对不同类型进行特化的eval_impl方法
    template <typename Dict, typename std::enable_if_t<std::is_same_v<Dict, mc::dict> ||
                                                           std::is_same_v<Dict, mc::mutable_dict>,
                                                       int> = 0>
    bool eval_impl(const Dict& obj) const {
        std::string key(m_field);
        if (obj.contains(key)) {
            auto field_value = obj[key];
            // 根据类型进行比较
            if (field_value.is_numeric() && m_value.is_numeric()) {
                if (field_value.is_integer() && m_value.is_integer()) {
                    return field_value.template as<int64_t>() > m_value.template as<int64_t>();
                } else if (field_value.is_double() && m_value.is_double()) {
                    return field_value.template as<double>() > m_value.template as<double>();
                } else if (field_value.is_integer() && m_value.is_double()) {
                    return static_cast<double>(field_value.template as<int64_t>()) >
                           m_value.template as<double>();
                } else if (field_value.is_double() && m_value.is_integer()) {
                    return field_value.template as<double>() >
                           static_cast<double>(m_value.template as<int64_t>());
                }
            } else if (field_value.is_string() && m_value.is_string()) {
                return field_value.as_string() > m_value.as_string();
            }
        }
        return false;
    }

    // 通用对象处理方法
    template <typename T, typename std::enable_if_t<!std::is_same_v<T, mc::dict> &&
                                                        !std::is_same_v<T, mc::mutable_dict> &&
                                                        !std::is_same_v<T, mc::variant>,
                                                    int> = 0>
    bool eval_impl(const T& obj) const {
        // 使用反射获取对象字段值
        auto field_value =
            mc::reflect::get_property<std::remove_cv_t<std::remove_reference_t<T>>>(obj, m_field);

        // 根据类型进行比较
        if (field_value.is_numeric() && m_value.is_numeric()) {
            if (field_value.is_integer() && m_value.is_integer()) {
                return field_value.template as<int64_t>() > m_value.template as<int64_t>();
            } else if (field_value.is_double() && m_value.is_double()) {
                return field_value.template as<double>() > m_value.template as<double>();
            } else if (field_value.is_integer() && m_value.is_double()) {
                return static_cast<double>(field_value.template as<int64_t>()) >
                       m_value.template as<double>();
            } else if (field_value.is_double() && m_value.is_integer()) {
                return field_value.template as<double>() >
                       static_cast<double>(m_value.template as<int64_t>());
            }
        } else if (field_value.is_string() && m_value.is_string()) {
            return field_value.as_string() > m_value.as_string();
        }
        return false;
    }

    std::string m_field;
    mc::variant m_value;
};

/**
 * 大于等于条件，表示 field >= value
 */
class greater_equal_condition : public base_condition {
public:
    greater_equal_condition(std::string field, mc::variant value)
        : m_field(std::move(field)), m_value(std::move(value)) {
    }

    condition_type get_type() const override {
        return condition_type::greater_equal_condition;
    }

    const std::string& get_field() const {
        return m_field;
    }

    const mc::variant& get_value() const {
        return m_value;
    }

protected:
    bool eval_impl(const mc::variant& obj) const override {
        if (obj.is_dict()) {
            const auto& dict = obj.as<mc::dict>();
            std::string key(m_field);
            if (dict.contains(key)) {
                auto field_value = dict[key];
                // 根据类型进行比较
                if (field_value.is_numeric() && m_value.is_numeric()) {
                    if (field_value.is_integer() && m_value.is_integer()) {
                        return field_value.template as<int64_t>() >= m_value.template as<int64_t>();
                    } else if (field_value.is_double() && m_value.is_double()) {
                        return field_value.template as<double>() >= m_value.template as<double>();
                    } else if (field_value.is_integer() && m_value.is_double()) {
                        return static_cast<double>(field_value.template as<int64_t>()) >=
                               m_value.template as<double>();
                    } else if (field_value.is_double() && m_value.is_integer()) {
                        return field_value.template as<double>() >=
                               static_cast<double>(m_value.template as<int64_t>());
                    }
                } else if (field_value.is_string() && m_value.is_string()) {
                    return field_value.as_string() >= m_value.as_string();
                }
            }
        }
        return false;
    }

private:
    // 对不同类型进行特化的eval_impl方法
    template <typename Dict, typename std::enable_if_t<std::is_same_v<Dict, mc::dict> ||
                                                           std::is_same_v<Dict, mc::mutable_dict>,
                                                       int> = 0>
    bool eval_impl(const Dict& obj) const {
        std::string key(m_field);
        if (obj.contains(key)) {
            auto field_value = obj[key];
            // 根据类型进行比较
            if (field_value.is_numeric() && m_value.is_numeric()) {
                if (field_value.is_integer() && m_value.is_integer()) {
                    return field_value.template as<int64_t>() >= m_value.template as<int64_t>();
                } else if (field_value.is_double() && m_value.is_double()) {
                    return field_value.template as<double>() >= m_value.template as<double>();
                } else if (field_value.is_integer() && m_value.is_double()) {
                    return static_cast<double>(field_value.template as<int64_t>()) >=
                           m_value.template as<double>();
                } else if (field_value.is_double() && m_value.is_integer()) {
                    return field_value.template as<double>() >=
                           static_cast<double>(m_value.template as<int64_t>());
                }
            } else if (field_value.is_string() && m_value.is_string()) {
                return field_value.as_string() >= m_value.as_string();
            }
        }
        return false;
    }

    // 通用对象处理方法
    template <typename T, typename std::enable_if_t<!std::is_same_v<T, mc::dict> &&
                                                        !std::is_same_v<T, mc::mutable_dict> &&
                                                        !std::is_same_v<T, mc::variant>,
                                                    int> = 0>
    bool eval_impl(const T& obj) const {
        // 使用反射获取对象字段值
        auto field_value =
            mc::reflect::get_property<std::remove_cv_t<std::remove_reference_t<T>>>(obj, m_field);

        // 根据类型进行比较
        if (field_value.is_numeric() && m_value.is_numeric()) {
            if (field_value.is_integer() && m_value.is_integer()) {
                return field_value.template as<int64_t>() >= m_value.template as<int64_t>();
            } else if (field_value.is_double() && m_value.is_double()) {
                return field_value.template as<double>() >= m_value.template as<double>();
            } else if (field_value.is_integer() && m_value.is_double()) {
                return static_cast<double>(field_value.template as<int64_t>()) >=
                       m_value.template as<double>();
            } else if (field_value.is_double() && m_value.is_integer()) {
                return field_value.template as<double>() >=
                       static_cast<double>(m_value.template as<int64_t>());
            }
        } else if (field_value.is_string() && m_value.is_string()) {
            return field_value.as_string() >= m_value.as_string();
        }
        return false;
    }

    std::string m_field;
    mc::variant m_value;
};

/**
 * 小于条件，表示 field < value
 */
class less_condition : public base_condition {
public:
    less_condition(std::string field, mc::variant value)
        : m_field(std::move(field)), m_value(std::move(value)) {
    }

    condition_type get_type() const override {
        return condition_type::less_condition;
    }

    const std::string& get_field() const {
        return m_field;
    }

    const mc::variant& get_value() const {
        return m_value;
    }

protected:
    bool eval_impl(const mc::variant& obj) const override {
        if (obj.is_dict()) {
            const auto& dict = obj.as<mc::dict>();
            std::string key(m_field);
            if (dict.contains(key)) {
                auto field_value = dict[key];
                // 根据类型进行比较
                if (field_value.is_numeric() && m_value.is_numeric()) {
                    if (field_value.is_integer() && m_value.is_integer()) {
                        return field_value.template as<int64_t>() < m_value.template as<int64_t>();
                    } else if (field_value.is_double() && m_value.is_double()) {
                        return field_value.template as<double>() < m_value.template as<double>();
                    } else if (field_value.is_integer() && m_value.is_double()) {
                        return static_cast<double>(field_value.template as<int64_t>()) <
                               m_value.template as<double>();
                    } else if (field_value.is_double() && m_value.is_integer()) {
                        return field_value.template as<double>() <
                               static_cast<double>(m_value.template as<int64_t>());
                    }
                } else if (field_value.is_string() && m_value.is_string()) {
                    return field_value.as_string() < m_value.as_string();
                }
            }
        }
        return false;
    }

private:
    // 对不同类型进行特化的eval_impl方法
    template <typename Dict, typename std::enable_if_t<std::is_same_v<Dict, mc::dict> ||
                                                           std::is_same_v<Dict, mc::mutable_dict>,
                                                       int> = 0>
    bool eval_impl(const Dict& obj) const {
        std::string key(m_field);
        if (obj.contains(key)) {
            auto field_value = obj[key];
            // 根据类型进行比较
            if (field_value.is_numeric() && m_value.is_numeric()) {
                if (field_value.is_integer() && m_value.is_integer()) {
                    return field_value.template as<int64_t>() < m_value.template as<int64_t>();
                } else if (field_value.is_double() && m_value.is_double()) {
                    return field_value.template as<double>() < m_value.template as<double>();
                } else if (field_value.is_integer() && m_value.is_double()) {
                    return static_cast<double>(field_value.template as<int64_t>()) <
                           m_value.template as<double>();
                } else if (field_value.is_double() && m_value.is_integer()) {
                    return field_value.template as<double>() <
                           static_cast<double>(m_value.template as<int64_t>());
                }
            } else if (field_value.is_string() && m_value.is_string()) {
                return field_value.as_string() < m_value.as_string();
            }
        }
        return false;
    }

    // 通用对象处理方法
    template <typename T, typename std::enable_if_t<!std::is_same_v<T, mc::dict> &&
                                                        !std::is_same_v<T, mc::mutable_dict> &&
                                                        !std::is_same_v<T, mc::variant>,
                                                    int> = 0>
    bool eval_impl(const T& obj) const {
        // 使用反射获取对象字段值
        auto field_value =
            mc::reflect::get_property<std::remove_cv_t<std::remove_reference_t<T>>>(obj, m_field);

        // 根据类型进行比较
        if (field_value.is_numeric() && m_value.is_numeric()) {
            if (field_value.is_integer() && m_value.is_integer()) {
                return field_value.template as<int64_t>() < m_value.template as<int64_t>();
            } else if (field_value.is_double() && m_value.is_double()) {
                return field_value.template as<double>() < m_value.template as<double>();
            } else if (field_value.is_integer() && m_value.is_double()) {
                return static_cast<double>(field_value.template as<int64_t>()) <
                       m_value.template as<double>();
            } else if (field_value.is_double() && m_value.is_integer()) {
                return field_value.template as<double>() <
                       static_cast<double>(m_value.template as<int64_t>());
            }
        } else if (field_value.is_string() && m_value.is_string()) {
            return field_value.as_string() < m_value.as_string();
        }
        return false;
    }

    std::string m_field;
    mc::variant m_value;
};

/**
 * 小于等于条件，表示 field <= value
 */
class less_equal_condition : public base_condition {
public:
    less_equal_condition(std::string field, mc::variant value)
        : m_field(std::move(field)), m_value(std::move(value)) {
    }

    condition_type get_type() const override {
        return condition_type::less_equal_condition;
    }

    const std::string& get_field() const {
        return m_field;
    }

    const mc::variant& get_value() const {
        return m_value;
    }

protected:
    bool eval_impl(const mc::variant& obj) const override {
        if (obj.is_dict()) {
            const auto& dict = obj.as<mc::dict>();
            std::string key(m_field);
            if (dict.contains(key)) {
                auto field_value = dict[key];
                // 根据类型进行比较
                if (field_value.is_numeric() && m_value.is_numeric()) {
                    if (field_value.is_integer() && m_value.is_integer()) {
                        return field_value.template as<int64_t>() <= m_value.template as<int64_t>();
                    } else if (field_value.is_double() && m_value.is_double()) {
                        return field_value.template as<double>() <= m_value.template as<double>();
                    } else if (field_value.is_integer() && m_value.is_double()) {
                        return static_cast<double>(field_value.template as<int64_t>()) <=
                               m_value.template as<double>();
                    } else if (field_value.is_double() && m_value.is_integer()) {
                        return field_value.template as<double>() <=
                               static_cast<double>(m_value.template as<int64_t>());
                    }
                } else if (field_value.is_string() && m_value.is_string()) {
                    return field_value.as_string() <= m_value.as_string();
                }
            }
        }
        return false;
    }

private:
    // 对不同类型进行特化的eval_impl方法
    template <typename Dict, typename std::enable_if_t<std::is_same_v<Dict, mc::dict> ||
                                                           std::is_same_v<Dict, mc::mutable_dict>,
                                                       int> = 0>
    bool eval_impl(const Dict& obj) const {
        std::string key(m_field);
        if (obj.contains(key)) {
            auto field_value = obj[key];
            // 根据类型进行比较
            if (field_value.is_numeric() && m_value.is_numeric()) {
                if (field_value.is_integer() && m_value.is_integer()) {
                    return field_value.template as<int64_t>() <= m_value.template as<int64_t>();
                } else if (field_value.is_double() && m_value.is_double()) {
                    return field_value.template as<double>() <= m_value.template as<double>();
                } else if (field_value.is_integer() && m_value.is_double()) {
                    return static_cast<double>(field_value.template as<int64_t>()) <=
                           m_value.template as<double>();
                } else if (field_value.is_double() && m_value.is_integer()) {
                    return field_value.template as<double>() <=
                           static_cast<double>(m_value.template as<int64_t>());
                }
            } else if (field_value.is_string() && m_value.is_string()) {
                return field_value.as_string() <= m_value.as_string();
            }
        }
        return false;
    }

    // 通用对象处理方法
    template <typename T, typename std::enable_if_t<!std::is_same_v<T, mc::dict> &&
                                                        !std::is_same_v<T, mc::mutable_dict> &&
                                                        !std::is_same_v<T, mc::variant>,
                                                    int> = 0>
    bool eval_impl(const T& obj) const {
        // 使用反射获取对象字段值
        auto field_value =
            mc::reflect::get_property<std::remove_cv_t<std::remove_reference_t<T>>>(obj, m_field);

        // 根据类型进行比较
        if (field_value.is_numeric() && m_value.is_numeric()) {
            if (field_value.is_integer() && m_value.is_integer()) {
                return field_value.template as<int64_t>() <= m_value.template as<int64_t>();
            } else if (field_value.is_double() && m_value.is_double()) {
                return field_value.template as<double>() <= m_value.template as<double>();
            } else if (field_value.is_integer() && m_value.is_double()) {
                return static_cast<double>(field_value.template as<int64_t>()) <=
                       m_value.template as<double>();
            } else if (field_value.is_double() && m_value.is_integer()) {
                return field_value.template as<double>() <=
                       static_cast<double>(m_value.template as<int64_t>());
            }
        } else if (field_value.is_string() && m_value.is_string()) {
            return field_value.as_string() <= m_value.as_string();
        }
        return false;
    }

    std::string m_field;
    mc::variant m_value;
};

} // namespace mc::database::query

#endif // MC_DATABASE_QUERY_CONDITION_H