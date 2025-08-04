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
#include <mc/expr/function.h>
#include <mc/reflect.h>
#include <mc/variant.h>

#include <memory>
#include <string>

namespace mc::engine {
class abstract_object;
}

namespace mc::expr {
class function;
class context_impl;

class MC_API context_base {
public:
    virtual ~context_base() = default;

    /**
     * @brief 带父级上下文的构造函数
     * @param parent 父级上下文
     */
    explicit context_base(context_base* parent = nullptr);

    context_base(const context_base&);
    context_base& operator=(const context_base&);
    context_base(context_base&&) noexcept;
    context_base& operator=(context_base&&) noexcept;

    /**
     * @brief 设置父级上下文
     */
    virtual void set_parent(context_base* parent);

    /**
     * @brief 获取父级上下文
     */
    context_base* get_parent() const;

    /**
     * @brief 判断变量是否存在
     * @note 会递归查找父级上下文
     */
    virtual bool has_variable(std::string_view name, std::string_view iface = {}) const;

    /**
     * @brief 判断函数是否存在
     * @note 会递归查找父级上下文
     */
    virtual bool has_function(std::string_view name, std::string_view iface = {}) const;

    /**
     * @brief 获取变量值
     * @param name 变量名
     * @return 变量值
     */
    virtual const mc::variant& get_variable(std::string_view name,
                                            std::string_view iface = {}) const;

    /**
     * @brief 函数调用
     * @param name 函数名
     * @param args 参数
     * @return 返回值
     */
    virtual mc::variant invoke(std::string_view name, const mc::variants& args,
                               std::string_view iface = {}) const;

protected:
    context_base* m_parent{nullptr};
};

/**
 * @brief 表达式上下文类，用于存储变量和函数
 */
class MC_API context : public context_base {
public:
    /**
     * @brief 默认构造函数
     */
    context();

    /**
     * @brief 带父级上下文的构造函数
     * @param parent 父级上下文
     */
    explicit context(context_base* parent);

    /**
     * @brief 从dict构造上下文
     */
    explicit context(const mc::dict& dict, context_base* parent = nullptr);

    context(const context& other);
    context& operator=(const context& other);
    context(context&& other) noexcept;
    context& operator=(context&& other) noexcept;

    /**
     * @brief 设置变量值
     * @return 变量 ID
     */
    int register_variable(std::string name, const mc::variant& value);

    /**
     * @brief 获取变量值
     * @note 如果当前上下文找不到变量，会尝试从父级上下文查找
     */
    const mc::variant& get_variable(std::string_view name,
                                    std::string_view iface = {}) const override;

    /**
     * @brief 判断变量是否存在
     * @note 会递归查找父级上下文
     */
    bool has_variable(std::string_view name, std::string_view iface = {}) const override;

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
     * @brief 注册对象
     * @return 对象 ID
     */
    int register_object(std::string name, mc::engine::abstract_object* obj);

    /**
     * @brief 判断函数是否存在
     * @note 会递归查找父级上下文
     */
    bool has_function(std::string_view name, std::string_view iface = {}) const override;

    /**
     * @brief 调用函数
     * @param name 函数名
     * @param args 参数
     * @return 返回值
     */
    mc::variant invoke(std::string_view name, const mc::variants& args,
                       std::string_view iface = {}) const override;

private:
    std::shared_ptr<context_impl> m_impl;
};

class MC_API object_context : public context_base {
public:
    object_context(mc::engine::abstract_object* obj, context_base* parent = nullptr);

    mc::engine::abstract_object* get_object() const;

    bool               has_variable(std::string_view name, std::string_view iface = {}) const override;
    bool               has_function(std::string_view name, std::string_view iface = {}) const override;
    const mc::variant& get_variable(std::string_view name,
                                    std::string_view iface = {}) const override;
    mc::variant        invoke(std::string_view name, const mc::variants& args,
                              std::string_view iface = {}) const override;

private:
    mc::engine::abstract_object* m_object;
    mutable mc::variant          m_property;
};

} // namespace mc::expr

#endif // MC_EXPR_CONTEXT_H