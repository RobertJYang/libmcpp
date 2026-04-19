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
 * @file property_access_node.h
 * @brief 定义了表达式语法树的属性访问节点
 */
#ifndef MC_EXPR_NODE_PROPERTY_ACCESS_NODE_H
#define MC_EXPR_NODE_PROPERTY_ACCESS_NODE_H

#include <mc/expr/node.h>

#include <memory>
#include <string>

namespace mc::expr {

/**
 * @brief 属性访问节点
 */
class property_access_node : public node {
public:
    property_access_node(node_ptr object, std::string property)
        : m_object(std::move(object)), m_property(std::move(property))
    {}

    node_type get_type() const override
    {
        return node_type::property_access;
    }

    mc::variant evaluate(const context_base& ctx) const override;

    mc::string to_string() const override;

    const node& get_object() const
    {
        return *m_object;
    }

    const std::string& get_property() const
    {
        return m_property;
    }

private:
    node_ptr    m_object;
    std::string m_property;
};

} // namespace mc::expr

#endif // MC_EXPR_NODE_PROPERTY_ACCESS_NODE_H
