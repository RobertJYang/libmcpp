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
 * @file mutable_dict.cpp
 * @brief 实现 mutable_dict.h 中声明的方法
 */
#include <mc/mutable_dict.h>
#include <mc/variant.h>
#include <stdexcept>

namespace mc {

// 默认构造函数
mutable_dict::mutable_dict() = default;

// 从键值对集合构造
mutable_dict::mutable_dict(std::vector<entry> entries) : dict(std::move(entries)) {
}

// 从 dict 构造
mutable_dict::mutable_dict(const dict& other) : dict(other) {
}

// 拷贝构造函数
mutable_dict::mutable_dict(const mutable_dict& other) = default;

// 移动构造函数
mutable_dict::mutable_dict(mutable_dict&& other) noexcept = default;

// 析构函数
mutable_dict::~mutable_dict() = default;

// 赋值运算符
mutable_dict& mutable_dict::operator=(const mutable_dict& other) = default;
mutable_dict& mutable_dict::operator=(mutable_dict&& other) noexcept = default;

// 从 dict 赋值
mutable_dict& mutable_dict::operator=(const dict& other) {
    dict::operator=(other);
    return *this;
}

// 添加或更新键值对
mutable_dict& mutable_dict::operator()(const std::string& key, variant value) {
    // 查找是否已存在该键
    for (auto& item : m_items) {
        if (item.key == key) {
            // 更新值
            item.value = std::move(value);
            return *this;
        }
    }

    // 不存在则添加新的键值对
    m_items.push_back({key, variant(std::move(value))});
    return *this;
}

// 获取或设置指定键的值
variant& mutable_dict::operator[](const std::string& key) {
    // 查找是否已存在该键
    for (auto& item : m_items) {
        if (item.key == key) {
            return item.value;
        }
    }

    // 不存在则添加新的键值对
    m_items.push_back({key, variant()});
    return m_items.back().value;
}

// 获取指定键的值（const 版本）
const variant& mutable_dict::operator[](const std::string& key) const {
    return dict::operator[](key);
}

// 移除指定键的键值对
bool mutable_dict::erase(const std::string& key) {
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it->key == key) {
            m_items.erase(it);
            return true;
        }
    }
    return false;
}

// 清空所有键值对
void mutable_dict::clear() {
    m_items.clear();
}

// 获取开始迭代器
mutable_dict::iterator mutable_dict::begin() {
    return m_items.begin();
}

// 获取开始迭代器（const 版本）
mutable_dict::const_iterator mutable_dict::begin() const {
    return m_items.begin();
}

// 获取结束迭代器
mutable_dict::iterator mutable_dict::end() {
    return m_items.end();
}

// 获取结束迭代器（const 版本）
mutable_dict::const_iterator mutable_dict::end() const {
    return m_items.end();
}

// 获取指定索引位置的键值对
mutable_dict::entry& mutable_dict::at(size_t index) {
    if (index >= m_items.size()) {
        throw std::out_of_range("字典索引越界");
    }
    return m_items[index];
}

// 获取指定索引位置的键值对（const 版本）
const mutable_dict::entry& mutable_dict::at(size_t index) const {
    return dict::at(index);
}

} // namespace mc