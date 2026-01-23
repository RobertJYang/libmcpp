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
 * @file array.h
 * @brief 定义了引用语义的数组类 array，用于表示动态数组
 */
#ifndef MC_ARRAY_H
#define MC_ARRAY_H

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <vector>

#include <mc/common.h>
#include <mc/memory.h>
#include <mc/pretty_name.h>
#include <mc/traits.h>
#include <mc/variant/variants.h>

namespace mc {

namespace detail {

/**
 * @brief 抛出索引越界异常
 *
 * @param msg 异常消息
 */
[[noreturn]] MC_API void throw_array_out_of_range(const char* msg);

/**
 * @brief array 的内部实现类
 *
 * @tparam T 元素类型
 * @tparam Allocator 分配器类型
 */
template <typename T, typename Allocator = std::allocator<T>>
class array_impl : public mc::i_variants, public std::vector<T, Allocator> {
public:
    using base_type = std::vector<T, Allocator>;

    // 继承所有 std::vector 的构造函数
    using base_type::base_type;

    // 默认构造函数
    array_impl() : base_type() {
    }

    // 从 allocator 构造
    explicit array_impl(const Allocator& alloc) : base_type(alloc) {
    }

    // 从 std::vector 拷贝构造
    array_impl(const base_type& vec) : base_type(vec) {
    }

    // 从 std::vector 移动构造
    array_impl(base_type&& vec) noexcept : base_type(std::move(vec)) {
    }

    // 拷贝构造函数
    array_impl(const array_impl& other) : base_type(static_cast<const base_type&>(other)) {
    }

    // 移动构造函数
    array_impl(array_impl&& other) noexcept : base_type(std::move(static_cast<base_type&&>(other))) {
    }

    ~array_impl() {
    }

    // 赋值运算符
    array_impl& operator=(const array_impl& other) {
        base_type::operator=(static_cast<const base_type&>(other));
        return *this;
    }

    array_impl& operator=(array_impl&& other) noexcept {
        base_type::operator=(std::move(static_cast<base_type&&>(other)));
        return *this;
    }

    // 实现 mc::i_variants 接口
    size_t do_size() const override {
        return base_type::size();
    }

    bool do_empty() const override {
        return base_type::empty();
    }

    void do_clear() override {
        base_type::clear();
    }

    void do_reserve(size_t new_cap) override {
        base_type::reserve(new_cap);
    }

    void do_resize(size_t count) override {
        base_type::resize(count);
    }

    std::type_index element_type() const override {
        return typeid(T);
    }

    std::string element_type_name() const override {
        return mc::pretty_name<T>();
    }

    variant        do_at(size_t index) const override;
    void           do_set(size_t index, const variant& value) override;
    variant        do_front() const override;
    variant        do_back() const override;
    void           do_push_back(const variant& value) override;
    void           do_pop_back() override;
    void           do_insert(size_t pos, const variant& value) override;
    void           do_erase(size_t pos) override;
    void           do_erase(size_t first, size_t last) override;
    void           do_assign(size_t count, const variant& value) override;
    void           do_resize(size_t count, const variant& value) override;
    size_t         do_capacity() const override;
    size_t         do_max_size() const override;
    void           do_shrink_to_fit() override;
    void           do_for_each(std::function<void(const variant&)> visitor) const override;
    bool           supports_reference_access() const override;
    variant*       get_ptr(size_t index) override;
    const variant* get_ptr(size_t index) const override;

    // 拷贝操作
    mc::shared_ptr<i_variants> copy() const override;
    mc::shared_ptr<i_variants> deep_copy(mc::detail::copy_context* ctx = nullptr) const override;
};
} // namespace detail

/**
 * @brief 引用语义的数组类，内部使用共享指针管理数据
 *
 * @tparam T 元素类型
 * @tparam Allocator 分配器类型
 *
 * @note 此类使用共享数据模型，拷贝操作会共享内部数据。
 *       多个 array 对象可能共享相同的内部数据，
 *       修改共享数据会影响所有共享该数据的对象。
 */
template <typename T, typename Allocator = std::allocator<T>>
class array {
public:
    using impl_type              = detail::array_impl<T, Allocator>;
    using vector_type            = std::vector<T, Allocator>;
    using value_type             = typename vector_type::value_type;
    using allocator_type         = typename vector_type::allocator_type;
    using size_type              = typename vector_type::size_type;
    using difference_type        = typename vector_type::difference_type;
    using reference              = typename vector_type::reference;
    using const_reference        = typename vector_type::const_reference;
    using pointer                = typename vector_type::pointer;
    using const_pointer          = typename vector_type::const_pointer;
    using iterator               = typename vector_type::iterator;
    using const_iterator         = typename vector_type::const_iterator;
    using reverse_iterator       = typename vector_type::reverse_iterator;
    using const_reverse_iterator = typename vector_type::const_reverse_iterator;

