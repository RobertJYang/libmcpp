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
#include <unordered_map>

namespace mc {

// 默认构造函数
dict::dict() : m_data(std::make_shared<data_t>()) {
}

// 从键值对集合构造
dict::dict(std::vector<entry> entries) : m_data(std::make_shared<data_t>(entries.empty() ? 1 : entries.size())) {
    // 处理重复的键，保留最后一个值
    for (auto&& entry_val : entries) {
        // 查找是否已存在该键
        auto it = m_data->index.find(entry_val.key);
        if (it != m_data->index.end()) {
            // 如果键已存在，替换值
            const_cast<entry&>(*it).value = entry_val.value;
        } else {
            // 如果键不存在，添加新的键值对
            entry* new_entry = new entry(std::move(entry_val.key), entry_val.value);
            m_data->entries.push_back(*new_entry);
            m_data->index.insert(*new_entry);
        }
    }
}

// 从初始化列表构造
dict::dict(std::initializer_list<std::pair<std::string, variant>> init) 
    : m_data(std::make_shared<data_t>(init.size())) {
    // 处理初始化列表中的键值对
    for (const auto& pair : init) {
        // 查找是否已存在该键
        auto it = m_data->index.find(pair.first);
        if (it != m_data->index.end()) {
            // 如果键已存在，替换值
            const_cast<entry&>(*it).value = pair.second;
        } else {
            // 如果键不存在，添加新的键值对
            entry* new_entry = new entry(pair.first, pair.second);
            m_data->entries.push_back(*new_entry);
            m_data->index.insert(*new_entry);
        }
    }
}

// 拷贝构造函数 - 共享数据
dict::dict(const dict& other) : m_data(other.m_data) {
}

// 移动构造函数
dict::dict(dict&& other) noexcept = default;

// 析构函数
dict::~dict() = default;

// 赋值运算符 - 共享数据
dict& dict::operator=(const dict& other) = default;
dict& dict::operator=(dict&& other) noexcept = default;

// 查找指定键的元素
const dict::entry* dict::find_entry(const std::string& key) const {
    auto it = m_data->index.find(key);
    if (it != m_data->index.end()) {
        return &(*it);
    }
    return nullptr;
}

// 查找指定键的元素 (string_view 版本)
const dict::entry* dict::find_entry(std::string_view key) const {
    auto it = m_data->index.find(key);
    if (it != m_data->index.end()) {
        return &(*it);
    }
    return nullptr;
}

// 查找指定键的元素 (const char* 版本)
const dict::entry* dict::find_entry(const char* key) const {
    auto it = m_data->index.find(key);
    if (it != m_data->index.end()) {
        return &(*it);
    }
    return nullptr;
}

// 获取指定键的值
const variant& dict::operator[](const std::string& key) const {
    const entry* e = find_entry(key);
    if (e) {
        return e->value;
    }
    throw std::out_of_range("字典中不存在键: " + key);
}

// 获取指定键的值 (string_view 版本)
const variant& dict::operator[](std::string_view key) const {
    const entry* e = find_entry(key);
    if (e) {
        return e->value;
    }
    throw std::out_of_range("字典中不存在键: " + std::string(key));
}

// 获取指定键的值 (const char* 版本)
const variant& dict::operator[](const char* key) const {
    const entry* e = find_entry(key);
    if (e) {
        return e->value;
    }
    throw std::out_of_range("字典中不存在键: " + std::string(key));
}

// 获取指定键的值，如果不存在则返回默认值
const variant& dict::get(const std::string& key, const variant& default_value) const {
    const entry* e = find_entry(key);
    if (e) {
        return e->value;
    }
    return default_value;
}

// 获取指定键的值，如果不存在则返回默认值 (string_view 版本)
const variant& dict::get(std::string_view key, const variant& default_value) const {
    const entry* e = find_entry(key);
    if (e) {
        return e->value;
    }
    return default_value;
}

// 获取指定键的值，如果不存在则返回默认值 (const char* 版本)
const variant& dict::get(const char* key, const variant& default_value) const {
    const entry* e = find_entry(key);
    if (e) {
        return e->value;
    }
    return default_value;
}

// 判断是否包含指定键
bool dict::contains(const std::string& key) const {
    return find_entry(key) != nullptr;
}

// 判断是否包含指定键 (string_view 版本)
bool dict::contains(std::string_view key) const {
    return find_entry(key) != nullptr;
}

// 判断是否包含指定键 (const char* 版本)
bool dict::contains(const char* key) const {
    return find_entry(key) != nullptr;
}

// 获取键值对数量
size_t dict::size() const {
    return m_data->entries.size();
}

// 判断是否为空
bool dict::empty() const {
    return m_data->entries.empty();
}

// 获取开始迭代器
dict::const_iterator dict::begin() const {
    return m_data->entries.begin();
}

// 获取结束迭代器
dict::const_iterator dict::end() const {
    return m_data->entries.end();
}

// 获取所有键
std::vector<std::string> dict::keys() const {
    std::vector<std::string> result;
    result.reserve(size());
    for (const auto& item : m_data->entries) {
        result.push_back(item.key);
    }
    return result;
}

// 获取所有值
std::vector<variant> dict::values() const {
    std::vector<variant> result;
    result.reserve(size());
    for (const auto& item : m_data->entries) {
        result.push_back(item.value);
    }
    return result;
}

// 获取指定索引位置的键值对
const dict::entry& dict::at(size_t index) const {
    if (index >= size()) {
        throw std::out_of_range("字典索引越界");
    }
    
    auto it = m_data->entries.begin();
    std::advance(it, index);
    return *it;
}

// 查找键的索引位置
int dict::find_index(const std::string& key) const {
    const entry* e = find_entry(key);
    if (!e) {
        return -1;
    }
    
    // 计算索引位置
    int index = 0;
    for (auto it = m_data->entries.begin(); it != m_data->entries.end(); ++it, ++index) {
        if (&(*it) == e) {
            return index;
        }
    }
    
    return -1; // 理论上不会到达这里
}

// 查找键的索引位置 (string_view 版本)
int dict::find_index(std::string_view key) const {
    const entry* e = find_entry(key);
    if (!e) {
        return -1;
    }
    
    // 计算索引位置
    int index = 0;
    for (auto it = m_data->entries.begin(); it != m_data->entries.end(); ++it, ++index) {
        if (&(*it) == e) {
            return index;
        }
    }
    
    return -1; // 理论上不会到达这里
}

// 查找键的索引位置 (const char* 版本)
int dict::find_index(const char* key) const {
    const entry* e = find_entry(key);
    if (!e) {
        return -1;
    }
    
    // 计算索引位置
    int index = 0;
    for (auto it = m_data->entries.begin(); it != m_data->entries.end(); ++it, ++index) {
        if (&(*it) == e) {
            return index;
        }
    }
    
    return -1; // 理论上不会到达这里
}

// 比较两个 dict 对象是否相等
bool dict::operator==(const dict& other) const {
    // 如果指向同一个数据，则一定相等
    if (m_data == other.m_data) {
        return true;
    }
    
    // 如果大小不同，则不相等
    if (size() != other.size()) {
        return false;
    }

    // 检查每个键值对是否相等
    for (const auto& item : m_data->entries) {
        // 检查键是否存在
        const entry* other_entry = other.find_entry(item.key);
        if (!other_entry) {
            return false;
        }
        // 检查值是否相等
        if (!(item.value == other_entry->value)) {
            return false;
        }
    }

    return true;
}

// 查找指定键的元素，返回迭代器 (std::string 版本)
dict::const_iterator dict::find(const std::string& key) const {
    const entry* e = find_entry(key);
    if (e) {
        // 直接从元素指针获取迭代器
        return m_data->entries.iterator_to(*const_cast<entry*>(e));
    }
    return m_data->entries.end();
}

// 查找指定键的元素，返回迭代器 (std::string_view 版本)
dict::const_iterator dict::find(std::string_view key) const {
    const entry* e = find_entry(key);
    if (e) {
        // 直接从元素指针获取迭代器
        return m_data->entries.iterator_to(*const_cast<entry*>(e));
    }
    return m_data->entries.end();
}

// 查找指定键的元素，返回迭代器 (const char* 版本)
dict::const_iterator dict::find(const char* key) const {
    const entry* e = find_entry(key);
    if (e) {
        // 直接从元素指针获取迭代器
        return m_data->entries.iterator_to(*const_cast<entry*>(e));
    }
    return m_data->entries.end();
}

// 将 dict 转换为 variant
variant to_variant(const dict& d) {
    variant result;
    to_variant(d, result);
    return result;
}

} // namespace mc