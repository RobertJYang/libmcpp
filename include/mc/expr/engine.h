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
class engine {
public:
    /**
     * @brief 默认构造函数
     */
    engine();

    /**
     * @brief 析构函数
     */
    ~engine();

    /**
     * @brief 编译表达式
     * @param expr 表达式字符串
     * @return 表达式语法树根节点
     */
    std::shared_ptr<node> compile(std::string_view expr);

    /**
     * @brief 求值表达式
     * @param expr 表达式字符串
     * @param ctx 表达式上下文
     * @return 表达式计算结果
     */
    mc::variant evaluate(std::string_view expr, const context& ctx = {});

    /**
     * @brief 注册函数
     * @param func 函数对象
     */
    void register_function(std::shared_ptr<function> func);

    /**
     * @brief 创建上下文
     * @return 初始化了内置函数的上下文
     */
    context create_context() const;

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace mc::expr

#endif // MC_EXPR_ENGINE_H