/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <iostream>
#include <map>
#include <mc/expr/engine.h>
#include <mc/expr/function/collection.h>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace mc;

namespace mc::expr {

func_collection::func_collection() = default;

func_collection& func_collection::get_instance()
{
    static func_collection instance;
    return instance;
}

void func_collection::add(const std::string_view& position, std::shared_ptr<mc::engine::service> service, mc::dict& functions)
{
    std::lock_guard lock(m_mutex);
    std::string     position_str(position);
    m_services[position_str] = service;
    m_functions.insert(position, functions);
}

mc::variant func_collection::get(const std::string_view& position, const std::string_view& func_name)
{
    if (m_functions.find(position) == m_functions.end()) {
        return mc::variant();
    }

    auto functions = m_functions[position].as<mc::dict>();
    if (functions.find(func_name) == functions.end()) {
        return mc::variant();
    }

    return functions[func_name];
}

std::shared_ptr<mc::engine::service> func_collection::get_service(const std::string_view& position)
{
    std::lock_guard lock(m_mutex);
    std::string     position_str(position);
    auto            it = m_services.find(position_str);
    if (it == m_services.end()) {
        return nullptr;
    }
    return it->second;
}

mc::dict func_collection::get(const std::string_view& position)
{
    if (m_functions.find(position) == m_functions.end()) {
        return mc::dict();
    }
    return m_functions[position].as<mc::dict>();
}

mc::dict func_collection::remove(const std::string_view& position)
{
    if (m_functions.find(position) == m_functions.end()) {
        return mc::dict();
    }
    std::lock_guard lock(m_mutex);
    mc::dict        result = m_functions[position].as<mc::dict>();
    m_functions.erase(position);

    std::string position_str(position);
    m_services.erase(position_str);

    return result;
}

bool func_collection::contains(const std::string_view& position)
{
    return m_functions.find(position) != m_functions.end();
}

void func_collection::clear()
{
    std::lock_guard lock(m_mutex);
    m_functions.clear();
    m_services.clear();
}

size_t func_collection::size() const
{
    std::lock_guard lock(m_mutex);
    return m_functions.size();
}

bool func_collection::empty() const
{
    std::lock_guard lock(m_mutex);
    return m_functions.empty() && m_services.empty();
}

} // namespace mc::expr