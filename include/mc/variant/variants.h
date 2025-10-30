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
 * @file variants.h
 * @brief 定义了 variants 类，用于管理所有类型的数组数据
 */
#ifndef MC_VARIANTS_H
#define MC_VARIANTS_H

#include <memory>
#include <typeindex>
#include <typeinfo>
#include <type_traits>

#include <mc/memory.h>
#include <mc/traits.h>
#include <mc/variant/copy_context.h>
#include <mc/variant/variant_common.h>

namespace mc {
// 前向声明
class i_variants;

/**
 * @brief variants 的迭代器类
 */
class MC_API variants_iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = variant;
    using difference_type   = std::ptrdiff_t;
    using pointer           = variant*;
    using reference         = variant_reference;

    // 构造函数
    variants_iterator();
    variants_iterator(mc::shared_ptr<i_variants> data, size_t index);

    // 拷贝构造函数
    variants_iterator(const variants_iterator& other) = default;

    // 移动构造函数
    variants_iterator(variants_iterator&& other) noexcept = default;

    // 赋值运算符
    variants_iterator& operator=(const variants_iterator& other)     = default;
    variants_iterator& operator=(variants_iterator&& other) noexcept = default;

    // 析构函数
    ~variants_iterator() = default;

    // 迭代器操作
    variants_iterator& operator++();
    variants_iterator  operator++(int);
    variants_iterator& operator--();
    variants_iterator  operator--(int);

    variants_iterator& operator+=(difference_type n);
    variants_iterator& operator-=(difference_type n);

    variants_iterator operator+(difference_type n) const;
    variants_iterator operator-(difference_type n) const;
    difference_type   operator-(const variants_iterator& other) const;

    // 解引用操作
    reference operator*() const;
    reference operator[](difference_type n) const;

    // 比较操作
    bool operator==(const variants_iterator& other) const;
    bool operator!=(const variants_iterator& other) const;
    bool operator<(const variants_iterator& other) const;
    bool operator<=(const variants_iterator& other) const;
    bool operator>(const variants_iterator& other) const;
    bool operator>=(const variants_iterator& other) const;

    // 友元类声明
    friend class variants_const_iterator;

private:
    mc::shared_ptr<i_variants> m_data;
    size_t                     m_index;
};

/**
 * @brief variants 的常量迭代器类
 */
class MC_API variants_const_iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = variant;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const variant*;
    using reference         = const variant_reference;

    // 构造函数
    variants_const_iterator();
    variants_const_iterator(mc::shared_ptr<i_variants> data, size_t index);

    // 从非常量迭代器构造
    variants_const_iterator(const variants_iterator& other);

    // 拷贝构造函数
    variants_const_iterator(const variants_const_iterator& other) = default;

    // 移动构造函数
    variants_const_iterator(variants_const_iterator&& other) noexcept = default;

    // 赋值运算符
    variants_const_iterator& operator=(const variants_const_iterator& other)     = default;
    variants_const_iterator& operator=(variants_const_iterator&& other) noexcept = default;

    // 析构函数
    ~variants_const_iterator() = default;

    // 迭代器操作
    variants_const_iterator& operator++();
    variants_const_iterator  operator++(int);
    variants_const_iterator& operator--();
    variants_const_iterator  operator--(int);

    variants_const_iterator& operator+=(difference_type n);
    variants_const_iterator& operator-=(difference_type n);

    variants_const_iterator operator+(difference_type n) const;
    variants_const_iterator operator-(difference_type n) const;
    difference_type         operator-(const variants_const_iterator& other) const;

    // 解引用操作
    reference operator*() const;
    reference operator[](difference_type n) const;

    // 比较操作
    bool operator==(const variants_const_iterator& other) const;
    bool operator!=(const variants_const_iterator& other) const;
    bool operator<(const variants_const_iterator& other) const;
    bool operator<=(const variants_const_iterator& other) const;
    bool operator>(const variants_const_iterator& other) const;
    bool operator>=(const variants_const_iterator& other) const;

