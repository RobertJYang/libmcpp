/**
 * @file dict.h
 * @brief 定义了不可变的字典类，用于表示键值对集合，保持插入顺序
 */
#pragma once

#include <string>
#include <vector>
#include "variant.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <map>
#include <unordered_map>

namespace mc {

/**
 * @brief 不可变的字典类，保持键值对的插入顺序
 */
class dict {
public:
    /**
     * @brief 表示键值对的结构体
     */
    struct entry {
        std::string key;
        variant value;
    };

    /**
     * @brief 迭代器类型定义
     */
    using const_iterator = std::vector<entry>::const_iterator;

    /**
     * @brief 默认构造函数
     */
    dict();

    /**
     * @brief 从键值对集合构造
     */
    dict(std::vector<entry> entries);

    /**
     * @brief 拷贝构造函数
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
     */
    dict& operator=(const dict& other);
    dict& operator=(dict&& other) noexcept;

    /**
     * @brief 获取指定键的值
     * @throw std::out_of_range 如果键不存在
     */
    const variant& operator[](const std::string& key) const;

    /**
     * @brief 获取指定键的值，如果不存在则返回默认值
     */
    const variant& get(const std::string& key, const variant& default_value) const;

    /**
     * @brief 判断是否包含指定键
     */
    bool contains(const std::string& key) const;

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
    const entry& at(size_t index) const;

    /**
     * @brief 查找键的索引位置
     * @return 键的索引位置，如果不存在则返回 -1
     */
    int find_index(const std::string& key) const;

protected:
    /**
     * @brief 存储键值对的向量，保持插入顺序
     */
    std::vector<entry> m_items;
};

} // namespace mc

// 包含容器转换头文件
#include <mc/variant/container_convert.h>
