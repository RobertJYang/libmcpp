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

namespace mc::expr {
class function;
class context_impl;

/**
 * @brief 表达式上下文类，用于存储变量和函数
 */
class context {
public:
    /**
     * @brief 默认构造函数
     */
    context();

    /**
     * @brief 带父级上下文的构造函数
     * @param parent 父级上下文
     */
    explicit context(const context* parent);

    /**
     * @brief 从dict构造上下文
     */
    explicit context(const mc::dict& dict, const context* parent = nullptr);

    /**
     * @brief 移动构造函数
     */
    context(context&& other) noexcept = default;

    /**
     * @brief 移动赋值运算符
     */
    context& operator=(context&& other) noexcept = default;

    /**
     * @brief 拷贝构造函数
     */
    context(const context& other) = default;

    /**
     * @brief 拷贝赋值运算符
     */
    context& operator=(const context& other) = default;

    /**
     * @brief 设置父级上下文
     */
    void set_parent(const context& parent);

    /**
     * @brief 获取父级上下文
     */
    context get_parent() const;

    /**
     * @brief 设置变量值
     * @return 变量 ID
     */
    int register_variable(std::string name, const mc::variant& value);

    /**
     * @brief 获取变量值
     * @note 如果当前上下文找不到变量，会尝试从父级上下文查找
     */
    mc::variant& get_variable(std::string_view name) const;

    /**
     * @brief 获取变量值
     * @note 如果当前上下文找不到变量，会尝试从父级上下文查找
     */
    mc::variant& get_variable(int id) const;

    /**
     * @brief 判断变量是否存在
     * @note 会递归查找父级上下文
     */
    bool has_variable(std::string_view name) const;

    /**
     * @brief 从dict导入变量
     */
    void import_from_dict(const mc::dict& dict);

    /**
     * @brief 注册函数
     * @return 函数 ID
     */
    int register_function(std::shared_ptr<function> func);

    /**
     * @brief 获取函数
     * @note 如果当前上下文找不到函数，会尝试从父级上下文查找
     */
    std::shared_ptr<function> get_function(std::string_view name) const;

    /**
     * @brief 判断函数是否存在
     * @note 会递归查找父级上下文
     */
    bool has_function(std::string_view name) const;

    /**
     * @brief 检查上下文是否为空
     * @return 如果上下文是默认构造的空上下文，则返回 true
     */
    bool is_empty() const;

private:
    std::shared_ptr<context_impl> m_impl;
};

} // namespace mc::expr

#endif // MC_EXPR_CONTEXT_H