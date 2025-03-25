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
 * @file options.h
 * @brief 侵入式容器选项
 */
#ifndef MC_INTRUSIVE_OPTIONS_H
#define MC_INTRUSIVE_OPTIONS_H

#include <boost/intrusive/options.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <type_traits>

namespace mc {
namespace intrusive {

/**
 * @brief 常量时间大小选项
 */
using constant_time_size = boost::intrusive::constant_time_size<true>;

/**
 * @brief 成员钩子选项
 *
 * @tparam T 类型
 * @tparam Member 成员类型
 * @tparam MemberPointer 成员指针
 */
template <typename T, typename Member, Member T::* MemberPointer>
using member_hook = boost::intrusive::member_hook<T, Member, MemberPointer>;

/**
 * @brief 基类钩子选项
 *
 * @tparam T 类型
 */
template <typename T>
using base_hook = boost::intrusive::base_hook<T>;

/**
 * @brief 链表成员钩子
 * 
 * 这是一个特殊的钩子，用于支持使用自定义指针类型的侵入式链表
 * 
 * @tparam T 容器元素类型
 * @tparam PointerType 指针类型，默认为裸指针 T*
 * @tparam VoidPointer 内部void指针类型，默认与PointerType相同
 */
template <typename T, typename PointerType = T*, typename VoidPointer = PointerType>
using list_member_hook = boost::intrusive::list_member_hook<
    boost::intrusive::void_pointer<VoidPointer>
>;

/**
 * @brief 缓存最后一个元素选项
 */
using cache_last = boost::intrusive::cache_last<true>;

/**
 * @brief 优化大小选项
 */
using optimize_size = boost::intrusive::optimize_size<true>;

/**
 * @brief 电源2桶选项
 */
using power_2_buckets = boost::intrusive::power_2_buckets<true>;

/**
 * @brief 缓存开始选项
 */
using cache_begin = boost::intrusive::cache_begin<true>;

} // namespace intrusive
} // namespace mc

#endif // MC_INTRUSIVE_OPTIONS_H