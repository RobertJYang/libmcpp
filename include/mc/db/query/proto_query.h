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
 * @file proto_query.h
 * @brief 使用 Boost.Proto 实现的查询 DSL
 */
#ifndef MC_DATABASE_QUERY_PROTO_QUERY_H
#define MC_DATABASE_QUERY_PROTO_QUERY_H

#include <boost/proto/proto.hpp>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <mc/db/query/condition.h>
#include <mc/variant.h>

namespace mc::db::query {

namespace proto = boost::proto;

/**
 * 查询 DSL 的命名空间
 */
namespace dsl {

/**
 * 字段引用，表示查询中的一个字段
 */
class field_ref {
public:
    explicit field_ref(std::string_view name) : m_name(name) {
    }

    const std::string& name() const {
        return m_name;
    }

private:
    std::string m_name;
};

/**
 * 定义查询域
 */
struct query_domain;

/**
 * 查询表达式，所有查询DSL表达式的基类
 */
template <typename Expr>
struct query_expr : proto::extends<Expr, query_expr<Expr>, query_domain> {
    using base_type = proto::extends<Expr, query_expr<Expr>, query_domain>;

    query_expr(const Expr& expr = Expr()) : base_type(expr) {
    }

    /**
     * 将表达式转换为条件对象
     */
    condition as_condition() const;
};

/**
 * 查询域定义
 */
struct query_domain : proto::domain<proto::generator<query_expr>> {};

/**
 * 定义是否为终结符的元函数
 */
template <typename T>
struct is_terminal : proto::matches<T, proto::terminal<proto::_>> {};

/**
 * 查询语法定义 - 简化版
 */
struct query_grammar : proto::or_<
                           // 终结符
                           proto::terminal<field_ref>, proto::terminal<proto::_>,

                           // 二元操作
                           proto::binary_expr<proto::_, proto::_, proto::_>> {};

// 前向声明转换上下文
struct condition_context;

/**
 * 创建字段引用表达式
 */
inline query_expr<proto::terminal<field_ref>::type> field(std::string_view name) {
    proto::terminal<field_ref>::type term = {{field_ref(name)}};
    return query_expr<proto::terminal<field_ref>::type>(term);
}

/**
 * 特殊操作：BETWEEN
 */
template <typename T>
inline condition between(const query_expr<proto::terminal<field_ref>::type>& field_expr,
                         const T& lower, const T& upper) {
    const field_ref& field = proto::value(field_expr);
    mc::variants     range = {mc::variant(lower), mc::variant(upper)};
    return condition(compare_op::between, std::string(field.name()), mc::variant(range));
}

/**
 * 特殊操作：IN
 */
template <typename T>
inline condition in(const query_expr<proto::terminal<field_ref>::type>& field_expr,
                    const std::initializer_list<T>&                     values) {
    const field_ref& field = proto::value(field_expr);
    mc::variants     variants;
    for (const auto& value : values) {
        variants.push_back(mc::variant(value));
    }
    return condition(compare_op::in, std::string(field.name()), mc::variant(variants));
}

/**
 * 特殊操作：LIKE
 */
inline condition like(const query_expr<proto::terminal<field_ref>::type>& field_expr,
                      const std::string&                                  pattern) {
    const field_ref& field = proto::value(field_expr);
    return condition(compare_op::like, std::string(field.name()), mc::variant(pattern));
}

/**
 * 特殊操作：CONTAINS
 */
inline condition contains(const query_expr<proto::terminal<field_ref>::type>& field_expr,
                          const std::string&                                  substring) {
    const field_ref& field = proto::value(field_expr);
    return condition(compare_op::contains, std::string(field.name()), mc::variant(substring));
}

/**
 * 将值转换为 mc::variant
 */
struct convert_value : proto::callable {
    template <typename Sig>
    struct result;

    template <typename This, typename T>
    struct result<This(T)> {
        using type = mc::variant;
    };

