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

#include <algorithm>
#include <cctype>
#include <mc/dbus/error.h>
#include <mc/dbus/path.h>

namespace mc {
namespace dbus {

path::path() : m_path("/") {
}

path::path(std::string p) {
    if (!is_valid(p)) {
        MC_THROW(mc::invalid_arg_exception, "无效的DBus对象路径: ${path}", ("path", p));
    }
    m_path = std::move(p);
}

path::path(const char* p) : path(std::string(p)) {
}

const std::string& path::str() const {
    return m_path;
}

path::operator std::string() const {
    return m_path;
}

bool path::operator==(const path& other) const {
    return m_path == other.m_path;
}

bool path::operator!=(const path& other) const {
    return !(*this == other);
}

bool path::operator<(const path& other) const {
    return m_path < other.m_path;
}

path path::operator/(const std::string& element) const {
    if (!is_valid_element(element)) {
        MC_THROW(mc::invalid_arg_exception, "无效的DBus路径元素: ${element}", ("element", element));
    }

    std::string new_path;
    if (m_path == "/") {
        new_path = m_path + element;
    } else {
        new_path = m_path + "/" + element;
    }
    return path(new_path);
}

path& path::operator=(std::string p) {
    if (!is_valid(p)) {
        MC_THROW(mc::invalid_arg_exception, "无效的DBus对象路径: ${path}", ("path", p));
    }

    m_path = std::move(p);
    return *this;
}

path path::parent() const {
    if (m_path == "/") {
        return *this;
    }

    size_t pos = m_path.find_last_of('/');
    if (pos == 0) {
        return path("/");
    }
    return path(m_path.substr(0, pos));
}

std::string path::basename() const {
    if (m_path == "/") {
        return "";
    }

    size_t pos = m_path.find_last_of('/');
    return m_path.substr(pos + 1);
}

bool path::is_valid(const std::string& p) {
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
    std::string::size_type start = 1;
    std::string::size_type end;

    while (start < p.size()) {
        end = p.find('/', start);
        if (end == std::string::npos) {
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

bool path::is_valid_element(const std::string& element) {
    if (element.empty()) {
        return false;
    }

    return std::all_of(element.begin(), element.end(), [](char c) {
        return std::isalnum(c) || c == '_';
    });
}

std::ostream& operator<<(std::ostream& os, const path& p) {
    os << p.str();
    return os;
}

} // namespace dbus
} // namespace mc