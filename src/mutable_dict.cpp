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
 * @brief 实现 dict.h 中声明的 mutable_dict 类的方法
 */
#include <mc/dict.h>
#include <mc/variant.h>
#include <stdexcept>

namespace mc {

// 默认构造函数
mutable_dict::mutable_dict() = default;

// 单键值对构造函数
mutable_dict::mutable_dict(variant key, variant value) : dict() {
    (*this)(std::move(key), std::move(value));
}

// 从键值对集合构造
mutable_dict::mutable_dict(std::vector<entry> entries) : dict(std::move(entries)) {
}

// 从初始化列表构造
mutable_dict::mutable_dict(std::initializer_list<std::pair<variant, variant>> init) : dict(init) {
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
mutable_dict& mutable_dict::operator=(const mutable_dict& other)     = default;
mutable_dict& mutable_dict::operator=(mutable_dict&& other) noexcept = default;

// 从 dict 赋值
mutable_dict& mutable_dict::operator=(const dict& other) {
    dict::operator=(other);
    return *this;
}

// 从初始化列表赋值
mutable_dict& mutable_dict::operator=(std::initializer_list<std::pair<variant, variant>> init) {
    // 创建一个新的mutable_dict并用初始化列表构造
    mutable_dict new_dict(init);
    // 交换内部数据
    *this = std::move(new_dict);
    return *this;
}

// 查找指定键的元素（可变版本）
dict::entry* mutable_dict::find_entry(const std::string& key) {
    return find_entry(std::string_view(key));
}

dict::entry* mutable_dict::find_entry(std::string_view key) {
    if (!m_data) {
        return nullptr;
    }

    auto it = m_data->index.find(key);
    return it == m_data->index.end() ? nullptr : const_cast<entry*>(&*it);
}

dict::entry* mutable_dict::find_entry(const char* key) {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return find_entry(std::string_view(key));
}

dict::entry* mutable_dict::find_entry(const variant& key) {
    if (!m_data) {
        return nullptr;
    }

    auto it = m_data->index.find(key);
    return it == m_data->index.end() ? nullptr : const_cast<entry*>(&*it);
}

mutable_dict& mutable_dict::operator()(variant key, variant value) {
    entry* e = find_entry(key);
    if (e) {
        e->value = std::move(value);
    } else {
        entry* new_entry = new entry(std::move(key), std::move(value));
        m_data->entries.push_back(*new_entry);
        m_data->index.insert(*new_entry);
    }
    return *this;
}

// 获取或设置指定键的值
variant& mutable_dict::operator[](const std::string& key) {
    return (*this)[std::string_view(key)];
}

// 获取或设置指定键的值（string_view 版本）
variant& mutable_dict::operator[](std::string_view key) {
    entry* e = find_entry(key);
    if (e) {
        return e->value;
    }

    entry* new_entry = new entry(std::string(key), variant());
    m_data->entries.push_back(*new_entry);
    m_data->index.insert(*new_entry);
    return new_entry->value;
}

// 获取或设置指定键的值（const char* 版本）
variant& mutable_dict::operator[](const char* key) {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return (*this)[std::string_view(key)];
}

variant& mutable_dict::operator[](const variant& key) {
    if (key.is_null()) {
        throw std::invalid_argument("键不能为空指针");
    }

    entry* e = find_entry(key);
    if (e) {
        return e->value;
    }

    entry* new_entry = new entry(key, variant());
    m_data->entries.push_back(*new_entry);
    m_data->index.insert(*new_entry);
    return new_entry->value;
}

// 获取指定键的值（const 版本）
const variant& mutable_dict::operator[](const std::string& key) const {
    return dict::operator[](key);
}

// 获取指定键的值（const 版本，string_view 版本）
const variant& mutable_dict::operator[](std::string_view key) const {
    return dict::operator[](key);
}

// 获取指定键的值（const 版本，const char* 版本）
const variant& mutable_dict::operator[](const char* key) const {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return dict::operator[](key);
}

const variant& mutable_dict::operator[](const variant& key) const {
    return dict::operator[](key);
}

// 移除指定键的键值对
bool mutable_dict::erase(const std::string& key) {
    return erase(std::string_view(key));
}

// 移除指定键的键值对（string_view 版本）
bool mutable_dict::erase(std::string_view key) {
    entry* e = find_entry(key);
    if (e) {
        m_data->index.erase(m_data->index.iterator_to(*e));
        m_data->entries.erase(m_data->entries.iterator_to(*e));
        delete e;
        return true;
    }
    return false;
}

// 移除指定键的键值对（const char* 版本）
bool mutable_dict::erase(const char* key) {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return erase(std::string_view(key));
}

bool mutable_dict::erase(const variant& key) {
    entry* e = find_entry(key);
    if (e) {
        m_data->index.erase(m_data->index.iterator_to(*e));
        m_data->entries.erase(m_data->entries.iterator_to(*e));
        delete e;
        return true;
    }
    return false;
}

// 清空所有键值对
void mutable_dict::clear() {
    // 先清空索引，这样钩子就不再链接到容器中
    m_data->index.clear();
    // 然后清空链表并释放内存
    m_data->entries.clear_and_dispose([](entry* p) {
        delete p;
    });
}

// 获取开始迭代器
mutable_dict::iterator mutable_dict::begin() {
    return m_data->entries.begin();
}

// 获取开始迭代器（const 版本）
mutable_dict::const_iterator mutable_dict::begin() const {
    return m_data->entries.begin();
}

// 获取结束迭代器
mutable_dict::iterator mutable_dict::end() {
    return m_data->entries.end();
}

// 获取结束迭代器（const 版本）
mutable_dict::const_iterator mutable_dict::end() const {
    return m_data->entries.end();
}

// 获取指定索引位置的键值对（可变）
mutable_dict::entry& mutable_dict::at_index(size_t index) {
    if (index >= size()) {
        throw std::out_of_range("字典索引越界");
    }

    auto it = m_data->entries.begin();
    std::advance(it, index);
    return *it;
}

// 获取指定键的值（可变）
variant& mutable_dict::at(const std::string& key) {
    return at(std::string_view(key));
}

variant& mutable_dict::at(std::string_view key) {
    entry* e = find_entry(key);
    if (!e) {
        throw std::out_of_range("字典中不存在键: " + std::string(key));
    }
    return e->value;
}

variant& mutable_dict::at(const char* key) {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return at(std::string_view(key));
}

variant& mutable_dict::at(const variant& key) {
    entry* e = find_entry(key);
    if (!e) {
        throw std::out_of_range("字典中不存在键: " + key.to_string());
    }
    return e->value;
}

// 查找指定键的元素，返回迭代器 (std::string 版本)
mutable_dict::iterator mutable_dict::find(const std::string& key) {
    return find(std::string_view(key));
}

// 查找指定键的元素，返回迭代器 (std::string_view 版本)
mutable_dict::iterator mutable_dict::find(std::string_view key) {
    entry* e = find_entry(key);
    if (e) {
        // 直接从元素指针获取迭代器
        return m_data->entries.iterator_to(*e);
    }
    return m_data->entries.end();
}

// 查找指定键的元素，返回迭代器 (const char* 版本)
mutable_dict::iterator mutable_dict::find(const char* key) {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return find(std::string_view(key));
}

mutable_dict::iterator mutable_dict::find(const variant& key) {
    entry* e = find_entry(key);
    if (e) {
        return m_data->entries.iterator_to(*e);
    }
    return m_data->entries.end();
}

// 查找指定键的元素，返回迭代器 (std::string 版本，const)
mutable_dict::const_iterator mutable_dict::find(const std::string& key) const {
    return dict::find(key);
}

// 查找指定键的元素，返回迭代器 (std::string_view 版本，const)
mutable_dict::const_iterator mutable_dict::find(std::string_view key) const {
    return dict::find(key);
}

// 查找指定键的元素，返回迭代器 (const char* 版本，const)
mutable_dict::const_iterator mutable_dict::find(const char* key) const {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return dict::find(key);
}

mutable_dict::const_iterator mutable_dict::find(const variant& key) const {
    return dict::find(key);
}

} // namespace mc