    template <typename T>
    mc::variant operator()(const T& value) const {
        return mc::variant(value);
    }

    // 字符串字面量特化
    mc::variant operator()(const char* value) const {
        return mc::variant(std::string(value));
    }
};

/**
 * 条件上下文 - 用于Proto表达式求值
 */
struct condition_context : boost::proto::callable_context<condition_context> {
    // 为Proto求值系统提供eval模板类型
    template <typename Expr>
    struct eval {
        typedef condition result_type;

        // 调用运算符，用于求值表达式
        condition operator()(const Expr& expr, condition_context& ctx) const {
            return ctx(typename boost::proto::tag_of<Expr>::type(), expr);
        }
    };

    // 处理终结符 - 字段引用
    condition
    operator()(boost::proto::tag::terminal,
               const query_expr<typename boost::proto::terminal<field_ref>::type>& expr) const {
        throw std::runtime_error("不能直接计算字段引用");
    }

    // 处理常量终结符 (整数、字符串等)
    template <typename T>
    condition operator()(boost::proto::tag::terminal,
                         const query_expr<typename boost::proto::terminal<T>::type>& expr) const {
        throw std::runtime_error("不能直接计算常量");
    }

    // 等于操作符
    template <typename LeftExpr, typename RightExpr>
    condition operator()(boost::proto::tag::equal_to,
                         const query_expr<boost::proto::exprns_::basic_expr<
                             boost::proto::tagns_::tag::equal_to,
                             boost::proto::argsns_::list2<LeftExpr, RightExpr>, 2>>& expr) const {
        const auto& left  = boost::proto::left(expr);
        const auto& right = boost::proto::right(expr);

        if constexpr (std::is_same_v<
                          typename std::decay<LeftExpr>::type,
                          query_expr<typename boost::proto::terminal<field_ref>::type>>) {
            const field_ref& field = boost::proto::value(left);
            return conditions::eq(field.name(), boost::proto::value(right));
        } else {
            throw std::runtime_error("不支持的等于操作");
        }
    }

    // 不等于操作符
    template <typename LeftExpr, typename RightExpr>
    condition operator()(boost::proto::tag::not_equal_to,
                         const query_expr<boost::proto::exprns_::basic_expr<
                             boost::proto::tagns_::tag::not_equal_to,
                             boost::proto::argsns_::list2<LeftExpr, RightExpr>, 2>>& expr) const {
        const auto& left  = boost::proto::left(expr);
        const auto& right = boost::proto::right(expr);

        if constexpr (std::is_same_v<
                          typename std::decay<LeftExpr>::type,
                          query_expr<typename boost::proto::terminal<field_ref>::type>>) {
            const field_ref& field = boost::proto::value(left);
            return conditions::ne(field.name(), boost::proto::value(right));
        } else {
            throw std::runtime_error("不支持的不等于操作");
        }
    }

    // 大于操作符
    template <typename LeftExpr, typename RightExpr>
    condition operator()(boost::proto::tag::greater,
                         const query_expr<boost::proto::exprns_::basic_expr<
                             boost::proto::tagns_::tag::greater,
                             boost::proto::argsns_::list2<LeftExpr, RightExpr>, 2>>& expr) const {
        const auto& left  = boost::proto::left(expr);
        const auto& right = boost::proto::right(expr);

        if constexpr (std::is_same_v<
                          typename std::decay<LeftExpr>::type,
                          query_expr<typename boost::proto::terminal<field_ref>::type>>) {
            const field_ref& field = boost::proto::value(left);
            return conditions::gt(field.name(), boost::proto::value(right));
        } else {
            throw std::runtime_error("不支持的大于操作");
        }
    }

