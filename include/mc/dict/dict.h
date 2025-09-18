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
class mutable_dict;
namespace dict_types {
struct entry;
struct const_iterator;
struct data_t;
} // namespace dict_types

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
class MC_API dict {
public:
    using key_type       = variant;
    using value_type     = variant;
    using entry          = dict_types::entry;
    using const_iterator = dict_types::const_iterator;
    using data_t         = dict_types::data_t;

    /**
     * @brief 默认构造函数
     */
    dict() = default;

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
     * @brief 获取指定键的值
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
    std::vector<variant> keys() const;

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

    mutable_dict as_mut() const;

    void clear();

protected:
    /**
     * @brief 共享的数据指针
     */
    mutable mc::shared_ptr<data_t> m_data;

    /**
     * @brief 查找指定键的元素
     * @return 指向找到的元素的指针，如果不存在则返回 nullptr
     */
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
};

/**
 * @brief 将 dict 转换为 variant
 */
MC_API variant to_variant(const dict& d);

} // namespace mc
// 将 variant 实现相关的代码放到单独的文件中
// 实现细节移到 dict.cpp 中，避免循环依赖

// 定义在std命名空间中特化hash以支持dict和mutable_dict
namespace std {
template <>
struct hash<mc::dict> {
    size_t operator()(const mc::dict& d) const {
        return d.hash();
    }
};

} // namespace std

#endif // MC_DICT_H
