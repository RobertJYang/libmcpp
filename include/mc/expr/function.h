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
 * @file function.h
 * @brief 定义了表达式函数接口和相关实现
 */
#ifndef MC_EXPR_FUNCTION_H
#define MC_EXPR_FUNCTION_H

#include <functional>
#include <mc/exception.h>
#include <mc/pretty_name.h>
#include <mc/reflect/metadata_info.h>
#include <mc/traits.h>
#include <mc/variant.h>

namespace mc::expr {

/**
 * @brief 表达式函数接口
 */
class function {
public:
    virtual ~function() = default;

    /**
     * @brief 调用函数
     * @param args 参数列表
     * @return 函数返回值
     */
    virtual mc::variant call(const mc::variants& args) const = 0;

    /**
     * @brief 获取函数名称
     */
    virtual const std::string& get_name() const = 0;

    /**
     * @brief 获取参数数量
     * @return 参数数量，如果返回-1表示可变参数
     */
    virtual int get_arg_count() const = 0;
};

/**
 * @brief 模板函数实现
 */
template <typename RetType, typename... Args>
class simple_function : public function {
public:
    using function_type = std::function<RetType(Args...)>;
    using result_type   = mc::traits::remove_cvref_t<RetType>;
    using args_type     = std::tuple<mc::traits::remove_cvref_t<Args>...>;

    // 静态断言确保返回类型可以转换为 mc::variant
    static_assert(std::is_void_v<RetType> || mc::reflect::is_variant_constructible_v<RetType>,
                  "函数返回类型必须是 void 或者可以转换为 mc::variant");

    // 静态断言确保所有参数类型都可以从 mc::variant 转换
    static_assert(mc::reflect::all_variant_constructible_v<mc::traits::remove_cvref_t<Args>...>,
                  "参数类型必须可转换为 mc::variant");

    simple_function(std::string name, function_type func)
        : m_name(std::move(name)), m_func(std::move(func)) {
    }

    template <size_t... I>
    variant call_with_exact_args(const variants& args, std::index_sequence<I...>) const {
        if constexpr (std::is_void_v<RetType>) {
            m_func(mc::reflect::detail::convert_arg<mc::traits::remove_cvref_t<Args>>(m_name,
                                                                                      args[I])...);
            return {};
        } else {
            return m_func(mc::reflect::detail::convert_arg<mc::traits::remove_cvref_t<Args>>(
                m_name, args[I])...);
        }
    }

    mc::variant call(const mc::variants& args) const override {
        constexpr size_t arg_count = sizeof...(Args);

        if constexpr (std::is_same_v<args_type, std::tuple<mc::variants>>) {
            if constexpr (std::is_void_v<RetType>) {
                m_func(args);
                return {};
            } else {
                return mc::variant(m_func(args));
            }
        } else {
            MC_ASSERT_THROW(args.size() == arg_count, mc::invalid_arg_exception,
                            "函数参数不匹配: ${func_name} 期望 ${expected}, 实际 ${actual}",
                            ("func_name", m_name)("expected", arg_count)("actual", args.size()));
            return call_with_exact_args(args, std::make_index_sequence<arg_count>());
        }
    }

    const std::string& get_name() const override {
        return m_name;
    }

    int get_arg_count() const override {
        return static_cast<int>(sizeof...(Args));
    }

private:
    std::string   m_name;
    function_type m_func;
};

template <typename Func>
auto make_simple_function(std::string name, Func&& func) {
    using func_traits          = mc::traits::function_traits<Func>;
    using func_args            = typename func_traits::args_type;
    using return_type          = typename func_traits::return_type;
    using ret_and_agrs         = mc::traits::type_prepend_t<func_args, return_type>;
    using simple_function_type = typename mc::traits::apply_type_t<simple_function, ret_and_agrs>;

    return std::make_shared<simple_function_type>(std::move(name), func);
}

} // namespace mc::expr

#endif // MC_EXPR_FUNCTION_H