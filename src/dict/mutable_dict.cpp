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

#include <mc/dict.h>
#include <mc/variant.h>
#include <stdexcept>

namespace mc {

// dict 的可变操作实现

dict::dict(variant key, variant value)
    : dict()
{
    (*this)(std::move(key), std::move(value));
}

dict& dict::operator=(std::initializer_list<std::pair<variant, variant>> init)
{
    dict new_dict(init);
    *this = std::move(new_dict);
    return *this;
}

dict::entry* dict::find_entry(const std::string& key)
{
    return find_entry(std::string_view(key));
}

dict::entry* dict::find_entry(std::string_view key)
{
    if (!m_data) {
        return nullptr;
    }

    auto it = m_data->index.find(key, m_data->index.hash_function(), m_data->index.key_eq());
    return it == m_data->index.end() ? nullptr : const_cast<entry*>(&*it);
}

dict::entry* dict::find_entry(const char* key)
{
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return find_entry(std::string_view(key));
}

dict::entry* dict::find_entry(const variant& key)
{
    if (!m_data) {
        return nullptr;
    }

    auto it = m_data->index.find(key, m_data->index.hash_function(), m_data->index.key_eq());
    return it == m_data->index.end() ? nullptr : const_cast<entry*>(&*it);
}

dict& dict::operator()(variant key, variant value)
{
    auto* e = find_entry(key);
    if (e) {
        e->value = std::move(value);
    } else {
        auto&  data      = ensure_data();
        entry* new_entry = new entry(std::move(key), std::move(value));
        data.entries.push_back(*new_entry);
        data.index.insert(*new_entry);
    }
    return *this;
}

variant& dict::operator[](const std::string& key)
{
    return (*this)[std::string_view(key)];
}

variant& dict::operator[](std::string_view key)
{
    auto* e = find_entry(key);
    if (e) {
        return e->value;
    }

    auto&  data      = ensure_data();
    entry* new_entry = new entry(std::string(key), variant());
    data.entries.push_back(*new_entry);
    data.index.insert(*new_entry);
    return new_entry->value;
}

variant& dict::operator[](const char* key)
{
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return (*this)[std::string_view(key)];
}

variant& dict::operator[](const variant& key)
{
    if (key.is_null()) {
        throw std::invalid_argument("键不能为空指针");
    }

    auto* e = find_entry(key);
    if (e) {
        return e->value;
    }

    auto&  data      = ensure_data();
    entry* new_entry = new entry(key, variant());
    data.entries.push_back(*new_entry);
    data.index.insert(*new_entry);
    return new_entry->value;
}

// 支持整数键的 operator[]（用于数组类型 dict）
variant& dict::operator[](int key)
{
    return (*this)[static_cast<int64_t>(key)];
}

variant& dict::operator[](int64_t key)
{
    return (*this)[mc::variant(key)];
}

bool dict::erase(const std::string& key)
{
    return erase(std::string_view(key));
}

bool dict::erase(std::string_view key)
{
    auto* e = find_entry(key);
    if (e) {
        m_data->index.erase(m_data->index.iterator_to(*e));
        m_data->entries.erase(m_data->entries.iterator_to(*e));
        delete e;
        return true;
    }
    return false;
}

bool dict::erase(const char* key)
{
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return erase(std::string_view(key));
}

bool dict::erase(const variant& key)
{
    auto* e = find_entry(key);
    if (e) {
        m_data->index.erase(m_data->index.iterator_to(*e));
        m_data->entries.erase(m_data->entries.iterator_to(*e));
        delete e;
        return true;
    }
    return false;
}

dict::iterator dict::begin()
{
    if (!m_data) {
        return {};
    }

    return m_data->entries.begin();
}

dict::iterator dict::end()
{
    if (!m_data) {
        return {};
    }

    return m_data->entries.end();
}

dict_types::entry& dict::at_index(size_t index)
{
    if (index >= size()) {
        throw std::out_of_range("字典索引越界");
    }

    auto it = m_data->entries.begin();
    std::advance(it, index);
    return *it;
}

variant& dict::at(const std::string& key)
{
    return at(std::string_view(key));
}

variant& dict::at(std::string_view key)
{
    auto* e = find_entry(key);
    if (!e) {
        throw std::out_of_range("字典中不存在键: " + std::string(key));
    }
    return e->value;
}

variant& dict::at(const char* key)
{
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return at(std::string_view(key));
}

variant& dict::at(const variant& key)
{
    auto* e = find_entry(key);
    if (!e) {
        throw std::out_of_range("字典中不存在键: " + key.to_string());
    }
    return e->value;
}

dict::iterator dict::find(const std::string& key)
{
    return find(std::string_view(key));
}

dict::iterator dict::find(std::string_view key)
{
    if (!m_data) {
        return end();
    }

    auto* e = find_entry(key);
    if (e) {
        return m_data->entries.iterator_to(*e);
    }
    return m_data->entries.end();
}

dict::iterator dict::find(const char* key)
{
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return find(std::string_view(key));
}

dict::iterator dict::find(const variant& key)
{
    auto* e = find_entry(key);
    if (!e) {
        return end();
    }

    return m_data->entries.iterator_to(*e);
}

dict::data_t& dict::ensure_data() const
{
    if (!m_data) {
        m_data = mc::make_shared<dict::data_t>();
    }

    return *m_data;
}

dict& dict::operator+=(const dict& other)
{
    for (const auto& item : other.m_data->entries) {
        (*this)(item.key, item.value);
    }
    return *this;
}

dict& dict::insert(dict_types::entry e)
{
    auto&  data      = ensure_data();
    entry* new_entry = new entry(std::move(e.key), std::move(e.value));
    data.entries.push_back(*new_entry);
    data.index.insert(*new_entry);
    return *this;
}

std::pair<dict::iterator, bool> dict::insert(variant key, variant value)
{
    auto* e = find_entry(key);
    if (e) {
        return {m_data->entries.iterator_to(*e), false};
    }

    auto&  data      = ensure_data();
    entry* new_entry = new entry(std::move(key), std::move(value));
    data.entries.push_back(*new_entry);
    data.index.insert(*new_entry);
    return {data.entries.iterator_to(*new_entry), true};
}

dict::iterator dict::insert(const_iterator hint, variant key, variant value)
{
    auto* e = find_entry(key);
    if (e) {
        return m_data->entries.iterator_to(*e);
    }

    auto&  data      = ensure_data();
    entry* new_entry = new entry(std::move(key), std::move(value));

    if (hint != data.entries.end()) {
        auto* hint_entry = const_cast<dict_types::entry*>(&*hint);
        data.entries.insert(data.entries.iterator_to(*hint_entry), *new_entry);
    } else {
        data.entries.push_back(*new_entry);
    }

    data.index.insert(*new_entry);
    return data.entries.iterator_to(*new_entry);
}

void dict::insert(std::initializer_list<entry> ilist)
{
    for (const auto& e : ilist) {
        insert(e);
    }
}

} // namespace mc