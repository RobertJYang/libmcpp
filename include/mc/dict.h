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
 * @file dict.h
 * @brief 定义了字典类，包括不可变的 dict 和可变的 mutable_dict，用于表示键值对集合，保持插入顺序
 */
#ifndef MC_DICT_H
#define MC_DICT_H

#include "variant.h"
#include <string>
#include <string_view>
#include <vector>

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

#include <mc/intrusive/intrusive.h>

namespace mc {

/**
 * @brief 不可变的字典类，保持键值对的插入顺序
 *
 * @note 此类使用共享数据模型，拷贝操作会共享内部数据。
 *       多个 dict 对象可能共享相同的内部数据，
 *       通过 mutable_dict 修改数据会影响所有共享该数据的对象。
 *
 * @note
 * 此类使用侵入式容器实现，有序索引部分用的是list，通过键查找元素的性能较好，但随机索引访问（如
 * at_index(index)） 的性能较慢，时间复杂度为 O(n)。如果需要顺序访问所有元素，强烈建议使用迭代器
 *       （begin()/end()）而不是索引，以获得更好的性能。例如：
 *
 *       // 低效的遍历方式（不推荐）
 *       for (size_t i = 0; i < dict.size(); ++i) {
 *           const auto& entry = dict.at_index(i);
 *           // 处理 entry...
 *       }
 *
 *       // 高效的遍历方式（推荐）
 *       for (const auto& entry : dict) {
 *           // 处理 entry...
 *       }
 */
class dict {
public:
    /**
     * @brief 表示键值对的结构体，包含侵入式链表和哈希表的钩子
     */
    struct entry : public mc::intrusive::list_hook, public mc::intrusive::unordered_set_hook {
        std::string key;
        variant     value;

        // 默认构造函数
        entry() = default;

        // 带参数的构造函数
        entry(std::string k, variant v) : key(std::move(k)), value(std::move(v)) {
        }

        // 模板构造函数，允许直接使用任意类型 T 作为值参数
        template <typename T>
        entry(std::string k, T&& v) : key(std::move(k)), value(std::forward<T>(v)) {
        }

        // 模板构造函数，允许使用不同类型的键
        template <typename K, typename T>
        entry(K&& k, T&& v) : key(std::forward<K>(k)), value(std::forward<T>(v)) {
        }

        // 拷贝构造函数 - 不拷贝钩子状态
        entry(const entry& other)
            : mc::intrusive::list_hook(), mc::intrusive::unordered_set_hook(), key(other.key),
              value(other.value) {
        }

        // 移动构造函数 - 不移动钩子状态
        entry(entry&& other) noexcept
            : mc::intrusive::list_hook(), mc::intrusive::unordered_set_hook(),
              key(std::move(other.key)), value(std::move(other.value)) {
        }

        // 禁止赋值操作，因为钩子不支持赋值
        entry& operator=(const entry&) = delete;
        entry& operator=(entry&&)      = delete;

        // 相等比较
        friend bool operator==(const entry& a, const entry& b) {
            return a.key == b.key && a.value == b.value;
        }
        friend bool operator!=(const entry& a, const entry& b) {
            return !(a == b);
        }
    };

    // 定义哈希函数和相等比较函数
    struct key_hash {
        std::size_t operator()(const entry& e) const {
            return std::hash<std::string>()(e.key);
        }
        std::size_t operator()(const std::string& key) const {
            return std::hash<std::string>()(key);
        }
        std::size_t operator()(std::string_view key) const {
            return std::hash<std::string_view>()(key);
        }
        std::size_t operator()(const char* key) const {
            return std::hash<std::string_view>()(key);
        }
    };

    struct key_equal {
        bool operator()(const entry& lhs, const entry& rhs) const {
            return lhs.key == rhs.key;
        }
        bool operator()(const std::string& key, const entry& e) const {
            return key == e.key;
        }
        bool operator()(const entry& e, const std::string& key) const {
            return e.key == key;
        }
        bool operator()(std::string_view key, const entry& e) const {
            return key == e.key;
        }
        bool operator()(const entry& e, std::string_view key) const {
            return e.key == key;
        }
        bool operator()(const char* key, const entry& e) const {
            return key == e.key;
        }
        bool operator()(const entry& e, const char* key) const {
            return e.key == key;
        }
    };

