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
 * @file list.h
 * @brief 封装 Boost 的侵入式链表容器
 */
#ifndef MC_INTRUSIVE_LIST_H
#define MC_INTRUSIVE_LIST_H

#include <mc/intrusive/hook.h>
#include <boost/intrusive/list.hpp>
#include <functional>

namespace mc {
namespace intrusive {

/**
 * @brief 侵入式链表容器
 * 
 * @tparam T 元素类型
 * @tparam HookOptions 钩子选项
 */
template <typename T, typename... HookOptions>
class list {
private:
    using boost_list_type = boost::intrusive::list<T, HookOptions...>;

public:
    // 类型定义
    using value_type = typename boost_list_type::value_type;
    using pointer = typename boost_list_type::pointer;
    using const_pointer = typename boost_list_type::const_pointer;
    using reference = typename boost_list_type::reference;
    using const_reference = typename boost_list_type::const_reference;
    using iterator = typename boost_list_type::iterator;
    using const_iterator = typename boost_list_type::const_iterator;
    using reverse_iterator = typename boost_list_type::reverse_iterator;
    using const_reverse_iterator = typename boost_list_type::const_reverse_iterator;
    using size_type = typename boost_list_type::size_type;
    using difference_type = typename boost_list_type::difference_type;

    // 构造函数
    list() = default;

    // 禁止拷贝和移动
    list(const list&) = delete;
    list(list&&) = delete;
    list& operator=(const list&) = delete;
    list& operator=(list&&) = delete;

    // 析构函数
    ~list() = default;

    // 迭代器
    iterator begin() { return m_list.begin(); }
    const_iterator begin() const { return m_list.begin(); }
    const_iterator cbegin() const { return m_list.cbegin(); }
    iterator end() { return m_list.end(); }
    const_iterator end() const { return m_list.end(); }
    const_iterator cend() const { return m_list.cend(); }
    reverse_iterator rbegin() { return m_list.rbegin(); }
    const_reverse_iterator rbegin() const { return m_list.rbegin(); }
    const_reverse_iterator crbegin() const { return m_list.crbegin(); }
    reverse_iterator rend() { return m_list.rend(); }
    const_reverse_iterator rend() const { return m_list.rend(); }
    const_reverse_iterator crend() const { return m_list.crend(); }

    // 容量
    bool empty() const { return m_list.empty(); }
    size_type size() const { return m_list.size(); }

    // 元素访问
    reference front() { return m_list.front(); }
    const_reference front() const { return m_list.front(); }
    reference back() { return m_list.back(); }
    const_reference back() const { return m_list.back(); }

    // 修改器
    void push_front(reference value) { m_list.push_front(value); }
    void pop_front() { m_list.pop_front(); }
    void push_back(reference value) { m_list.push_back(value); }
    void pop_back() { m_list.pop_back(); }
    iterator insert(const_iterator pos, reference value) { return m_list.insert(pos, value); }
    iterator erase(const_iterator pos) { return m_list.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return m_list.erase(first, last); }
    void clear() { m_list.clear(); }

    // 特殊操作
    template <typename Disposer>
    void clear_and_dispose(Disposer disposer) {
        m_list.clear_and_dispose(disposer);
    }

    // 获取迭代器
    iterator iterator_to(reference value) { return m_list.iterator_to(value); }
    const_iterator iterator_to(const_reference value) const { return m_list.iterator_to(value); }

private:
    boost_list_type m_list;
};

} // namespace intrusive
} // namespace mc

#endif // MC_INTRUSIVE_LIST_H 