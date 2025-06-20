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

mutable_dict::mutable_dict(variant key, variant value) : dict() {
    (*this)(std::move(key), std::move(value));
}

mutable_dict::mutable_dict(std::vector<entry> entries) : dict(std::move(entries)) {
}

mutable_dict::mutable_dict(std::initializer_list<std::pair<variant, variant>> init) : dict(init) {
}

mutable_dict::mutable_dict(const dict& other) : dict(other) {
}

mutable_dict& mutable_dict::operator=(const dict& other) {
    dict::operator=(other);
    return *this;
}

mutable_dict& mutable_dict::operator=(std::initializer_list<std::pair<variant, variant>> init) {
    mutable_dict new_dict(init);
    *this = std::move(new_dict);
    return *this;
}

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
        auto&  data      = ensure_data();
        entry* new_entry = new entry(std::move(key), std::move(value));
        data.entries.push_back(*new_entry);
        data.index.insert(*new_entry);
    }
    return *this;
}

variant& mutable_dict::operator[](const std::string& key) {
    return (*this)[std::string_view(key)];
}

variant& mutable_dict::operator[](std::string_view key) {
    entry* e = find_entry(key);
    if (e) {
        return e->value;
    }

    auto&  data      = ensure_data();
    entry* new_entry = new entry(std::string(key), variant());
    data.entries.push_back(*new_entry);
    data.index.insert(*new_entry);
    return new_entry->value;
}

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

    auto&  data      = ensure_data();
    entry* new_entry = new entry(key, variant());
    data.entries.push_back(*new_entry);
    data.index.insert(*new_entry);
    return new_entry->value;
}

const variant& mutable_dict::operator[](const std::string& key) const {
    return dict::operator[](key);
}

const variant& mutable_dict::operator[](std::string_view key) const {
    return dict::operator[](key);
}

const variant& mutable_dict::operator[](const char* key) const {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return dict::operator[](key);
}

const variant& mutable_dict::operator[](const variant& key) const {
    return dict::operator[](key);
}

bool mutable_dict::erase(const std::string& key) {
    return erase(std::string_view(key));
}

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

void mutable_dict::clear() {
    if (!m_data) {
        return;
    }

    // 先清空索引，这样钩子就不再链接到容器中
    m_data->index.clear();
    // 然后清空链表并释放内存
    m_data->entries.clear_and_dispose([](entry* p) {
        delete p;
    });
}

mutable_dict::iterator mutable_dict::begin() {
    if (!m_data) {
        return {};
    }

    return m_data->entries.begin();
}

mutable_dict::const_iterator mutable_dict::begin() const {
    if (!m_data) {
        return {};
    }

    return m_data->entries.begin();
}

mutable_dict::iterator mutable_dict::end() {
    if (!m_data) {
        return {};
    }

    return m_data->entries.end();
}

mutable_dict::const_iterator mutable_dict::end() const {
    if (!m_data) {
        return {};
    }

    return m_data->entries.end();
}

mutable_dict::entry& mutable_dict::at_index(size_t index) {
    if (index >= size()) {
        throw std::out_of_range("字典索引越界");
    }

    auto it = m_data->entries.begin();
    std::advance(it, index);
    return *it;
}

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

mutable_dict::iterator mutable_dict::find(const std::string& key) {
    return find(std::string_view(key));
}

mutable_dict::iterator mutable_dict::find(std::string_view key) {
    if (!m_data) {
        return end();
    }

    entry* e = find_entry(key);
    if (e) {
        return m_data->entries.iterator_to(*e);
    }
    return m_data->entries.end();
}

mutable_dict::iterator mutable_dict::find(const char* key) {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return find(std::string_view(key));
}

mutable_dict::iterator mutable_dict::find(const variant& key) {
    entry* e = find_entry(key);
    if (!e) {
        return end();
    }

    return m_data->entries.iterator_to(*e);
}

mutable_dict::const_iterator mutable_dict::find(const std::string& key) const {
    return dict::find(key);
}

mutable_dict::const_iterator mutable_dict::find(std::string_view key) const {
    return dict::find(key);
}

mutable_dict::const_iterator mutable_dict::find(const char* key) const {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return dict::find(key);
}

mutable_dict::const_iterator mutable_dict::find(const variant& key) const {
    return dict::find(key);
}

dict::data_t& mutable_dict::ensure_data() const {
    if (!m_data) {
        m_data = mc::make_shared<dict::data_t>();
    }

    return *m_data;
}

mutable_dict& mutable_dict::operator+=(const mutable_dict& other) {
    for (const auto& item : other.m_data->entries) {
        (*this)(item.key, item.value);
    }
    return *this;
}

mutable_dict& mutable_dict::insert(entry e) {
    auto&  data      = ensure_data();
    entry* new_entry = new entry(std::move(e.key), std::move(e.value));
    data.entries.push_back(*new_entry);
    data.index.insert(*new_entry);
    return *this;
}

std::pair<mutable_dict::iterator, bool> mutable_dict::insert(variant key, variant value) {
    entry* e = find_entry(key);
    if (e) {
        return {m_data->entries.iterator_to(*e), false};
    }

    auto&  data      = ensure_data();
    entry* new_entry = new entry(std::move(key), std::move(value));
    data.entries.push_back(*new_entry);
    data.index.insert(*new_entry);
    return {data.entries.iterator_to(*new_entry), true};
}

mutable_dict::iterator mutable_dict::insert(const_iterator hint, variant key, variant value) {
    entry* e = find_entry(key);
    if (e) {
        return m_data->entries.iterator_to(*e);
    }

    auto&  data      = ensure_data();
    entry* new_entry = new entry(std::move(key), std::move(value));

    if (hint != data.entries.end()) {
        entry* hint_entry = const_cast<entry*>(&*hint);
        data.entries.insert(data.entries.iterator_to(*hint_entry), *new_entry);
    } else {
        data.entries.push_back(*new_entry);
    }

    data.index.insert(*new_entry);
    return data.entries.iterator_to(*new_entry);
}

} // namespace mc