    // 定义侵入式容器类型
    using entry_list = mc::intrusive::list<entry>;
    using entry_set =
        mc::intrusive::unordered_set<entry, key_hash, key_equal, mc::intrusive::constant_time_size>;

    /**
     * @brief 迭代器类型定义
     */
    using const_iterator = entry_list::const_iterator;

    /**
     * @brief 默认构造函数
     */
    dict();

    /**
     * @brief 从键值对集合构造
     */
    dict(std::vector<entry> entries);

    /**
     * @brief 从初始化列表构造
     *
     * @param init 键值对初始化列表
     *
     * @note 此构造函数允许使用更简洁的语法创建字典：
     *       dict d = {{"key1", 123}, {"key2", "value"}, {"key3", true}};
     */
    dict(std::initializer_list<std::pair<std::string, variant>> init);

    /**
     * @brief 从初始化列表构造（模板版本）
     *
     * @param init 键值对初始化列表
     *
     * @note 此构造函数允许使用更简洁的语法创建字典，无需显式创建 variant：
     *       dict d = {{"key1", 123}, {"key2", "value"}, {"key3", true}};
     */
    template <typename T>
    dict(std::initializer_list<std::pair<std::string, T>> init)
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

    /**
     * @brief 从初始化列表构造（模板版本，支持不同类型的键）
     *
     * @param init 键值对初始化列表
     *
     * @note 此构造函数允许使用更简洁的语法创建字典，无需显式创建 variant：
     *       dict d = {{key1_str, 123}, {key2_view, "value"}, {key3_cstr, true}};
     */
    template <typename K, typename T>
    dict(std::initializer_list<std::pair<K, T>> init)
        : m_data(std::make_shared<data_t>(init.size())) {
        // 处理初始化列表中的键值对
        for (const auto& pair : init) {
            // 查找是否已存在该键
            auto it = m_data->index.find(std::string(pair.first));
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

    /**
     * @brief 拷贝构造函数
     * @note 此操作会共享内部数据，不会复制数据
     */
    dict(const dict& other);

    /**
     * @brief 移动构造函数
     */
    dict(dict&& other) noexcept;

    /**
     * @brief 析构函数
     */
    ~dict();

    /**
     * @brief 赋值运算符
     * @note 此操作会共享内部数据，不会复制数据
     */
    dict& operator=(const dict& other);
    dict& operator=(dict&& other) noexcept;

    /**
     * @brief 获取指定键的值
     * @throw std::out_of_range 如果键不存在
     */
    const variant& operator[](const std::string& key) const;
    const variant& operator[](std::string_view key) const;
    const variant& operator[](const char* key) const;

    /**
     * @brief 获取指定键的值，如果不存在则返回默认值
     */
    const variant& get(const std::string& key, const variant& default_value) const;
    const variant& get(std::string_view key, const variant& default_value) const;
    const variant& get(const char* key, const variant& default_value) const;

    /**
     * @brief 判断是否包含指定键
     */
    bool contains(const std::string& key) const;
    bool contains(std::string_view key) const;
    bool contains(const char* key) const;

    /**
     * @brief 获取键值对数量
     */
    size_t size() const;

    /**
     * @brief 判断是否为空
     */
    bool empty() const;

    /**
     * @brief 获取开始迭代器
     */
    const_iterator begin() const;

    /**
     * @brief 获取结束迭代器
     */
    const_iterator end() const;

    /**
     * @brief 获取所有键
     */
    std::vector<std::string> keys() const;

    /**
     * @brief 获取所有值
     */
    std::vector<variant> values() const;

    /**
     * @brief 获取指定索引位置的键值对
     * @throw std::out_of_range 如果索引越界
     */
    const entry& at_index(size_t index) const;

    /**
     * @brief 获取指定键的值
     * @throw std::out_of_range 如果键不存在
     */
    const variant& at(const std::string& key) const;
    const variant& at(std::string_view key) const;
    const variant& at(const char* key) const;

    /**
     * @brief 查找键的索引位置
     * @return 键的索引位置，如果不存在则返回 -1
     */
    int find_index(const std::string& key) const;
    int find_index(std::string_view key) const;
    int find_index(const char* key) const;

    /**
     * @brief 比较两个 dict 对象是否相等
     * @param other 要比较的 dict 对象
     * @return 如果两个对象相等则返回 true，否则返回 false
     */
    bool operator==(const dict& other) const;

    /**
     * @brief 比较两个 dict 对象是否不相等
     * @param other 要比较的 dict 对象
     * @return 如果两个对象不相等则返回 true，否则返回 false
     */
    bool operator!=(const dict& other) const {
        return !(*this == other);
    }

    /**
     * @brief 计算字典的哈希值
     * @return 哈希值
     * @note 此哈希算法参考了Lua表的哈希算法，考虑了键值对的内容
     */
    size_t hash() const;

    /**
     * @brief 查找指定键的元素，返回迭代器
     * @param key 要查找的键
     * @return 指向找到元素的迭代器，如果不存在则返回 end()
     */
    const_iterator find(const std::string& key) const;
    const_iterator find(std::string_view key) const;
    const_iterator find(const char* key) const;

protected:
    /**
     * @brief 存储数据的结构
     */
    struct data_t {
        // 哈希表桶数组
        mc::intrusive::bucket_array buckets;
        // 有序链表
        entry_list entries;
        // 哈希表
        entry_set index;

        // 构造函数
        data_t(size_t bucket_count = 16)
            : buckets(std::max<size_t>(bucket_count, 1)), entries(),
              index(buckets.data(), buckets.size()) {
        }

        // 析构函数，清理资源
        ~data_t() {
            // 先清空索引，这样钩子就不再链接到容器中
            index.clear();
            // 然后清空链表并释放内存
            entries.clear_and_dispose([](entry* p) {
                delete p;
            });
        }
    };

    /**
     * @brief 共享的数据指针
     */
    std::shared_ptr<data_t> m_data;

    /**
     * @brief 查找指定键的元素
     * @return 指向找到的元素的指针，如果不存在则返回 nullptr
     */
    const entry* find_entry(const std::string& key) const;
    const entry* find_entry(std::string_view key) const;
    const entry* find_entry(const char* key) const;

    /**
     * @brief 计算指定元素在列表中的索引位置
     * @param e 要计算索引的元素指针
     * @return 元素的索引位置，如果元素不存在则返回 -1
     */
    int find_entry_index(const entry* e) const;
};

/**
 * @brief 将 dict 转换为 variant
 */
variant to_variant(const dict& d);

/**
 * @brief 可变的字典类，保持键值对的插入顺序
 *
 * @note 此类继承自 dict，使用共享数据模型。
 *       对 mutable_dict 的修改会影响所有共享同一数据的 dict 对象。
 *       如果需要独立的数据副本，应该先创建一个新的 mutable_dict 对象，
 *       然后再进行修改。
 *
 * @note 与 dict 类一样，此类使用侵入式容器实现，有序索引部分用的是list，通过键查找元素的性能较好，
 *       但随机索引访问（如 at_index(index)）的性能较慢，时间复杂度为 O(n)。
 *       如果需要顺序访问所有元素，强烈建议使用迭代器（begin()/end()）而不是索引，
 *       以获得更好的性能。例如：
 *
 *       // 低效的遍历方式（不推荐）
 *       for (size_t i = 0; i < dict.size(); ++i) {
 *           auto& entry = dict.at_index(i);
 *           // 处理 entry...
 *       }
 *
 *       // 高效的遍历方式（推荐）
 *       for (auto& entry : dict) {
 *           // 处理 entry...
 *       }
 */
class mutable_dict : public dict {
public:
    // 使用基类的 entry 类型
    using entry = dict::entry;
    // 使用基类的迭代器类型
    using iterator       = dict::entry_list::iterator;
    using const_iterator = dict::entry_list::const_iterator;

    /**
     * @brief 默认构造函数
     */
    mutable_dict();

    /**
     * @brief 单键值对构造函数
     *
     * @param key 键
     * @param value 值
     *
     * @note 此构造函数允许使用链式调用创建字典：
     *       mutable_dict(key1, value1)(key2, value2)(key3, value3);
     */
    mutable_dict(std::string key, variant value);

    /**
     * @brief 单键值对构造函数（模板版本）
     *
     * @param key 键
     * @param value 值
     *
     * @note 此构造函数允许使用链式调用创建字典，无需显式创建 variant：
     *       mutable_dict(key1, value1)(key2, value2)(key3, value3);
     */
    template <typename T>
    mutable_dict(std::string key, T&& value) : dict() {
        (*this)(std::move(key), std::forward<T>(value));
    }

    /**
     * @brief 单键值对构造函数（模板版本，支持不同类型的键）
     *
     * @param key 键
     * @param value 值
     *
     * @note 此构造函数允许使用链式调用创建字典，无需显式创建 variant：
     *       mutable_dict(key1, value1)(key2, value2)(key3, value3);
     */
    template <typename K, typename T>
    mutable_dict(K&& key, T&& value) : dict() {
        (*this)(std::forward<K>(key), std::forward<T>(value));
    }

    /**
     * @brief 从键值对集合构造
     */
    mutable_dict(std::vector<entry> entries);

    /**
     * @brief 从初始化列表构造
     *
     * @param init 键值对初始化列表
     *
     * @note 此构造函数允许使用更简洁的语法创建字典：
     *       mutable_dict md = {{"key1", 123}, {"key2", "value"}, {"key3", true}};
     */
    mutable_dict(std::initializer_list<std::pair<std::string, variant>> init);

    /**
     * @brief 从初始化列表构造（模板版本）
     *
     * @param init 键值对初始化列表
     *
     * @note 此构造函数允许使用更简洁的语法创建字典，无需显式创建 variant：
     *       mutable_dict md = {{"key1", 123}, {"key2", "value"}, {"key3", true}};
     */
    template <typename T>
    mutable_dict(std::initializer_list<std::pair<std::string, T>> init) : dict(init) {
    }

    /**
     * @brief 从初始化列表构造（模板版本，支持不同类型的键）
     *
     * @param init 键值对初始化列表
     *
     * @note 此构造函数允许使用更简洁的语法创建字典，无需显式创建 variant：
     *       mutable_dict md = {{key1_str, 123}, {key2_view, "value"}, {key3_cstr, true}};
     */
    template <typename K, typename T>
    mutable_dict(std::initializer_list<std::pair<K, T>> init) : dict(init) {
    }

    /**
     * @brief 从 dict 构造
     * @note 此操作会共享内部数据，修改会影响原始 dict 对象
     */
    mutable_dict(const dict& other);

    /**
     * @brief 拷贝构造函数
     * @note 此操作会共享内部数据，修改会影响原始对象
     */
    mutable_dict(const mutable_dict& other);

    /**
     * @brief 移动构造函数
     */
    mutable_dict(mutable_dict&& other) noexcept;

    /**
     * @brief 析构函数
     */
    ~mutable_dict();

    /**
     * @brief 赋值运算符
     * @note 此操作会共享内部数据，修改会影响原始对象
     */
    mutable_dict& operator=(const mutable_dict& other);
    mutable_dict& operator=(mutable_dict&& other) noexcept;
    mutable_dict& operator=(const dict& other);

    /**
     * @brief 从初始化列表赋值
     * @param init 键值对初始化列表
     * @return 返回自身引用
     * @note 此操作会替换当前的内容
     */
    mutable_dict& operator=(std::initializer_list<std::pair<std::string, variant>> init);

    /**
     * @brief 从初始化列表赋值（模板版本）
     * @param init 键值对初始化列表
     * @return 返回自身引用
     * @note 此操作会替换当前的内容
     */
    template <typename T>
    mutable_dict& operator=(std::initializer_list<std::pair<std::string, T>> init) {
        // 创建一个新的mutable_dict并用初始化列表构造
        mutable_dict new_dict(init);
        // 交换内部数据
        *this = std::move(new_dict);
        return *this;
    }

    /**
     * @brief 从初始化列表赋值（模板版本，支持不同类型的键）
     * @param init 键值对初始化列表
     * @return 返回自身引用
     * @note 此操作会替换当前的内容
     */
    template <typename K, typename T>
    mutable_dict& operator=(std::initializer_list<std::pair<K, T>> init) {
        // 创建一个新的mutable_dict并用初始化列表构造
        mutable_dict new_dict(init);
        // 交换内部数据
        *this = std::move(new_dict);
        return *this;
    }

    /**
     * @brief 添加或更新键值对
     * @return 返回自身引用，支持链式调用
     * @note 此操作会修改共享数据，影响所有共享该数据的对象
     */
    mutable_dict& operator()(const std::string& key, variant value);
    mutable_dict& operator()(std::string_view key, variant value);
    mutable_dict& operator()(const char* key, variant value);

    /**
     * @brief 添加或更新键值对的模板版本，自动将值转换为 variant
     *
     * @tparam T 值的类型，必须能够被 variant 构造
     * @param key 键
     * @param var 值
     * @return mutable_dict& 返回自身引用，支持链式调用
     *
     * @note 此方法允许使用更简洁的语法创建和填充字典：
     *       mutable_dict md;
     *       md("key1", 123)("key2", "value")("key3", true);
     */
    template <typename T>
    mutable_dict& operator()(std::string key, T&& var) {
        return (*this)(std::move(key), variant(std::forward<T>(var)));
    }

    template <typename T>
    mutable_dict& operator()(std::string_view key, T&& var) {
        return (*this)(key, variant(std::forward<T>(var)));
    }

    template <typename T>
    mutable_dict& operator()(const char* key, T&& var) {
        return (*this)(key, variant(std::forward<T>(var)));
    }

    /**
     * @brief 获取或设置指定键的值
     * @note 如果键不存在，会自动创建
     * @note 此操作可能会修改共享数据，影响所有共享该数据的对象
     */
    variant& operator[](const std::string& key);
    variant& operator[](std::string_view key);
    variant& operator[](const char* key);

    /**
     * @brief 获取指定键的值（const 版本）
     * @throw std::out_of_range 如果键不存在
     */
    const variant& operator[](const std::string& key) const;
    const variant& operator[](std::string_view key) const;
    const variant& operator[](const char* key) const;

    /**
     * @brief 移除指定键的键值对
     * @return 如果键存在并被移除则返回 true，否则返回 false
     * @note 此操作会修改共享数据，影响所有共享该数据的对象
     */
    bool erase(const std::string& key);
    bool erase(std::string_view key);
    bool erase(const char* key);

    /**
     * @brief 清空所有键值对
     * @note 此操作会修改共享数据，影响所有共享该数据的对象
     */
    void clear();

    /**
     * @brief 获取开始迭代器
     * @note 通过迭代器修改数据会影响所有共享该数据的对象
     */
    iterator       begin();
    const_iterator begin() const;

    /**
     * @brief 获取结束迭代器
     */
    iterator       end();
    const_iterator end() const;

    /**
     * @brief 获取指定索引位置的键值对
     * @throw std::out_of_range 如果索引越界
     * @note 修改返回的引用会影响所有共享该数据的对象
     */
    entry& at_index(size_t index);

    /**
     * @brief 获取指定键的值（可变）
     * @throw std::out_of_range 如果键不存在
     */
    variant& at(const std::string& key);
    variant& at(std::string_view key);
    variant& at(const char* key);

    // 继承基类的所有 const 版本 at 方法
    using dict::at;

    /**
     * @brief 查找指定键的元素，返回迭代器
     * @param key 要查找的键
     * @return 指向找到元素的迭代器，如果不存在则返回 end()
     */
    iterator       find(const std::string& key);
    iterator       find(std::string_view key);
    iterator       find(const char* key);
    const_iterator find(const std::string& key) const;
    const_iterator find(std::string_view key) const;
    const_iterator find(const char* key) const;

    // 继承基类的const版本operator[]
    using dict::operator[];

    // 继承基类的const版本at_index
    using dict::at_index;

    // 继承基类的const版本begin/end
    using dict::begin;
    using dict::end;

private:
    /**
     * @brief 查找指定键的元素
     */
    entry* find_entry(const std::string& key);
    entry* find_entry(std::string_view key);
    entry* find_entry(const char* key);
};

} // namespace mc

// 将 variant 实现相关的代码放到单独的文件中
#include <mc/variant/container_convert.inl>
#include <mc/variant/variant_dict.inl>

// 定义在std命名空间中特化hash以支持dict和mutable_dict
namespace std {
template <>
struct hash<mc::dict> {
    size_t operator()(const mc::dict& d) const {
        return d.hash();
    }
};

template <>
struct hash<mc::mutable_dict> {
    size_t operator()(const mc::mutable_dict& d) const {
        return d.hash();
    }
};
} // namespace std

#endif // MC_DICT_H
