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
 * @file object_method_call_node.h
 * @brief 定义了表达式语法树的对象方法调用节点
 */
#ifndef MC_EXPR_NODE_OBJECT_METHOD_CALL_NODE_H
#define MC_EXPR_NODE_OBJECT_METHOD_CALL_NODE_H

#include <mc/expr/node.h>

#include <memory>
#include <string>
#include <vector>

namespace mc::expr {

/**
 * @brief 对象方法调用节点
 */
class object_method_call_node : public node {
public:
    object_method_call_node(node_ptr object, std::string method_name, node_ptrs args)
        : m_object(std::move(object)), m_method_name(std::move(method_name)), m_args(std::move(args))
    {}

    node_type get_type() const override
    {
        return node_type::object_method_call;
    }

    mc::variant evaluate(const context_base& ctx) const override;

    std::string to_string() const override;

    const node& get_object() const
    {
        return *m_object;
    }

    const std::string& get_method_name() const
    {
        return m_method_name;
    }

    const node_ptrs& get_args() const
    {
        return m_args;
    }

private:
    node_ptr    m_object;
    std::string m_method_name;
    node_ptrs   m_args;
};

} // namespace mc::expr

#endif // MC_EXPR_NODE_OBJECT_METHOD_CALL_NODE_H