private:
    mc::shared_ptr<i_variants> m_data;
    size_t                     m_index;
};

class i_variants : public mc::memory::shared_base {
public:
    virtual ~i_variants() = default;

    // 基本操作
    virtual size_t do_size() const  = 0;
    virtual bool   do_empty() const = 0;

    // 元素访问
    virtual variant do_at(size_t index) const                  = 0;
    virtual void    do_set(size_t index, const variant& value) = 0;
    virtual variant do_front() const                           = 0;
    virtual variant do_back() const                            = 0;

    // 修改操作
    virtual void do_push_back(const variant& value)            = 0;
    virtual void do_pop_back()                                 = 0;
    virtual void do_clear()                                    = 0;
    virtual void do_insert(size_t pos, const variant& value)   = 0;
    virtual void do_erase(size_t pos)                          = 0;
    virtual void do_erase(size_t first, size_t last)           = 0;
    virtual void do_assign(size_t count, const variant& value) = 0;

    // 容量管理
    virtual void   do_reserve(size_t new_cap)                    = 0;
    virtual void   do_resize(size_t count)                       = 0;
    virtual void   do_resize(size_t count, const variant& value) = 0;
    virtual size_t do_capacity() const                           = 0;
    virtual size_t do_max_size() const                           = 0;
    virtual void   do_shrink_to_fit()                            = 0;

    // 迭代器支持
    virtual void do_for_each(std::function<void(const variant&)> visitor) const = 0;

    // 类型信息
    virtual std::type_index element_type() const      = 0;
    virtual std::string     element_type_name() const = 0;

    // 引用访问支持（用于 variant_reference）
    virtual bool           supports_reference_access() const = 0;
    virtual variant*       get_ptr(size_t index)             = 0;
    virtual const variant* get_ptr(size_t index) const       = 0;

    // 拷贝操作
    virtual mc::shared_ptr<i_variants> copy() const                                             = 0;
    virtual mc::shared_ptr<i_variants> deep_copy(mc::detail::copy_context* ctx = nullptr) const = 0;
};

class MC_API variants {
public:
    // 类型定义
    using value_type      = variant;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using reference       = variant&;
    using const_reference = const variant&;
    using pointer         = variant*;
    using const_pointer   = const variant*;

    using iterator               = variants_iterator;
    using const_iterator         = variants_const_iterator;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    variants();

    variants(const variants& other)     = default;
    variants(variants&& other) noexcept = default;
    ~variants()                         = default;

    variants& operator=(const variants& other)     = default;
    variants& operator=(variants&& other) noexcept = default;

    // 从其他 variant 构造
    variants(const mc::variant& other);

    // 带分配器的构造函数
    variants(const variants& other, const std::allocator<char>& alloc);
    explicit variants(const std::allocator<char>& alloc);

    // 强类型数组构造
    template <typename T, typename Allocator>
    variants(mc::array<T, Allocator> arr,
             std::enable_if_t<is_variant_constructible_v<T> || is_variant_v<T>>* = nullptr);

    // 初始化列表构造（默认构造为弱类型）
    variants(const std::initializer_list<variant>& list);
    template <typename T>
    variants(const std::initializer_list<T>& list,
             std::enable_if_t<is_variant_constructible_v<T> &&
                              !is_variant_reference_v<T> &&
                              !std::is_same_v<T, variant>>* = nullptr);
    template <typename T, typename Allocator>
    variants(const std::vector<T, Allocator>& vec,
             std::enable_if_t<is_variant_constructible_v<T> || is_variant_v<T>>* = nullptr);
    template <typename T, typename Allocator>
    variants(std::vector<T, Allocator>&& vec,
             std::enable_if_t<is_variant_constructible_v<T> || is_variant_v<T>>* = nullptr);

    /**
     * @brief 特殊构造函数：当 T 是 variant_reference 时，创建 variants
     * @param list variant_reference 初始化列表
     */

    variants(const std::initializer_list<variant_reference>& list);