    // 不支持 variant_reference 数组，因为 variant_reference 是用于访问 variant 的元素的
    static_assert(!is_variant_reference_v<T>, "T must not be a variant_reference type");

    /**
     * @brief 默认构造函数
     */
    array() = default;

    /**
     * @brief 从分配器构造空数组
     * @param alloc 分配器
     */
    explicit array(const Allocator& alloc) : m_data(mc::make_shared<impl_type>(alloc)) {
    }

    /**
     * @brief 构造包含 count 个默认值元素的数组
     * @param count 元素数量
     * @param alloc 分配器
     */
    explicit array(size_type count, const Allocator& alloc = Allocator())
        : m_data(mc::make_shared<impl_type>(count, alloc)) {
    }

    /**
     * @brief 构造包含 count 个指定值元素的数组
     * @param count 元素数量
     * @param value 元素值
     * @param alloc 分配器
     */
    array(size_type count, const T& value, const Allocator& alloc = Allocator())
        : m_data(mc::make_shared<impl_type>(count, value, alloc)) {
    }

    /**
     * @brief 从迭代器范围构造
     * @tparam InputIt 输入迭代器类型
     * @param first 起始迭代器
     * @param last 结束迭代器
     * @param alloc 分配器
     */
    template <typename InputIt>
    array(InputIt first, InputIt last, const Allocator& alloc = Allocator(),
          std::enable_if_t<!is_variant_reference_v<typename std::iterator_traits<InputIt>::value_type>>* = nullptr)
        : m_data(mc::make_shared<impl_type>(first, last, alloc)) {
    }

    /**
     * @brief 拷贝构造函数
     * @note 此操作会共享内部数据，不会复制数据
     */
    array(const array& other) = default;

    /**
     * @brief 从另一个 array 拷贝构造（带 allocator）
     * @param other 源 array
     * @param alloc 分配器（忽略，因为共享数据）
     * @note 此操作会共享内部数据，不会复制数据，allocator 参数被忽略
     */
    array(const array& other, const Allocator& alloc) : m_data(other.m_data) {
    }

    /**
     * @brief 移动构造函数
     */
    array(array&& other) noexcept = default;

    /**
     * @brief 从初始化列表构造
     * @param init 初始化列表
     * @param alloc 分配器
     */
    array(std::initializer_list<T> init, const Allocator& alloc = Allocator())
        : m_data(mc::make_shared<impl_type>(init, alloc)) {
    }

    /**
     * @brief 从 std::vector 构造
     * @param vec 源 vector
     * @param alloc 分配器
     */
    array(const vector_type& vec, const Allocator& alloc = Allocator())
        : m_data(mc::make_shared<impl_type>(vec)) {
    }

    /**
     * @brief 从 std::vector 移动构造
     * @param vec 源 vector
     * @param alloc 分配器
     */
    array(vector_type&& vec, const Allocator& alloc = Allocator())
        : m_data(mc::make_shared<impl_type>(std::move(vec))) {
    }

    /**
     * @brief 特殊构造函数：当 T 是 variant_reference 时，创建 array<variant>
     * @param init variant_reference 初始化列表
     * @param alloc 分配器
     */
    array(const std::initializer_list<variant_reference>& init,
          const Allocator&                                alloc = Allocator());

    /**
     * @brief 特殊构造函数：从 variant_reference 迭代器范围构造
     * @tparam InputIt 输入迭代器类型
     * @param first 起始迭代器
     * @param last 结束迭代器
     * @param alloc 分配器
     */
    template <typename InputIt>
    array(InputIt first, InputIt last,
          const Allocator& alloc                                                                        = Allocator(),
          std::enable_if_t<is_variant_reference_v<typename std::iterator_traits<InputIt>::value_type>>* = nullptr);

    /**
     * @brief 析构函数
     */
    ~array() = default;

    /**
     * @brief 赋值运算符
     * @note 此操作会共享内部数据，不会复制数据
     */
    array& operator=(const array& other) = default;

    /**
     * @brief 移动赋值运算符
     */
    array& operator=(array&& other) noexcept = default;

