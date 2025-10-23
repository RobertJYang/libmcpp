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
 * @brief 定义了字典类 dict ，用于表示键值对集合，保持插入顺序
 */
#ifndef MC_DICT_DICT_H
#define MC_DICT_DICT_H

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
#include <mc/memory.h>
#include <mc/variant/variant_common.h>

namespace mc {

// 前向声明
namespace detail {
class copy_context;
}

namespace dict_types {
struct entry;
struct iterator;
struct const_iterator;
struct reverse_iterator;
struct const_reverse_iterator;
struct data_t;
} // namespace dict_types

/**
 * @brief 字典类，保持键值对的插入顺序
 *
 * @note 此类使用共享数据模型，拷贝操作会共享内部数据。
 *       多个 dict 对象可能共享相同的内部数据，
 *       修改共享数据会影响所有共享该数据的对象。
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
class MC_API dict {
public:
    using key_type               = variant;
    using value_type             = variant;
    using entry                  = dict_types::entry;
    using iterator               = dict_types::iterator;
    using const_iterator         = dict_types::const_iterator;
    using reverse_iterator       = dict_types::reverse_iterator;
    using const_reverse_iterator = dict_types::const_reverse_iterator;
    using data_t                 = dict_types::data_t;

    /**
     * @brief 默认构造函数
     */
    dict() = default;

    /**
     * @brief 单键值对构造函数
     *
     * @param key 键
     * @param value 值
     *
     * @note 此构造函数允许使用链式调用创建字典：
     *       dict(key1, value1)(key2, value2)(key3, value3);
     */
    dict(variant key, variant value);

    /**
     * @brief 从键值对集合构造
     */
    dict(const std::vector<entry>& entries);

    /**
     * @brief 从初始化列表构造
     *
     * @param init 键值对初始化列表
     *
     * @note 此构造函数允许使用更简洁的语法创建字典：
     *       dict d = {{"key1", 123}, {"key2", "value"}, {"key3", true}};
     */
    dict(std::initializer_list<std::pair<variant, variant>> init);

    /**
     * @brief 从初始化列表构造（模板版本）
     *
     * @param init 键值对初始化列表
     *
     * @note 此构造函数允许使用更简洁的语法创建字典，无需显式创建 variant：
     *       dict md = {{"key1", 123}, {"key2", "value"}, {"key3", true}};
     */
    template <typename T>
    dict(std::initializer_list<std::pair<variant, T>> init) : dict(init.begin(), init.end()) {
    }

    /**
     * @brief 从初始化列表构造（模板版本，支持不同类型的键）
     *
     * @param init 键值对初始化列表
     *
     * @note 此构造函数允许使用更简洁的语法创建字典，无需显式创建 variant：
     *       dict md = {{key1_str, 123}, {key2_view, "value"}, {key3_cstr, true}};
     */
    template <typename K, typename T>
    dict(std::initializer_list<std::pair<K, T>> init) : dict(init.begin(), init.end()) {
    }

    /**
     * @brief 从迭代器范围构造
     * @tparam InputIt 输入迭代器类型
     * @param first 起始迭代器
     * @param last 结束迭代器
     */
    template <typename InputIt>
    dict(InputIt first, InputIt last);

    /**
     * @brief 拷贝构造函数
     * @note 此操作会共享内部数据，不会复制数据
     */
    dict(const dict& other) = default;

    /**
     * @brief 移动构造函数
     */
    dict(dict&& other) noexcept = default;

    /**
     * @brief 析构函数
     */
    ~dict() = default;

    /**
     * @brief 赋值运算符
     * @note 此操作会共享内部数据，不会复制数据
     */
    dict& operator=(const dict& other)     = default;
    dict& operator=(dict&& other) noexcept = default;

    /**
     * @brief 从初始化列表赋值
     * @param init 键值对初始化列表
     * @return 返回自身引用
     * @note 此操作会替换当前的内容
     */
    dict& operator=(std::initializer_list<std::pair<variant, variant>> init);

