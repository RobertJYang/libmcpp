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
 * @file engine.h
 * @brief 定义了表达式引擎类
 */
#ifndef MC_EXPR_ENGINE_H
#define MC_EXPR_ENGINE_H

#include <glib-2.0/glib.h>
#include <mc/expr/context.h>
#include <mc/expr/function.h>
#include <mc/expr/node.h>
#include <mc/variant.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mc::expr {

/**
 * @brief 表达式引擎类
 */
class MC_API engine {
public:
    /**
     * @brief 默认构造函数
     */
    MC_API engine();

    /**
     * @brief 析构函数
     */
    MC_API ~engine();

    MC_API static engine& get_instance();

    /**
     * @brief 编译表达式
     * @param expr 表达式字符串
     * @return 表达式语法树根节点
     */
    MC_API node_ptr compile(std::string_view expr);

    /**
     * @brief 求值表达式
     * @param expr 表达式字符串
     * @param ctx 表达式上下文
     * @return 表达式计算结果
     */
    MC_API mc::variant evaluate(std::string_view expr, const context_base& ctx);

    /**
     * @brief 求值表达式并返回GVariant类型结果
     * @param expr 表达式字符串
     * @param ctx 表达式上下文
     * @return 表达式计算结果（GVariant类型）
     */
    MC_API GVariant* evaluate_as_gvariant(std::string_view expr, const context_base& ctx);

    /**
     * @brief 获取全局上下文
     * @return 包含所有内置函数的全局上下文
     */
    MC_API context& get_global_context() const;

    /**
     * @brief 创建上下文
     * @param parent 父级上下文指针，如果为空则使用全局上下文
     * @return 上下文对象
     */
    MC_API context make_context(context_base* parent = nullptr) const;

    /**
     * @brief 创建带有指定变量的上下文
     * @param variables 初始变量集合
     * @param parent 父级上下文指针，如果为空则使用全局上下文
     * @return 上下文对象
     */
    MC_API context make_context(const mc::dict& variables, context_base* parent = nullptr) const;

    /**
     * @brief 创建上下文
     * @param object 对象指针
     * @param parent 父级上下文指针，如果为空则使用全局上下文
     * @return 上下文对象
     */
    MC_API object_context make_context(mc::engine::abstract_object* object,
                                       context_base*                parent = nullptr) const;

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace mc::expr

#endif // MC_EXPR_ENGINE_H