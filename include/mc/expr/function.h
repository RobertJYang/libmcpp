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
 * @brief 定义了表达式函数接口
 */
#ifndef MC_EXPR_FUNCTION_H
#define MC_EXPR_FUNCTION_H

#include <functional>
#include <mc/variant.h>
#include <memory>
#include <string>
#include <vector>

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
    virtual std::string get_name() const = 0;

    /**
     * @brief 获取参数数量
     * @return 参数数量，如果返回-1表示可变参数
     */
    virtual int get_arg_count() const = 0;
};

/**
 * @brief 简单函数实现
 */
class simple_function : public function {
public:
    using function_type = std::function<mc::variant(const mc::variants&)>;

    simple_function(std::string name, function_type func, int arg_count = -1)
        : m_name(std::move(name)), m_func(std::move(func)), m_arg_count(arg_count) {
    }

    mc::variant call(const mc::variants& args) const override {
        return m_func(args);
    }

    std::string get_name() const override {
        return m_name;
    }

    int get_arg_count() const override {
        return m_arg_count;
    }

private:
    std::string   m_name;
    function_type m_func;
    int           m_arg_count;
};

/**
 * @brief 创建一个简单函数
 */
std::shared_ptr<function> make_simple_function(const std::string&             name,
                                               simple_function::function_type func,
                                               int                            arg_count = -1);

} // namespace mc::expr

#endif // MC_EXPR_FUNCTION_H