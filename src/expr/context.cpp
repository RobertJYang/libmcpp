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

#include <mc/exception.h>
#include <mc/expr/context.h>
#include <mc/expr/error.h>
#include <mc/expr/function.h>

namespace mc::expr {

static const mc::variant empty_variant;

context::context(const mc::dict& dict) {
    import_from_dict(dict);
}

void context::set_variable(const std::string& name, const mc::variant& value) {
    m_variables[name] = value;
}

const mc::variant& context::get_variable(const std::string& name) const {
    auto it = m_variables.find(name);
    if (it != m_variables.end()) {
        return it->second;
    }

    return empty_variant;
}

bool context::has_variable(const std::string& name) const {
    return m_variables.find(name) != m_variables.end();
}

void context::import_from_dict(const mc::dict& dict) {
    for (const auto& entry : dict) {
        const auto& key = entry.key;
        if (key.is_string()) {
            m_variables[key.get_string()] = entry.value;
        }
    }
}

void context::set_function(std::shared_ptr<function> func) {
    if (!func) {
        MC_THROW(error, "表达式上下文错误: 函数指针不能为空");
    }

    m_functions[func->get_name()] = std::move(func);
}

std::shared_ptr<function> context::get_function(const std::string& name) const {
    auto it = m_functions.find(name);
    if (it != m_functions.end()) {
        return it->second;
    }

    return nullptr;
}

bool context::has_function(const std::string& name) const {
    return m_functions.find(name) != m_functions.end();
}

} // namespace mc::expr