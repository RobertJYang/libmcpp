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
 * @file hook.h
 * @brief 封装 Boost 的侵入式钩子类型
 */
#ifndef MC_INTRUSIVE_HOOK_H
#define MC_INTRUSIVE_HOOK_H

#include <boost/intrusive/list_hook.hpp>
#include <boost/intrusive/unordered_set_hook.hpp>
#include <boost/intrusive/options.hpp>

namespace mc {
namespace intrusive {

/**
 * @brief 链接模式枚举
 */
enum class link_mode {
    normal,     ///< 普通模式，不检查钩子状态
    safe,       ///< 安全模式，检查钩子状态
    auto_unlink ///< 自动解链模式，当对象被销毁时自动从容器中移除
};

/**
 * @brief 将内部链接模式映射到 Boost 的链接模式
 */
template <link_mode Mode>
struct boost_link_mode;

template <>
struct boost_link_mode<link_mode::normal> {
    using type = boost::intrusive::link_mode<boost::intrusive::normal_link>;
};

template <>
struct boost_link_mode<link_mode::safe> {
    using type = boost::intrusive::link_mode<boost::intrusive::safe_link>;
};

template <>
struct boost_link_mode<link_mode::auto_unlink> {
    using type = boost::intrusive::link_mode<boost::intrusive::auto_unlink>;
};

/**
 * @brief 链表钩子基类
 * 
 * 该类封装了 Boost 的链表钩子，提供了更加易用的接口
 */
class list_hook : public boost::intrusive::list_base_hook<typename boost_link_mode<link_mode::safe>::type> {
public:
    // 默认构造函数
    list_hook() = default;

    // 拷贝构造函数 - 不复制钩子状态
    list_hook(const list_hook&) : 
        boost::intrusive::list_base_hook<typename boost_link_mode<link_mode::safe>::type>() {}

    // 移动构造函数 - 不移动钩子状态
    list_hook(list_hook&&) noexcept : 
        boost::intrusive::list_base_hook<typename boost_link_mode<link_mode::safe>::type>() {}

    // 禁止赋值操作，因为钩子不支持赋值
    list_hook& operator=(const list_hook&) = delete;
    list_hook& operator=(list_hook&&) = delete;
};

/**
 * @brief 哈希表钩子基类
 * 
 * 该类封装了 Boost 的哈希表钩子，提供了更加易用的接口
 */
class unordered_set_hook : public boost::intrusive::unordered_set_base_hook<typename boost_link_mode<link_mode::safe>::type> {
public:
    // 默认构造函数
    unordered_set_hook() = default;

    // 拷贝构造函数 - 不复制钩子状态
    unordered_set_hook(const unordered_set_hook&) : 
        boost::intrusive::unordered_set_base_hook<typename boost_link_mode<link_mode::safe>::type>() {}

    // 移动构造函数 - 不移动钩子状态
    unordered_set_hook(unordered_set_hook&&) noexcept : 
        boost::intrusive::unordered_set_base_hook<typename boost_link_mode<link_mode::safe>::type>() {}

    // 禁止赋值操作，因为钩子不支持赋值
    unordered_set_hook& operator=(const unordered_set_hook&) = delete;
    unordered_set_hook& operator=(unordered_set_hook&&) = delete;
};

} // namespace intrusive
} // namespace mc

#endif // MC_INTRUSIVE_HOOK_H 