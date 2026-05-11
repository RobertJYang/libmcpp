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
 * @brief 侵入式容器钩子类型
 */
#ifndef MC_INTRUSIVE_HOOK_H
#define MC_INTRUSIVE_HOOK_H

#include <cstddef>

#include <mc/intrusive/offset_ptr.h>

namespace mc::intrusive {

// --- Option tags ---

template <bool Enabled>
struct constant_time_size {};

enum link_mode_type { safe_link, auto_unlink, normal_link };

template <link_mode_type Mode>
struct link_mode {};

// --- Hook states ---

namespace detail {

struct list_hook_state {
    mc::intrusive::offset_ptr<list_hook_state> prev{};
    mc::intrusive::offset_ptr<list_hook_state> next{};
};

struct slist_hook_state {
    mc::intrusive::offset_ptr<slist_hook_state> next{};
};

struct set_hook_state {
    mc::intrusive::offset_ptr<set_hook_state> parent{};
    mc::intrusive::offset_ptr<set_hook_state> left{};
    mc::intrusive::offset_ptr<set_hook_state> right{};
    bool                                      is_red{true};
};

struct unordered_hook_state {
    mc::intrusive::offset_ptr<unordered_hook_state> next{};
    mc::intrusive::offset_ptr<unordered_hook_state> prev{};
    std::size_t                                     hash_value{0};
};

} // namespace detail

// --- Base hooks ---

template <typename... Options>
struct list_base_hook : detail::list_hook_state {};

template <typename... Options>
struct list_member_hook : detail::list_hook_state {};

template <typename... Options>
struct slist_base_hook : detail::slist_hook_state {};

template <typename... Options>
struct slist_member_hook : detail::slist_hook_state {};

template <typename... Options>
struct set_base_hook : detail::set_hook_state {};

template <typename... Options>
struct set_member_hook : detail::set_hook_state {};

template <typename... Options>
struct unordered_set_base_hook : detail::unordered_hook_state {};

template <typename... Options>
struct unordered_set_member_hook : detail::unordered_hook_state {};

} // namespace mc::intrusive

#endif // MC_INTRUSIVE_HOOK_H