    /**
     * @brief 特殊构造函数：从 variant_reference 迭代器范围构造
     * @tparam InputIt 输入迭代器类型
     * @param first 起始迭代器
     * @param last 结束迭代器
     */
    template <typename InputIt>
    variants(InputIt first, InputIt last,
             std::enable_if_t<is_variant_reference_v<typename std::iterator_traits<InputIt>::value_type>>* = nullptr);

    explicit variants(mc::shared_ptr<i_variants> data);

    // 基本操作
    size_t size() const;
    bool   empty() const;

    // 元素访问
    variant at(size_t index) const;
    void    set(size_t index, const variant& value);

    variant_reference operator[](size_t index);
    variant           operator[](size_t index) const;

    // 获取可修改的引用
    variant_reference at_ref(size_t index);

    // 访问首尾元素
    variant_reference front();
    variant           front() const;
    variant_reference back();
    variant           back() const;

    // 修改操作
    void push_back(const variant& value);
    void pop_back();
    void clear();

    // assign 方法
    void assign(size_t count, const variant& value);
    template <typename InputIt>
    typename std::enable_if<!std::is_integral<InputIt>::value, void>::type
    assign(InputIt first, InputIt last) {
        ensure_data();
        clear();
        for (auto it = first; it != last; ++it) {
            push_back(*it);
        }
    }
    void assign(std::initializer_list<variant> ilist);

    // swap 方法
    void swap(variants& other) noexcept;

    // 容量管理
    void   reserve(size_t new_cap);
    void   resize(size_t count);
    void   resize(size_t count, const variant& value);
    size_t capacity() const;
    size_t max_size() const;
    void   shrink_to_fit();

    // 迭代器支持
    void for_each(std::function<void(const variant&)> visitor) const;

    // 类型信息
    std::type_index element_type() const;
    std::string     element_type_name() const;

    // 引用访问支持
    bool           supports_reference_access() const;
    variant*       get_ptr(size_t index);
    const variant* get_ptr(size_t index) const;

    // 迭代器支持
    variants_const_iterator begin() const;
    variants_const_iterator end() const;
    variants_iterator       begin();
    variants_iterator       end();
    variants_const_iterator cbegin() const;
    variants_const_iterator cend() const;

    // 反向迭代器支持
    reverse_iterator       rbegin();
    const_reverse_iterator rbegin() const;
    const_reverse_iterator crbegin() const;

    reverse_iterator       rend();
    const_reverse_iterator rend() const;
    const_reverse_iterator crend() const;

    // 容器操作支持
    void emplace_back(variant&& value);
    void emplace_back(const variant& value);

    // 插入操作
    void insert(size_t pos, const variant& value);
    void insert(size_t pos, size_t count, const variant& value);
    template <typename InputIt>
    void insert(size_t pos, InputIt first, InputIt last);

    // 迭代器位置插入
    variants_iterator insert(variants_iterator pos, const variant& value);
    variants_iterator insert(variants_iterator pos, size_t count, const variant& value);
    template <typename InputIt>
    variants_iterator insert(variants_iterator pos, InputIt first, InputIt last);

    // 删除操作
    void              erase(size_t pos);
    void              erase(size_t first, size_t last);
    variants_iterator erase(variants_const_iterator pos);
    variants_iterator erase(variants_const_iterator first, variants_const_iterator last);

    // 拷贝操作
    variants copy() const;

    // 比较运算符
    bool operator==(const variants& other) const;
    bool operator!=(const variants& other) const;
    bool operator<(const variants& other) const;
    bool operator>(const variants& other) const;
    bool operator<=(const variants& other) const;
    bool operator>=(const variants& other) const;

    // 深拷贝支持
    variants deep_copy(mc::detail::copy_context* ctx = nullptr) const;

    const i_variants* data() const;
    i_variants*       data();

    template <typename T>
    T* as() const {
        if (!m_data) {
            return nullptr;
        }
        return dynamic_cast<T*>(m_data.get());
    }

private:
    void ensure_data();

    mc::shared_ptr<i_variants> m_data;
};
} // namespace mc

#endif // MC_VARIANTS_H
