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
#include <mc/dict/entry.h>
#include <mc/json.h>
#include <mc/variant.h>
#include <stdexcept>
#include <unordered_map>

namespace mc {

// 从键值对集合构造
dict::dict(const std::vector<entry>& entries)
    : m_data(mc::make_shared<data_t>(entries.empty() ? 1 : entries.size())) {
    for (auto&& entry_val : entries) {
        auto it = m_data->index.find(entry_val.key);
        if (it != m_data->index.end()) {
            const_cast<entry&>(*it).value = entry_val.value;
        } else {
            entry* new_entry = new entry(std::move(entry_val.key), entry_val.value);
            m_data->entries.push_back(*new_entry);
            m_data->index.insert(*new_entry);
        }
    }
}

// 从初始化列表构造
dict::dict(std::initializer_list<std::pair<variant, variant>> init)
    : m_data(mc::make_shared<data_t>(init.size())) {
    for (const auto& pair : init) {
        auto it = m_data->index.find(pair.first);
        if (it != m_data->index.end()) {
            const_cast<entry&>(*it).value = pair.second;
        } else {
            entry* new_entry = new entry(pair.first, pair.second);
            m_data->entries.push_back(*new_entry);
            m_data->index.insert(*new_entry);
        }
    }
}

// 查找指定键的元素
const dict::entry* dict::find_entry(const std::string& key) const {
    return find_entry(std::string_view(key));
}

// 查找指定键的元素 (string_view 版本)
const dict::entry* dict::find_entry(std::string_view key) const {
    if (!m_data) {
        return nullptr;
    }

    auto it = m_data->index.find(key);
    if (it != m_data->index.end()) {
        return &(*it);
    }
    return nullptr;
}

// 查找指定键的元素 (const char* 版本)
const dict::entry* dict::find_entry(const char* key) const {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return find_entry(std::string_view(key));
}

const dict::entry* dict::find_entry(const variant& key) const {
    if (!m_data) {
        return nullptr;
    }

    auto it = m_data->index.find(key);
    if (it != m_data->index.end()) {
        return &(*it);
    }
    return nullptr;
}

// 获取指定键的值
const variant& dict::operator[](const std::string& key) const {
    return (*this)[std::string_view(key)];
}

// 获取指定键的值 (string_view 版本)
const variant& dict::operator[](std::string_view key) const {
    const auto* e = find_entry(key);
    if (e) {
        return e->value;
    }
    throw std::out_of_range("字典中不存在键: " + std::string(key));
}

// 获取指定键的值 (const char* 版本)
const variant& dict::operator[](const char* key) const {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return (*this)[std::string_view(key)];
}

const variant& dict::operator[](const variant& key) const {
    const auto* e = find_entry(key);
    if (e) {
        return e->value;
    }
    throw std::out_of_range("字典中不存在键: " + key.to_string());
}

// 获取指定键的值，如果不存在则返回默认值
const variant& dict::get(const std::string& key, const variant& default_value) const {
    return get(std::string_view(key), default_value);
}

// 获取指定键的值，如果不存在则返回默认值 (string_view 版本)
const variant& dict::get(std::string_view key, const variant& default_value) const {
    const auto* e = find_entry(key);
    if (e) {
        return e->value;
    }
    return default_value;
}

// 获取指定键的值，如果不存在则返回默认值 (const char* 版本)
const variant& dict::get(const char* key, const variant& default_value) const {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return get(std::string_view(key), default_value);
}

const variant& dict::get(const variant& key, const variant& default_value) const {
    const auto* e = find_entry(key);
    if (e) {
        return e->value;
    }
    return default_value;
}

// 判断是否包含指定键
bool dict::contains(const std::string& key) const {
    return contains(std::string_view(key));
}

// 判断是否包含指定键 (string_view 版本)
bool dict::contains(std::string_view key) const {
    return find_entry(key) != nullptr;
}

// 判断是否包含指定键 (const char* 版本)
bool dict::contains(const char* key) const {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return contains(std::string_view(key));
}

bool dict::contains(const variant& key) const {
    return find_entry(key) != nullptr;
}

// 获取键值对数量
size_t dict::size() const {
    if (!m_data) {
        return 0;
    }

    return m_data->entries.size();
}

// 判断是否为空
bool dict::empty() const {
    if (!m_data) {
        return true;
    }

    return m_data->entries.empty();
}

// 获取开始迭代器
dict::const_iterator dict::begin() const {
    if (!m_data) {
        return {};
    }

    return m_data->entries.begin();
}

// 获取结束迭代器
dict::const_iterator dict::end() const {
    if (!m_data) {
        return {};
    }

    return m_data->entries.end();
}

// 获取所有键
std::vector<variant> dict::keys() const {
    if (!m_data) {
        return {};
    }

    std::vector<variant> result;
    result.reserve(size());
    for (const auto& item : m_data->entries) {
        result.push_back(item.key);
    }
    return result;
}

// 获取所有值
std::vector<variant> dict::values() const {
    if (!m_data) {
        return {};
    }

    std::vector<variant> result;
    result.reserve(size());
    for (const auto& item : m_data->entries) {
        result.push_back(item.value);
    }
    return result;
}

// 获取指定索引位置的键值对
const dict::entry& dict::at_index(size_t index) const {
    if (!m_data || index >= size()) {
        throw std::out_of_range("字典索引越界");
    }

    auto it = m_data->entries.begin();
    std::advance(it, index);
    return *it;
}

// 获取指定键的值
const variant& dict::at(const std::string& key) const {
    return at(std::string_view(key));
}

const variant& dict::at(std::string_view key) const {
    const auto* e = find_entry(key);
    if (!e) {
        throw std::out_of_range("字典中不存在键: " + std::string(key));
    }
    return e->value;
}

const variant& dict::at(const char* key) const {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return at(std::string_view(key));
}

const variant& dict::at(const variant& key) const {
    const auto* e = find_entry(key);
    if (!e) {
        throw std::out_of_range("字典中不存在键: " + key.to_string());
    }
    return e->value;
}

// 计算指定元素在列表中的索引位置
int dict::find_entry_index(const entry* e) const {
    if (!e || !m_data) {
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

// 查找键的索引位置
int dict::find_index(const std::string& key) const {
    return find_entry_index(find_entry(key));
}

// 查找键的索引位置 (string_view 版本)
int dict::find_index(std::string_view key) const {
    return find_entry_index(find_entry(key));
}

// 查找键的索引位置 (const char* 版本)
int dict::find_index(const char* key) const {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return find_entry_index(find_entry(key));
}

int dict::find_index(const variant& key) const {
    return find_entry_index(find_entry(key));
}

// 比较两个 dict 对象是否相等
bool dict::operator==(const dict& other) const {
    // 如果指向同一个数据，则一定相等
    if (m_data == other.m_data) {
        return true;
    }

    // 如果不是指向同一个数据，那任意一个为空则一定不相等
    if (!m_data || !other.m_data) {
        return false;
    }

    // 如果大小不同，则不相等
    if (size() != other.size()) {
        return false;
    }

    // 检查每个键值对是否相等
    for (const auto& item : m_data->entries) {
        // 检查键是否存在
        const auto* other_entry = other.find_entry(item.key);
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

dict dict::operator+(const dict& other) const {
    mutable_dict result;

    for (const auto& item : m_data->entries) {
        result[item.key] = item.value;
    }

    for (const auto& item : other.m_data->entries) {
        result[item.key] = item.value;
    }

    return dict(result);
}

mutable_dict dict::as_mut() const {
    return mutable_dict(*this);
}

void dict::clear() {
    if (!m_data) {
        return;
    }

    // 先清空索引，这样钩子就不再链接到容器中
    m_data->index.clear();
    // 然后清空链表并释放内存
    m_data->entries.clear_and_dispose([](dict_types::entry* p) {
        delete p;
    });
}

// 查找指定键的元素，返回迭代器 (std::string 版本)
dict::const_iterator dict::find(const std::string& key) const {
    return find(std::string_view(key));
}

// 查找指定键的元素，返回迭代器 (std::string_view 版本)
dict::const_iterator dict::find(std::string_view key) const {
    const auto* e = find_entry(key);
    if (!e) {
        return end();
    }

    return m_data->entries.iterator_to(*const_cast<entry*>(e));
}

// 查找指定键的元素，返回迭代器 (const char* 版本)
dict::const_iterator dict::find(const char* key) const {
    if (key == nullptr) {
        throw std::invalid_argument("键不能为空指针");
    }
    return find(std::string_view(key));
}

dict::const_iterator dict::find(const variant& key) const {
    const auto* e = find_entry(key);
    if (!e) {
        return end();
    }
    return m_data->entries.iterator_to(*const_cast<entry*>(e));
}

// 将 dict 转换为 variant
variant to_variant(const dict& d) {
    variant result;
    to_variant(d, result);
    return result;
}

size_t dict::hash() const {
    if (empty()) {
        return 0;
    }

    // 使用黄金比例作为种子
    size_t       h    = 0x9e3779b9 ^ size();
    const size_t step = (size() >> 5) + 1;
    for (size_t l1 = size(); l1 >= step; l1 -= step) {
        const auto& e          = at_index(l1 - 1);
        size_t      entry_hash = e.key.hash() ^ e.value.hash();
        h                      = h ^ ((h << 5) + (h >> 2) + entry_hash);
    }

    return h;
}

std::string dict::to_string() const {
    return json::json_encode(*this);
}

} // namespace mc