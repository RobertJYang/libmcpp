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
 * @file context.h
 * @brief 定义了表达式上下文类
 */
#ifndef MC_EXPR_CONTEXT_H
#define MC_EXPR_CONTEXT_H

#include <mc/dict.h>
#include <mc/variant.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace mc::expr {

// 前置声明
class function;

/**
 * @brief 表达式上下文类，用于存储变量和函数
 */
class context {
public:
    /**
     * @brief 默认构造函数
     */
    context() = default;

    /**
     * @brief 从dict构造上下文
     */
    explicit context(const mc::dict& dict);

    /**
     * @brief 设置变量值
     */
    void set_variable(const std::string& name, const mc::variant& value);

    /**
     * @brief 获取变量值
     */
    const mc::variant& get_variable(const std::string& name) const;

    /**
     * @brief 判断变量是否存在
     */
    bool has_variable(const std::string& name) const;

    /**
     * @brief 从dict导入变量
     */
    void import_from_dict(const mc::dict& dict);

    /**
     * @brief 设置函数
     */
    void set_function(std::shared_ptr<function> func);

    /**
     * @brief 获取函数
     */
    std::shared_ptr<function> get_function(const std::string& name) const;

    /**
     * @brief 判断函数是否存在
     */
    bool has_function(const std::string& name) const;

private:
    // 变量表
    std::unordered_map<std::string, mc::variant> m_variables;

    // 函数表
    std::unordered_map<std::string, std::shared_ptr<function>> m_functions;
};

} // namespace mc::expr

#endif // MC_EXPR_CONTEXT_H