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

#include <mc/expr/context.h>

#include "expr/node/include/literal_node.h"

namespace mc::expr {

mc::variant literal_node::evaluate(const context_base& ctx) const {
    return m_value;
}

std::string literal_node::to_string() const {
    if (m_value.is_string()) {
        return "\"" + m_value.get_string() + "\"";
    } else {
        return m_value.to_string();
    }
}

node_ptr make_literal(mc::variant value) {
    return std::make_shared<literal_node>(std::move(value));
}

} // namespace mc::expr