    /**
     * @brief 从初始化列表赋值
     * @param ilist 初始化列表
     * @return 返回自身引用
     */
    array& operator=(std::initializer_list<T> ilist) {
        ensure_data();
        *m_data = ilist;
        return *this;
    }

    /**
     * @brief 用指定数量的元素替换容器内容
     *
     * @param count 新的大小
     * @param value 用来初始化新元素的值
     */
    void assign(size_type count, const T& value) {
        ensure_data();
        m_data->assign(count, value);
    }

    /**
     * @brief 用迭代器范围内的元素替换容器内容
     *
     * @tparam InputIt 输入迭代器类型
     * @param first 范围的开始
     * @param last 范围的结束
     */
    template <typename InputIt>
    void assign(InputIt first, InputIt last) {
        ensure_data();
        m_data->assign(first, last);
    }

    /**
     * @brief 用初始化列表的元素替换容器内容
     *
     * @param ilist 初始化列表
     */
    void assign(std::initializer_list<T> ilist) {
        ensure_data();
        m_data->assign(ilist);
    }

    // 元素访问

    /**
     * @brief 访问指定位置的元素
     * @param pos 元素位置
     * @return 元素引用
     * @throw mc::out_of_range_exception 如果 pos 越界
     */
    reference at(size_type pos) {
        ensure_data();
        try {
            auto* impl = static_cast<detail::array_impl<T>*>(m_data.get());
            return static_cast<std::vector<T, Allocator>&>(*impl).at(pos);
        } catch (const std::out_of_range&) {
            detail::throw_array_out_of_range("array::at: index out of range");
        }
    }

    const_reference at(size_type pos) const {
        if (!m_data) {
            detail::throw_array_out_of_range("array::at: index out of range");
        }
        try {
            const auto* impl = static_cast<const detail::array_impl<T>*>(m_data.get());
            return static_cast<const std::vector<T, Allocator>&>(*impl).at(pos);
        } catch (const std::out_of_range&) {
            detail::throw_array_out_of_range("array::at: index out of range");
        }
    }

    /**
     * @brief 访问指定位置的元素（不进行边界检查）
     * @param pos 元素位置
     * @return 元素引用
     */
    reference operator[](size_type pos) {
        ensure_data();
        return (*m_data)[pos];
    }

    const_reference operator[](size_type pos) const {
        if (!m_data) {
            detail::throw_array_out_of_range("array: index out of range");
        }
        return (*m_data)[pos];
    }

    /**
     * @brief 访问第一个元素
     * @return 第一个元素的引用
     */
    reference front() {
        ensure_data();
        return m_data->front();
    }

    const_reference front() const {
        if (!m_data) {
            detail::throw_array_out_of_range("array: array is empty");
        }
        return m_data->front();
    }

    /**
     * @brief 访问最后一个元素
     * @return 最后一个元素的引用
     */
    reference back() {
        ensure_data();
        return m_data->back();
    }

    const_reference back() const {
        if (!m_data) {
            detail::throw_array_out_of_range("array: array is empty");
        }
        return m_data->back();
    }

    /**
     * @brief 获取底层数组的指针
     * @return 指向底层数组的指针
     */
    T* data() noexcept {
        return m_data ? m_data->data() : nullptr;
    }

    const T* data() const noexcept {
        return m_data ? m_data->data() : nullptr;
    }

    // 迭代器

    /**
     * @brief 获取指向第一个元素的迭代器
     */
    iterator begin() {
        ensure_data();
        return m_data->begin();
    }

    const_iterator begin() const {
        return m_data ? m_data->begin() : const_iterator();
    }

    const_iterator cbegin() const {
        return begin();
    }

    /**
     * @brief 获取指向末尾的迭代器
     */
    iterator end() {
        ensure_data();
        return m_data->end();
    }

    const_iterator end() const {
        return m_data ? m_data->end() : const_iterator();
    }

    const_iterator cend() const {
        return end();
    }

    /**
     * @brief 获取反向迭代器
     */
    reverse_iterator rbegin() {
        ensure_data();
        return m_data->rbegin();
    }

    const_reverse_iterator rbegin() const {
        return m_data ? m_data->rbegin() : const_reverse_iterator();
    }

    const_reverse_iterator crbegin() const {
        return rbegin();
    }

    reverse_iterator rend() {
        ensure_data();
        return m_data->rend();
    }

    const_reverse_iterator rend() const {
        return m_data ? m_data->rend() : const_reverse_iterator();
    }

    const_reverse_iterator crend() const {
        return rend();
    }

