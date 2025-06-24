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
#ifndef MC_MUTABLE_DICT_H
#define MC_MUTABLE_DICT_H

#include <mc/dict/dict.h>
#include <mc/dict/entry.h>
#include <mc/variant/variant_base.h>

namespace mc {

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

    // 使用基类的迭代器类型 - 具体定义在 dict.cpp 中
    using iterator       = dict_types::iterator;
    using const_iterator = dict_types::const_iterator;

    mutable_dict()                                         = default;
    mutable_dict(const mutable_dict& other)                = default;
    mutable_dict(mutable_dict&& other) noexcept            = default;
    mutable_dict& operator=(const mutable_dict& other)     = default;
    mutable_dict& operator=(mutable_dict&& other) noexcept = default;
    ~mutable_dict()                                        = default;

    /**
     * @brief 单键值对构造函数
     *
     * @param key 键
     * @param value 值
     *
     * @note 此构造函数允许使用链式调用创建字典：
     *       mutable_dict(key1, value1)(key2, value2)(key3, value3);
     */
    mutable_dict(variant key, variant value);

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
    mutable_dict(std::initializer_list<std::pair<variant, variant>> init);

    /**
     * @brief 从初始化列表构造（模板版本）
     *
     * @param init 键值对初始化列表
     *
     * @note 此构造函数允许使用更简洁的语法创建字典，无需显式创建 variant：
     *       mutable_dict md = {{"key1", 123}, {"key2", "value"}, {"key3", true}};
     */
    template <typename T>
    mutable_dict(std::initializer_list<std::pair<variant, T>> init) : dict(init) {
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
    mutable_dict& operator=(const dict& other);

    /**
     * @brief 从初始化列表赋值
     * @param init 键值对初始化列表
     * @return 返回自身引用
     * @note 此操作会替换当前的内容
     */
    mutable_dict& operator=(std::initializer_list<std::pair<variant, variant>> init);

    /**
     * @brief 从初始化列表赋值（模板版本）
     * @param init 键值对初始化列表
     * @return 返回自身引用
     * @note 此操作会替换当前的内容
     */
    template <typename T>
    mutable_dict& operator=(std::initializer_list<std::pair<variant, T>> init) {
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
    mutable_dict& operator()(variant key, variant value);

    /**
     * @brief 获取或设置指定键的值
     * @note 如果键不存在，会自动创建
     * @note 此操作可能会修改共享数据，影响所有共享该数据的对象
     */
    variant& operator[](const std::string& key);
    variant& operator[](std::string_view key);
    variant& operator[](const char* key);
    variant& operator[](const variant& key);
    /**
     * @brief 获取指定键的值（const 版本）
     * @throw std::out_of_range 如果键不存在
     */
    const variant& operator[](const std::string& key) const;
    const variant& operator[](std::string_view key) const;
    const variant& operator[](const char* key) const;
    const variant& operator[](const variant& key) const;

    /**
     * @brief 移除指定键的键值对
     * @return 如果键存在并被移除则返回 true，否则返回 false
     * @note 此操作会修改共享数据，影响所有共享该数据的对象
     */
    bool erase(const std::string& key);
    bool erase(std::string_view key);
    bool erase(const char* key);
    bool erase(const variant& key);

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
    variant& at(const variant& key);

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
    iterator       find(const variant& key);
    const_iterator find(const std::string& key) const;
    const_iterator find(std::string_view key) const;
    const_iterator find(const char* key) const;
    const_iterator find(const variant& key) const;

    /**
     * @brief 合并两个字典
     * @param other 要合并的字典
     * @return 合并后的字典
     */
    mutable_dict& operator+=(const mutable_dict& other);

    /**
     * @brief 插入键值对
     * @param e 键值对
     * @return 返回自身引用
     */
    mutable_dict& insert(entry e);

    /**
     * @brief 插入键值对
     * @param key 键
     * @param value 值
     * @return 返回包含插入结果的std::pair，first为指向插入位置的迭代器，second为是否成功插入
     */
    std::pair<iterator, bool> insert(variant key, variant value);

    /**
     * @brief 带提示位置的插入键值对
     * @param hint 提示位置的迭代器，如果提示正确可以提高插入性能
     * @param key 键
     * @param value 值
     * @return 指向插入位置的迭代器
     */
    iterator insert(const_iterator hint, variant key, variant value);

    /**
     * @brief 插入迭代器范围，仅适用于dict::entry迭代器
     * @tparam InputIt 输入迭代器类型，必须能够解引用为dict::entry类型
     * @param first 起始迭代器
     * @param last 结束迭代器
     */
    template <typename InputIt>
    auto insert(InputIt first, InputIt last) -> std::enable_if_t<
        std::is_convertible_v<typename std::iterator_traits<InputIt>::value_type, entry>> {
        for (; first != last; ++first) {
            insert(*first);
        }
    }

    /**
     * @brief 插入键值对迭代器范围
     * @tparam InputIt 输入迭代器类型，必须能够解引用为std::pair<K,V>类型
     * @param first 起始迭代器
     * @param last 结束迭代器
     */
    template <typename InputIt>
    auto insert(InputIt first, InputIt last)
        -> std::enable_if_t<is_variant_constructible_v<
                                typename std::iterator_traits<InputIt>::value_type::first_type> &&
                            is_variant_constructible_v<
                                typename std::iterator_traits<InputIt>::value_type::second_type>> {
        for (; first != last; ++first) {
            insert(first->first, first->second);
        }
    }

    /**
     * @brief 插入初始化列表
     * @param ilist 包含键值对的初始化列表
     */
    void insert(std::initializer_list<entry> ilist) {
        insert(ilist.begin(), ilist.end());
    }

    /**
     * @brief 插入初始化列表模板版本，支持自动转换其他类型为variant
     * @tparam K 键的类型
     * @tparam V 值的类型
     * @param ilist 包含键值对的初始化列表
     */
    template <typename K, typename V>
    void insert(std::initializer_list<std::pair<K, V>> ilist) {
        for (const auto& pair : ilist) {
            insert(pair.first, pair.second);
        }
    }

    /**
     * @brief 尝试原地创建并插入元素
     * @tparam Arg 构造值的参数类型
     * @param key 键
     * @param arg 构造值的参数
     * @return 返回包含插入结果的std::pair，first为指向插入位置的迭代器，second为是否成功插入
     */
    template <typename Arg>
    std::pair<iterator, bool> emplace(variant key, Arg&& arg) {
        return insert(std::move(key), variant(std::forward<Arg>(arg)));
    }

    /**
     * @brief 如果键不存在，则尝试原地创建并插入元素
     * @tparam K 键的类型
     * @tparam Arg 构造值的参数类型
     * @param key 键
     * @param arg 构造值的参数
     * @return 返回包含插入结果的std::pair，first为指向插入位置的迭代器，second为是否成功插入
     */
    template <typename K, typename Arg>
    std::pair<iterator, bool> try_emplace(const K& key, Arg&& arg) {
        variant vkey(key);
        if (!contains(vkey)) {
            return emplace(vkey, std::forward<Arg>(arg));
        }
        return {find(vkey), false};
    }

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
    entry* find_entry(const variant& key);

    data_t& ensure_data() const;
};

inline std::string to_string(const dict& v) {
    return v.to_string();
}

inline std::string to_string(const mutable_dict& v) {
    return v.to_string();
}

} // namespace mc

namespace std {
template <>
struct hash<mc::mutable_dict> {
    size_t operator()(const mc::mutable_dict& d) const {
        return d.hash();
    }
};
} // namespace std

#include <mc/variant/variant_dict.inl>

#endif