    // 大于等于操作符
    template <typename LeftExpr, typename RightExpr>
    condition operator()(boost::proto::tag::greater_equal,
                         const query_expr<boost::proto::exprns_::basic_expr<
                             boost::proto::tagns_::tag::greater_equal,
                             boost::proto::argsns_::list2<LeftExpr, RightExpr>, 2>>& expr) const {
        const auto& left  = boost::proto::left(expr);
        const auto& right = boost::proto::right(expr);

        if constexpr (std::is_same_v<
                          typename std::decay<LeftExpr>::type,
                          query_expr<typename boost::proto::terminal<field_ref>::type>>) {
            const field_ref& field = boost::proto::value(left);
            return conditions::ge(field.name(), boost::proto::value(right));
        } else {
            throw std::runtime_error("不支持的大于等于操作");
        }
    }

    // 小于操作符
    template <typename LeftExpr, typename RightExpr>
    condition operator()(
        boost::proto::tag::less,
        const query_expr<boost::proto::exprns_::basic_expr<
            boost::proto::tagns_::tag::less, boost::proto::argsns_::list2<LeftExpr, RightExpr>, 2>>&
            expr) const {
        const auto& left  = boost::proto::left(expr);
        const auto& right = boost::proto::right(expr);

        if constexpr (std::is_same_v<
                          typename std::decay<LeftExpr>::type,
                          query_expr<typename boost::proto::terminal<field_ref>::type>>) {
            const field_ref& field = boost::proto::value(left);
            return conditions::lt(field.name(), boost::proto::value(right));
        } else {
            throw std::runtime_error("不支持的小于操作");
        }
    }

    // 小于等于操作符
    template <typename LeftExpr, typename RightExpr>
    condition operator()(boost::proto::tag::less_equal,
                         const query_expr<boost::proto::exprns_::basic_expr<
                             boost::proto::tagns_::tag::less_equal,
                             boost::proto::argsns_::list2<LeftExpr, RightExpr>, 2>>& expr) const {
        const auto& left  = boost::proto::left(expr);
        const auto& right = boost::proto::right(expr);

        if constexpr (std::is_same_v<
                          typename std::decay<LeftExpr>::type,
                          query_expr<typename boost::proto::terminal<field_ref>::type>>) {
            const field_ref& field = boost::proto::value(left);
            return conditions::le(field.name(), boost::proto::value(right));
        } else {
            throw std::runtime_error("不支持的小于等于操作");
        }
    }

    // 逻辑与操作符
    template <typename LeftExpr, typename RightExpr>
    condition operator()(boost::proto::tag::logical_and,
                         const query_expr<boost::proto::exprns_::basic_expr<
                             boost::proto::tagns_::tag::logical_and,
                             boost::proto::argsns_::list2<LeftExpr, RightExpr>, 2>>& expr) const {
        condition left_cond  = boost::proto::left(expr).as_condition();
        condition right_cond = boost::proto::right(expr).as_condition();
        return conditions::and_cond(std::vector<condition>{left_cond, right_cond});
    }

    // 逻辑或操作符
    template <typename LeftExpr, typename RightExpr>
    condition operator()(boost::proto::tag::logical_or,
                         const query_expr<boost::proto::exprns_::basic_expr<
                             boost::proto::tagns_::tag::logical_or,
                             boost::proto::argsns_::list2<LeftExpr, RightExpr>, 2>>& expr) const {
        condition left_cond  = boost::proto::left(expr).as_condition();
        condition right_cond = boost::proto::right(expr).as_condition();
        return conditions::or_cond(std::vector<condition>{left_cond, right_cond});
    }
};

// 实现 query_expr::as_condition()
template <typename Expr>
condition query_expr<Expr>::as_condition() const {
    condition_context ctx;
    return boost::proto::eval(*this, ctx);
}

} // namespace dsl

/**
 * 将条件表达式转换为condition对象
 */
template <typename Expr>
condition to_condition(const dsl::query_expr<Expr>& expr) {
    return expr.as_condition();
}

// 转发特殊操作
using dsl::between;
using dsl::contains;
using dsl::field;
using dsl::in;
using dsl::like;

} // namespace mc::db::query

#endif // MC_DATABASE_QUERY_PROTO_QUERY_H