    // 容量

    /**
     * @brief 检查数组是否为空
     * @return 如果数组为空则返回 true
     */
    bool empty() const noexcept {
        return !m_data || m_data->empty();
    }

    /**
     * @brief 获取元素数量
     * @return 元素数量
     */
    size_type size() const noexcept {
        return m_data ? m_data->size() : 0;
    }

    /**
     * @brief 获取最大可能的元素数量
     * @return 最大元素数量
     */
    size_type max_size() const noexcept {
        return m_data ? m_data->max_size() : vector_type().max_size();
    }

    /**
     * @brief 预留存储空间
     * @param new_cap 新容量
     */
    void reserve(size_type new_cap) {
        ensure_data();
        m_data->reserve(new_cap);
    }

    /**
     * @brief 获取当前容量
     * @return 当前容量
     */
    size_type capacity() const noexcept {
        return m_data ? m_data->capacity() : 0;
    }

    /**
     * @brief 释放未使用的内存
     */
    void shrink_to_fit() {
        if (m_data) {
            m_data->shrink_to_fit();
        }
    }

    // 修改器

    /**
     * @brief 清空所有元素
     * @note 此操作会修改共享数据，影响所有共享该数据的对象
     */
    void clear() {
        if (m_data) {
            m_data->clear();
        }
    }

    /**
     * @brief 在指定位置插入元素
     * @param pos 插入位置
     * @param value 要插入的值
     * @return 指向插入元素的迭代器
     */
    iterator insert(const_iterator pos, const T& value) {
        ensure_data();
        auto* impl = static_cast<detail::array_impl<T>*>(m_data.get());
        auto  it   = static_cast<std::vector<T, Allocator>&>(*impl).insert(pos, value);
        return it;
    }

    iterator insert(const_iterator pos, T&& value) {
        ensure_data();
        auto* impl = static_cast<detail::array_impl<T>*>(m_data.get());
        auto  it   = static_cast<std::vector<T, Allocator>&>(*impl).insert(pos, std::move(value));
        return it;
    }

    /**
     * @brief 在指定位置插入 count 个元素
     * @param pos 插入位置
     * @param count 插入数量
     * @param value 要插入的值
     * @return 指向第一个插入元素的迭代器
     */
    iterator insert(const_iterator pos, size_type count, const T& value) {
        ensure_data();
        auto* impl = static_cast<detail::array_impl<T>*>(m_data.get());
        auto  it   = static_cast<std::vector<T, Allocator>&>(*impl).insert(pos, count, value);
        return it;
    }

    /**
     * @brief 在指定位置插入迭代器范围内的元素
     * @tparam InputIt 输入迭代器类型
     * @param pos 插入位置
     * @param first 范围起始
     * @param last 范围结束
     * @return 指向第一个插入元素的迭代器
     */
    template <typename InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        ensure_data();
        auto* impl = static_cast<detail::array_impl<T>*>(m_data.get());
        auto  it   = static_cast<std::vector<T, Allocator>&>(*impl).insert(pos, first, last);
        return it;
    }

