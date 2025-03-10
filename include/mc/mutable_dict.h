/**
 * @file mutable_dict.h
 * @brief 定义了可变的字典类，用于表示键值对集合，保持插入顺序
 */
#ifndef MC_MUTABLE_DICT_H
#define MC_MUTABLE_DICT_H

#include <mc/dict.h>

namespace mc {

/**
 * @brief 可变的字典类，保持键值对的插入顺序
 */
class mutable_dict : public dict {
public:
    /**
     * @brief 默认构造函数
     */
    mutable_dict();

    /**
     * @brief 从键值对集合构造
     */
    mutable_dict(std::vector<entry> entries);

    /**
     * @brief 从 dict 构造
     */
    mutable_dict(const dict& other);

    /**
     * @brief 拷贝构造函数
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
     */
    mutable_dict& operator=(const mutable_dict& other);
    mutable_dict& operator=(mutable_dict&& other) noexcept;
    mutable_dict& operator=(const dict& other);

    /**
     * @brief 添加或更新键值对
     * @return 返回自身引用，支持链式调用
     */
    mutable_dict& operator()(const std::string& key, variant value);

    /**
     * @brief 获取或设置指定键的值
     * @note 如果键不存在，会自动创建
     */
    variant& operator[](const std::string& key);

    /**
     * @brief 获取指定键的值（const 版本）
     * @throw std::out_of_range 如果键不存在
     */
    const variant& operator[](const std::string& key) const;

    /**
     * @brief 移除指定键的键值对
     * @return 如果键存在并被移除则返回 true，否则返回 false
     */
    bool erase(const std::string& key);

    /**
     * @brief 清空所有键值对
     */
    void clear();

    /**
     * @brief 迭代器类型定义
     */
    using iterator = std::vector<entry>::iterator;
    using const_iterator = std::vector<entry>::const_iterator;

    /**
     * @brief 获取开始迭代器
     */
    iterator begin();
    const_iterator begin() const;

    /**
     * @brief 获取结束迭代器
     */
    iterator end();
    const_iterator end() const;

    /**
     * @brief 获取指定索引位置的键值对
     * @throw std::out_of_range 如果索引越界
     */
    entry& at(size_t index);
    const entry& at(size_t index) const;
};

/*
 * 注意：to_variant 函数已在 dict.h 中定义
 */

} // namespace mc 

#endif // MC_MUTABLE_DICT_H 