    /**
     * @brief 从初始化列表赋值（模板版本）
     * @param init 键值对初始化列表
     * @return 返回自身引用
     * @note 此操作会替换当前的内容
     */
    template <typename T>
    dict& operator=(std::initializer_list<std::pair<variant, T>> init) {
        dict new_dict(init);
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
    dict& operator=(std::initializer_list<std::pair<K, T>> init) {
        dict new_dict(init);
        *this = std::move(new_dict);
        return *this;
    }

    /**
     * @brief 添加或更新键值对的模板版本，自动将值转换为 variant
     *
     * @tparam T 值的类型，必须能够被 variant 构造
     * @param key 键
     * @param var 值
     * @return dict& 返回自身引用，支持链式调用
     *
     * @note 此方法允许使用更简洁的语法创建和填充字典：
     *       dict md;
     *       md("key1", 123)("key2", "value")("key3", true);
     */
    dict& operator()(variant key, variant value);

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
     * @brief 获取指定键的值，如果不存在则返回默认值
     */
    const variant& get(const std::string& key, const variant& default_value) const;
    const variant& get(std::string_view key, const variant& default_value) const;
    const variant& get(const char* key, const variant& default_value) const;
    const variant& get(const variant& key, const variant& default_value) const;
    /**
     * @brief 判断是否包含指定键
     */
    bool contains(const std::string& key) const;
    bool contains(std::string_view key) const;
    bool contains(const char* key) const;
    bool contains(const variant& key) const;
    /**
     * @brief 获取键值对数量
     */
    size_t size() const;

    /**
     * @brief 判断是否为空
     */
    bool empty() const;

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
     * @brief 获取反向开始迭代器
     */
    const_reverse_iterator rbegin() const;

    /**
     * @brief 获取反向结束迭代器
     */
    const_reverse_iterator rend() const;

    /**
     * @brief 获取所有键
     */
    std::vector<variant> keys() const;

    /**
     * @brief 获取所有值
     */
    std::vector<variant> values() const;

    /**
     * @brief 获取指定索引位置的键值对
     * @throw std::out_of_range 如果索引越界
     * @note 修改返回的引用会影响所有共享该数据的对象
     */
    entry&       at_index(size_t index);
    const entry& at_index(size_t index) const;

    /**
     * @brief 获取指定键的值（可变）
     * @throw std::out_of_range 如果键不存在
     */
    variant& at(const std::string& key);
    variant& at(std::string_view key);
    variant& at(const char* key);
    variant& at(const variant& key);

    /**
     * @brief 获取指定键的值（const 版本）
     * @throw std::out_of_range 如果键不存在
     */
    const variant& at(const std::string& key) const;
    const variant& at(std::string_view key) const;
    const variant& at(const char* key) const;
    const variant& at(const variant& key) const;
    const variant& at(std::size_t index) const;

    /**
     * @brief 查找键的索引位置
     * @return 键的索引位置，如果不存在则返回 -1
     */
    int find_index(const std::string& key) const;
    int find_index(std::string_view key) const;
    int find_index(const char* key) const;
    int find_index(const variant& key) const;
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
     * @brief 合并两个字典
     * @param other 要合并的字典
     * @return 合并后的字典
     */
    dict operator+(const dict& other) const;

    /**
     * @brief 计算字典的哈希值
     * @return 哈希值
     */
    size_t hash() const;

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
    const_iterator find(std::size_t index) const;
    std::string    to_string() const;

    /**
     * @brief 获取数据指针
     * @return 数据指针
     */
    data_t* data() const {
        return m_data.get();
    }

    /**
     * @brief 合并两个字典
     * @param other 要合并的字典
     * @return 返回自身引用
     */
    dict& operator+=(const dict& other);

    /**
     * @brief 插入键值对
     * @param e 键值对
     * @return 返回自身引用
     */
    dict& insert(entry e);

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
        std::is_convertible_v<typename std::iterator_traits<InputIt>::value_type, entry>>;

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
                                typename std::iterator_traits<InputIt>::value_type::second_type>>;

    /**
     * @brief 插入初始化列表
     * @param ilist 包含键值对的初始化列表
     */
    void insert(std::initializer_list<entry> ilist);

    /**
     * @brief 插入初始化列表模板版本，支持自动转换其他类型为variant
     * @tparam K 键的类型
     * @tparam V 值的类型
     * @param ilist 包含键值对的初始化列表
     */
    template <typename K, typename V>
    void insert(std::initializer_list<std::pair<K, V>> ilist);

    /**
     * @brief 尝试原地创建并插入元素
     * @tparam Arg 构造值的参数类型
     * @param key 键
     * @param arg 构造值的参数
     * @return 返回包含插入结果的std::pair，first为指向插入位置的迭代器，second为是否成功插入
     */
    template <typename Arg>
    std::pair<iterator, bool> emplace(variant key, Arg&& arg);

    /**
     * @brief 如果键不存在，则尝试原地创建并插入元素
     * @tparam K 键的类型
     * @tparam Arg 构造值的参数类型
     * @param key 键
     * @param arg 构造值的参数
     * @return 返回包含插入结果的std::pair，first为指向插入位置的迭代器，second为是否成功插入
     */
    template <typename K, typename Arg>
    std::pair<iterator, bool> try_emplace(const K& key, Arg&& arg);

    /**
     * @brief 获取可变引用（为了向后兼容保留）
     * @return 返回自身引用
     * @note dict 本身就是可变的，此方法仅为向后兼容而保留
     */
    dict& as_mut() {
        return *this;
    }

    /**
     * @brief 获取可变引用（const 版本，为了向后兼容保留）
     * @return 返回自身的可变副本
     * @note dict 本身就是可变的，此方法仅为向后兼容而保留
     */
    dict as_mut() const {
        return *this;
    }

    /**
     * @brief 浅拷贝字典
     * @return 浅拷贝后的字典
     */
    dict copy() const;

    /**
     * @brief 深拷贝字典
     * @param ctx 可选的深拷贝上下文，用于检测循环引用并记录已拷贝对象
     * @return 深度拷贝后的字典
     *
     * @note 如果传入 ctx 参数，则使用该上下文进行循环引用检测
     * @note 如果不传入 ctx，则创建局部上下文（用于顶层调用）
     * @note 遇到循环引用时，返回已拷贝的对象，保持引用关系
     */
    dict deep_copy(mc::detail::copy_context* ctx = nullptr) const;

protected:
    /**
     * @brief 共享的数据指针
     */
    mutable mc::shared_ptr<data_t> m_data;

    /**
     * @brief 查找指定键的元素
     * @return 指向找到的元素的指针，如果不存在则返回 nullptr
     */
    entry*       find_entry(const std::string& key);
    entry*       find_entry(std::string_view key);
    entry*       find_entry(const char* key);
    entry*       find_entry(const variant& key);
    const entry* find_entry(const std::string& key) const;
    const entry* find_entry(std::string_view key) const;
    const entry* find_entry(const char* key) const;
    const entry* find_entry(const variant& key) const;
    /**
     * @brief 计算指定元素在列表中的索引位置
     * @param e 要计算索引的元素指针
     * @return 元素的索引位置，如果元素不存在则返回 -1
     */
    int find_entry_index(const entry* e) const;

    /**
     * @brief 确保数据已初始化
     * @return 数据引用
     */
    data_t& ensure_data() const;
};

/**
 * @brief 将 dict 转换为 variant
 */
MC_API variant to_variant(const dict& d);

} // namespace mc
// 将 variant 实现相关的代码放到单独的文件中
// 实现细节移到 dict.cpp 中，避免循环依赖

