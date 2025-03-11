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

/**
 * @file dict.cpp
 * @brief 实现 dict.h 中声明的方法
 */
#include <mc/dict.h>
#include <mc/variant.h>
#include <stdexcept>

namespace mc {

// 默认构造函数
dict::dict() = default;

// 从键值对集合构造
dict::dict(std::vector<entry> entries) : m_items(std::move(entries)) {
    // 检查是否有重复的键
    for (size_t i = 0; i < m_items.size(); ++i) {
        for (size_t j = i + 1; j < m_items.size(); ++j) {
            if (m_items[i].key == m_items[j].key) {
                throw std::invalid_argument("字典中存在重复的键: " + m_items[i].key);
            }
        }
    }
}

// 拷贝构造函数
dict::dict(const dict& other) = default;

// 移动构造函数
dict::dict(dict&& other) noexcept = default;

// 析构函数
dict::~dict() = default;

// 赋值运算符
dict& dict::operator=(const dict& other) = default;
dict& dict::operator=(dict&& other) noexcept = default;

// 获取指定键的值
const variant& dict::operator[](const std::string& key) const {
    for (const auto& item : m_items) {
        if (item.key == key) {
            return item.value;
        }
    }
    throw std::out_of_range("字典中不存在键: " + key);
}

// 获取指定键的值，如果不存在则返回默认值
const variant& dict::get(const std::string& key, const variant& default_value) const {
    for (const auto& item : m_items) {
        if (item.key == key) {
            return item.value;
        }
    }
    return default_value;
}

// 判断是否包含指定键
bool dict::contains(const std::string& key) const {
    for (const auto& item : m_items) {
        if (item.key == key) {
            return true;
        }
    }
    return false;
}

// 获取键值对数量
size_t dict::size() const {
    return m_items.size();
}

// 判断是否为空
bool dict::empty() const {
    return m_items.empty();
}

// 获取开始迭代器
dict::const_iterator dict::begin() const {
    return m_items.begin();
}

// 获取结束迭代器
dict::const_iterator dict::end() const {
    return m_items.end();
}

// 获取所有键
std::vector<std::string> dict::keys() const {
    std::vector<std::string> result;
    result.reserve(m_items.size());
    for (const auto& item : m_items) {
        result.push_back(item.key);
    }
    return result;
}

// 获取所有值
std::vector<variant> dict::values() const {
    std::vector<variant> result;
    result.reserve(m_items.size());
    for (const auto& item : m_items) {
        result.push_back(item.value);
    }
    return result;
}

// 获取指定索引位置的键值对
const dict::entry& dict::at(size_t index) const {
    if (index >= m_items.size()) {
        throw std::out_of_range("字典索引越界");
    }
    return m_items[index];
}

// 查找键的索引位置
int dict::find_index(const std::string& key) const {
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].key == key) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// 比较两个 dict 对象是否相等
bool dict::operator==(const dict& other) const {
    // 如果大小不同，则不相等
    if (m_items.size() != other.m_items.size()) {
        return false;
    }

    // 检查每个键值对是否相等
    for (const auto& item : m_items) {
        // 检查键是否存在
        if (!other.contains(item.key)) {
            return false;
        }
        // 检查值是否相等
        if (!(item.value == other[item.key])) {
            return false;
        }
    }

    return true;
}

} // namespace mc