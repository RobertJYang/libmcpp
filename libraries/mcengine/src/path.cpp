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

#include <mc/engine/path.h>
#include <mc/exception.h>

#include <algorithm>

namespace mc {
namespace engine {

path::path() : m_path("/")
{}

path::path(mc::string p)
{
    if (!is_valid(p)) {
        MC_THROW(mc::invalid_arg_exception, "无效的对象路径: ${path}", ("path", p));
    }
    m_path = std::move(p);
}

path::path(const char* p) : path(mc::string(p))
{}

const mc::string& path::str() const
{
    return m_path;
}

bool path::operator==(const path& other) const
{
    return m_path == other.m_path;
}

bool path::operator!=(const path& other) const
{
    return !(*this == other);
}

bool path::operator<(const path& other) const
{
    return m_path < other.m_path;
}

path path::operator/(mc::string_view element) const
{
    if (!is_valid_element(element)) {
        MC_THROW(mc::invalid_arg_exception, "无效的路径元素: ${element}", ("element", element));
    }

    mc::string new_path;
    if (m_path == "/") {
        new_path = m_path + element;
    } else {
        new_path = m_path + "/" + element;
    }
    return path(new_path);
}

path& path::operator=(mc::string p)
{
    if (!is_valid(p)) {
        MC_THROW(mc::invalid_arg_exception, "无效的对象路径: ${path}", ("path", p));
    }

    m_path = std::move(p);
    return *this;
}

path path::parent() const
{
    if (m_path == "/") {
        return *this;
    }

    size_t pos = m_path.find_last_of('/');
    if (pos == 0) {
        return path("/");
    }
    return path(m_path.substr(0, pos));
}

mc::string path::basename() const
{
    if (m_path == "/") {
        return "";
    }

    size_t pos = m_path.find_last_of('/');
    return m_path.substr(pos + 1);
}

bool path::is_valid() const
{
    return is_valid(m_path);
}

bool path::is_valid(mc::string_view p)
{
    // 必须以'/'开头
    if (p.empty() || p[0] != '/') {
        return false;
    }

    // 根路径是有效的
    if (p == "/") {
        return true;
    }

    // 不能以'/'结尾（除了根路径）
    if (p.back() == '/') {
        return false;
    }

    // 检查路径的每个元素
    mc::string::size_type start = 1;
    mc::string::size_type end;

    while (start < p.size()) {
        end = p.find('/', start);
        if (end == mc::string::npos) {
            end = p.size();
        }

        // 检查当前元素是否有效
        if (end == start || !is_valid_element(p.substr(start, end - start))) {
            return false;
        }

        start = end + 1;
    }

    return true;
}

bool path::is_valid_element(mc::string_view element)
{
    if (element.empty()) {
        return false;
    }

    return std::all_of(element.begin(), element.end(), [](char c) {
        return std::isalnum(c) || c == '_';
    });
}

std::ostream& operator<<(std::ostream& os, const path& p)
{
    os << p.str();
    return os;
}

} // namespace engine
} // namespace mc