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

#include <mc/engine/path_template.h>
#include <mc/expr/context.h>
#include <mc/expr/function.h>
#include <mc/expr/node.h>
#include <mc/string_view.h>
#include <mc/variant.h>
#include <memory>

namespace mc::expr {

/**
 * @brief 表达式引擎类
 */
class MC_API engine {
public:
    /**
     * @brief 默认构造函数
     */
    engine();

    /**
     * @brief 析构函数
     */
    ~engine();

    static engine& get_instance();

    /**
     * @brief 编译表达式
     * @param expr 表达式字符串
     * @return 表达式语法树根节点
     */
    node_ptr compile(mc::string_view expr);

    /**
     * @brief 求值表达式
     * @param expr 表达式字符串
     * @param ctx 表达式上下文
     * @return 表达式计算结果
     */
    mc::variant evaluate(mc::string_view expr, const context_base& ctx);

    /**
     * @brief 获取全局上下文
     * @return 包含所有内置函数的全局上下文
     */
    context& get_global_context() const;

    /**
     * @brief 创建上下文
     * @param parent 父级上下文指针，如果为空则使用全局上下文
     * @return 上下文对象
     */
    context make_context(context_base* parent = nullptr) const;

    /**
     * @brief 创建带有指定变量的上下文
     * @param variables 初始变量集合
     * @param parent 父级上下文指针，如果为空则使用全局上下文
     * @return 上下文对象
     */
    context make_context(const mc::dict& variables, context_base* parent = nullptr) const;

    /**
     * @brief 创建上下文
     * @param object 对象指针
     * @param parent 父级上下文指针，如果为空则使用全局上下文
     * @return 上下文对象
     */
    object_context make_context(mc::engine::abstract_object* object, context_base* parent = nullptr) const;

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
};

MC_API mc::shared_ptr<mc::engine::path_template_backend> create_path_template_backend();
MC_API void                                              register_path_template_backend();

} // namespace mc::expr

#endif // MC_EXPR_ENGINE_H