    /**
     * @brief 在指定位置插入初始化列表
     * @param pos 插入位置
     * @param ilist 初始化列表
     * @return 指向第一个插入元素的迭代器
     */
    iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
        ensure_data();
        // 直接访问 array_impl<T> 的底层 vector
        auto* impl = static_cast<detail::array_impl<T>*>(m_data.get());
        auto  it   = static_cast<std::vector<T, Allocator>&>(*impl).insert(pos, ilist);
        return it;
    }

    /**
     * @brief 在指定位置原位构造元素
     * @tparam Args 构造参数类型
     * @param pos 插入位置
     * @param args 构造参数
     * @return 指向构造元素的迭代器
     */
    template <typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        ensure_data();
        return m_data->emplace(pos, std::forward<Args>(args)...);
    }

    /**
     * @brief 删除指定位置的元素
     * @param pos 要删除的元素位置
     * @return 指向删除元素之后元素的迭代器
     */
    iterator erase(const_iterator pos) {
        if (!m_data) {
            detail::throw_array_out_of_range("array::erase: array is empty");
        }
        return m_data->erase(pos);
    }

    /**
     * @brief 删除指定范围的元素
     * @param first 范围起始
     * @param last 范围结束
     * @return 指向删除范围之后元素的迭代器
     */
    iterator erase(const_iterator first, const_iterator last) {
        if (!m_data) {
            detail::throw_array_out_of_range("array::erase: array is empty");
        }
        return m_data->erase(first, last);
    }

    /**
     * @brief 在末尾添加元素
     * @param value 要添加的值
     */
    void push_back(const T& value) {
        ensure_data();
        m_data->push_back(value);
    }

    void push_back(T&& value) {
        ensure_data();
        m_data->push_back(std::move(value));
    }

    /**
     * @brief 在末尾原位构造元素
     * @tparam Args 构造参数类型
     * @param args 构造参数
     * @return 对新添加元素的引用
     */
    template <typename... Args>
    reference emplace_back(Args&&... args) {
        ensure_data();
        return m_data->emplace_back(std::forward<Args>(args)...);
    }

    /**
     * @brief 删除末尾元素
     */
    void pop_back() {
        if (m_data) {
            m_data->pop_back();
        }
    }

    /**
     * @brief 调整数组大小
     * @param count 新大小
     */
    void resize(size_type count) {
        ensure_data();
        m_data->resize(count);
    }

    /**
     * @brief 调整数组大小并使用指定值填充
     * @param count 新大小
     * @param value 填充值
     */
    void resize(size_type count, const value_type& value) {
        ensure_data();
        m_data->resize(count, value);
    }

    /**
     * @brief 交换两个数组的内容
     * @param other 另一个数组
     */
    void swap(array& other) noexcept {
        m_data.swap(other.m_data);
    }

    // 比较运算符

    /**
     * @brief 比较两个数组是否相等
     * @param other 另一个数组
     * @return 如果相等则返回 true
     */
    bool operator==(const array& other) const {
        if (m_data == other.m_data) {
            return true;
        }
        if (!m_data || !other.m_data) {
            return empty() && other.empty();
        }
        return *m_data == *other.m_data;
    }

    /**
     * @brief 比较两个数组是否不相等
     * @param other 另一个数组
     * @return 如果不相等则返回 true
     */
    bool operator!=(const array& other) const {
        return !(*this == other);
    }

    /**
     * @brief 字典序比较
     */
    bool operator<(const array& other) const {
        if (!m_data) {
            return other.m_data && !other.m_data->empty();
        }
        if (!other.m_data) {
            return false;
        }
        return *m_data < *other.m_data;
    }

    bool operator<=(const array& other) const {
        return !(other < *this);
    }

    bool operator>(const array& other) const {
        return other < *this;
    }

    bool operator>=(const array& other) const {
        return !(*this < other);
    }

    /**
     * @brief 获取可变引用（为了向后兼容保留）
     * @return 返回自身引用
     * @note array 本身就是可变的，此方法仅为向后兼容而保留
     */
    array& as_mut() {
        return *this;
    }

    /**
     * @brief 获取可变引用（const 版本，为了向后兼容保留）
     * @return 返回自身的可变副本
     * @note array 本身就是可变的，此方法仅为向后兼容而保留
     */
    array as_mut() const {
        return *this;
    }

    /**
     * @brief 拷贝数组（浅拷贝，创建新容器但元素共享）
     * @return 拷贝后的数组
     * @note 委托给 array_impl::copy() 实现
     */
    array copy() const {
        if (!m_data) {
            return array();
        }
        auto copied_base = m_data->copy();
        auto copied_impl = mc::static_pointer_cast<impl_type>(copied_base);
        return array::from_impl(std::move(copied_impl));
    }

    /**
     * @brief 深拷贝数组
     * @param ctx 可选的深拷贝上下文，用于检测循环引用并记录已拷贝对象
     * @return 深度拷贝后的数组
     */
    array deep_copy(mc::detail::copy_context* ctx = nullptr) const {
        if (!m_data) {
            return array();
        }
        auto copied_base = m_data->deep_copy(ctx);
        auto copied_impl = mc::static_pointer_cast<impl_type>(copied_base);
        return array::from_impl(std::move(copied_impl));
    }

    /**
     * @brief 获取内部实现指针
     * @return 内部实现指针
     */
    mc::shared_ptr<impl_type> get_impl() const {
        return m_data;
    }

    /**
     * @brief 从内部实现指针构造 array（仅供内部使用）
     * @param impl 内部实现指针
     * @return array 对象
     */
    static array from_impl(mc::shared_ptr<impl_type> impl) {
        array result;
        result.m_data = std::move(impl);
        return result;
    }

    /**
     * @brief 转换为 std::vector
     * @return std::vector 的拷贝
     */
    vector_type to_vector() const {
        return m_data ? vector_type(*m_data) : vector_type();
    }