// 包含完整的类型定义以支持模板实现
#include <mc/dict/entry.h>

// 定义在std命名空间中特化hash以支持dict和mutable_dict
namespace std {
template <>
struct hash<mc::dict> {
    size_t operator()(const mc::dict& d) const {
        return d.hash();
    }
};

} // namespace std

namespace mc {
template <typename InputIt>
dict::dict(InputIt first, InputIt last) {
    m_data = mc::make_shared<data_t>();
    for (; first != last; ++first) {
        insert(first->first, first->second);
    }
}

template <typename InputIt>
auto dict::insert(InputIt first, InputIt last) -> std::enable_if_t<
    std::is_convertible_v<typename std::iterator_traits<InputIt>::value_type, entry>> {
    for (; first != last; ++first) {
        insert(*first);
    }
}

template <typename InputIt>
auto dict::insert(InputIt first, InputIt last)
    -> std::enable_if_t<is_variant_constructible_v<
                            typename std::iterator_traits<InputIt>::value_type::first_type> &&
                        is_variant_constructible_v<
                            typename std::iterator_traits<InputIt>::value_type::second_type>> {
    for (; first != last; ++first) {
        insert(first->first, first->second);
    }
}

template <typename K, typename V>
void dict::insert(std::initializer_list<std::pair<K, V>> ilist) {
    for (const auto& pair : ilist) {
        insert(pair.first, pair.second);
    }
}

template <typename Arg>
std::pair<dict::iterator, bool> dict::emplace(variant key, Arg&& arg) {
    return insert(std::move(key), variant(std::forward<Arg>(arg)));
}

template <typename K, typename Arg>
std::pair<dict::iterator, bool> dict::try_emplace(const K& key, Arg&& arg) {
    variant vkey(key);
    if (!contains(vkey)) {
        return emplace(vkey, std::forward<Arg>(arg));
    }
    return {find(vkey), false};
}
} // namespace mc

#endif // MC_DICT_H
