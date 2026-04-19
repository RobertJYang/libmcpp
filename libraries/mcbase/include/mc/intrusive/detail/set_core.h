/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_INTRUSIVE_DETAIL_SET_CORE_H
#define MC_INTRUSIVE_DETAIL_SET_CORE_H

#include <mc/common.h>
#include <mc/intrusive/hook.h>

#include <cstddef>

namespace mc::intrusive::detail {

using set_compare_fn = bool (*)(const void* a, const void* b, const void* ctx) noexcept;

class MC_API set_core {
public:
    set_core() noexcept = default;

    set_core(const set_core&)            = delete;
    set_core& operator=(const set_core&) = delete;
    set_core(set_core&&)                 = delete;
    set_core& operator=(set_core&&)      = delete;

    set_hook_state*       root() noexcept;
    const set_hook_state* root() const noexcept;

    set_hook_state* insert(set_hook_state* node, set_compare_fn cmp, const void* ctx) noexcept;

    const set_hook_state* find(const void* key, set_compare_fn cmp, const void* ctx) const noexcept;
    set_hook_state*       lower_bound(const void* key, set_compare_fn cmp, const void* ctx) noexcept;
    const set_hook_state* lower_bound(const void* key, set_compare_fn cmp, const void* ctx) const noexcept;
    set_hook_state*       upper_bound(const void* key, set_compare_fn cmp, const void* ctx) noexcept;
    const set_hook_state* upper_bound(const void* key, set_compare_fn cmp, const void* ctx) const noexcept;

    void erase(set_hook_state* node) noexcept;

    void clear() noexcept;

    using set_dispose_fn = void (*)(set_hook_state* node, void* ctx);
    void clear_and_dispose(set_dispose_fn fn, void* ctx);

    void swap(set_core& other) noexcept;

    set_hook_state*       begin() noexcept;
    const set_hook_state* begin() const noexcept;
    set_hook_state*       last() noexcept;
    const set_hook_state* last() const noexcept;

    set_hook_state*       next(set_hook_state* node) noexcept;
    const set_hook_state* next(const set_hook_state* node) const noexcept;
    set_hook_state*       prev(set_hook_state* node) noexcept;
    const set_hook_state* prev(const set_hook_state* node) const noexcept;

    std::size_t count_all() const noexcept;

private:
    set_hook_state* resolve(const offset_ptr<set_hook_state>& ref) const noexcept;
    void            rotate_left(set_hook_state* node, set_hook_state*& root_node) noexcept;
    void            rotate_right(set_hook_state* node, set_hook_state*& root_node) noexcept;
    void            rebalance_after_insert(set_hook_state* node, set_hook_state*& root_node) noexcept;
    void rebalance_after_erase(set_hook_state* node, set_hook_state* parent, set_hook_state*& root_node) noexcept;
    void transplant(set_hook_state* u, set_hook_state* v, set_hook_state*& root_node) noexcept;
    static void reset_hook(set_hook_state* node) noexcept;

    offset_ptr<set_hook_state> m_root{};
};

} // namespace mc::intrusive::detail

#endif // MC_INTRUSIVE_DETAIL_SET_CORE_H