private:
    /**
     * @brief 共享的数据指针
     */
    mc::shared_ptr<impl_type> m_data;

    /**
     * @brief 确保数据已初始化
     */
    void ensure_data() {
        if (!m_data) {
            m_data = mc::make_shared<impl_type>();
        }
    }
};

// 特化 std::swap
template <typename T, typename Allocator>
void swap(array<T, Allocator>& lhs, array<T, Allocator>& rhs) noexcept {
    lhs.swap(rhs);
}

// array 与 std::vector 的比较运算符

/**
 * @brief 比较 array 与 std::vector 是否相等
 */
template <typename T1, typename T2, typename Allocator1, typename Allocator2>
auto operator==(const array<T1, Allocator1>& lhs, const std::vector<T2, Allocator2>& rhs)
    -> std::enable_if_t<std::is_same_v<T1, T2> || mc::traits::has_operator_equal_v<T1, T2>, bool> {
    if (lhs.empty() && rhs.empty()) {
        return true;
    }
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
 * @brief 比较 std::vector 与 array 是否相等（支持相同或不同元素类型）
 */
template <typename T1, typename T2, typename Allocator1, typename Allocator2>
auto operator==(const std::vector<T1, Allocator1>& lhs, const array<T2, Allocator2>& rhs)
    -> std::enable_if_t<std::is_same_v<T1, T2> || mc::traits::has_operator_equal_v<T1, T2>, bool> {
    return rhs == lhs;
}

/**
 * @brief 比较 array 与 std::vector 是否不相等
 */
template <typename T1, typename T2, typename Allocator1, typename Allocator2>
auto operator!=(const array<T1, Allocator1>& lhs, const std::vector<T2, Allocator2>& rhs)
    -> std::enable_if_t<std::is_same_v<T1, T2> || mc::traits::has_operator_equal_v<T1, T2>, bool> {
    return !(lhs == rhs);
}

/**
 * @brief 比较 std::vector 与 array 是否不相等
 */
template <typename T1, typename T2, typename Allocator1, typename Allocator2>
auto operator!=(const std::vector<T1, Allocator1>& lhs, const array<T2, Allocator2>& rhs)
    -> std::enable_if_t<std::is_same_v<T1, T2> || mc::traits::has_operator_equal_v<T1, T2>, bool> {
    return !(lhs == rhs);
}

/**
 * @brief 字典序比较 array 与 std::vector
 */
template <typename T, typename Allocator>
bool operator<(const array<T, Allocator>& lhs, const std::vector<T, Allocator>& rhs) {
    if (lhs.empty()) {
        return !rhs.empty();
    }
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

/**
 * @brief 字典序比较 std::vector 与 array
 */
template <typename T, typename Allocator>
bool operator<(const std::vector<T, Allocator>& lhs, const array<T, Allocator>& rhs) {
    if (lhs.empty()) {
        return !rhs.empty();
    }
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

/**
 * @brief 字典序比较 array 与 std::vector (<=)
 */
template <typename T, typename Allocator>
bool operator<=(const array<T, Allocator>& lhs, const std::vector<T, Allocator>& rhs) {
    return !(rhs < lhs);
}

/**
 * @brief 字典序比较 std::vector 与 array (<=)
 */
template <typename T, typename Allocator>
bool operator<=(const std::vector<T, Allocator>& lhs, const array<T, Allocator>& rhs) {
    return !(rhs < lhs);
}

/**
 * @brief 字典序比较 array 与 std::vector (>)
 */
template <typename T, typename Allocator>
bool operator>(const array<T, Allocator>& lhs, const std::vector<T, Allocator>& rhs) {
    return rhs < lhs;
}

/**
 * @brief 字典序比较 std::vector 与 array (>)
 */
template <typename T, typename Allocator>
bool operator>(const std::vector<T, Allocator>& lhs, const array<T, Allocator>& rhs) {
    return rhs < lhs;
}

/**
 * @brief 字典序比较 array 与 std::vector (>=)
 */
template <typename T, typename Allocator>
bool operator>=(const array<T, Allocator>& lhs, const std::vector<T, Allocator>& rhs) {
    return !(lhs < rhs);
}

/**
 * @brief 字典序比较 std::vector 与 array (>=)
 */
template <typename T, typename Allocator>
bool operator>=(const std::vector<T, Allocator>& lhs, const array<T, Allocator>& rhs) {
    return !(lhs < rhs);
}

} // namespace mc

#endif // MC_ARRAY_H
