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

#include "mc/shm/container/list.h"

#include <cstring>

// list 的非模板核心实现
// 所有公开函数假设调用者持有 ctrl.mutex（由 container_guard 管理）

namespace mc::shm::container {

namespace {

using detail::list_node_common;
using detail::from_offset;
using detail::self_offset_from;

// misc 字段位标：编码插入 / 删除时 anchor_prev / anchor_next 是否为 0
constexpr std::uint32_t flag_at_head = 0x1U; // anchor_prev == 0
constexpr std::uint32_t flag_at_tail = 0x2U; // anchor_next == 0

list_node_common* node_at(list_control& ctrl, std::int64_t off) noexcept
{
    if (off == 0) {
        return nullptr;
    }
    return static_cast<list_node_common*>(from_offset(&ctrl, off));
}

// ----- recover 子函数 -----

void recover_insert(list_control& ctrl, const container_journal_view& v) noexcept
{
    auto* target = node_at(ctrl, static_cast<std::int64_t>(v.target_node_offset));
    if (target == nullptr) {
        return;
    }
    if (!node_header_check(target->header)) {
        node_header_mark_degraded(target->header);
        ctrl.degraded_count.fetch_add(1, std::memory_order_acq_rel);
        return;
    }

    const auto state = node_header_load_state(target->header);
    if (state == node_state::linked) {
        // finish：链接已建好，size 可能已 +1 或未；fsck 将重算 size
        return;
    }
    if (state != node_state::pending) {
        // free / degraded：视为异常，isolate
        node_header_mark_degraded(target->header);
        ctrl.degraded_count.fetch_add(1, std::memory_order_acq_rel);
        return;
    }

    // rollback：还原 anchor_prev / anchor_next 的链接以及 head/tail
    const auto anchor_prev_off = static_cast<std::int64_t>(v.anchor_offset);
    const auto anchor_next_off = static_cast<std::int64_t>(v.extra);
    const auto saved_a         = static_cast<std::int64_t>(v.saved_link_a);
    const auto saved_b         = static_cast<std::int64_t>(v.saved_link_b);

    if ((v.misc & flag_at_head) != 0) {
        ctrl.head_offset = saved_a;
    } else if (auto* anchor_prev = node_at(ctrl, anchor_prev_off); anchor_prev != nullptr) {
        anchor_prev->next_offset = saved_a;
    }

    if ((v.misc & flag_at_tail) != 0) {
        ctrl.tail_offset = saved_b;
    } else if (auto* anchor_next = node_at(ctrl, anchor_next_off); anchor_next != nullptr) {
        anchor_next->prev_offset = saved_b;
    }

    // target 的 allocator slot 暂未归还；保守 mark degraded，由后续 GC 回收
    node_header_mark_degraded(target->header);
    ctrl.degraded_count.fetch_add(1, std::memory_order_acq_rel);
}

void recover_erase(list_control& ctrl, const container_journal_view& v) noexcept
{
    auto* target = node_at(ctrl, static_cast<std::int64_t>(v.target_node_offset));
    if (target == nullptr) {
        return;
    }
    if (!node_header_check(target->header)) {
        node_header_mark_degraded(target->header);
        ctrl.degraded_count.fetch_add(1, std::memory_order_acq_rel);
        return;
    }

    const auto state      = node_header_load_state(target->header);
    const auto target_off = static_cast<std::int64_t>(v.target_node_offset);
    const auto prev_off   = static_cast<std::int64_t>(v.saved_link_a);
    const auto next_off   = static_cast<std::int64_t>(v.saved_link_b);

    if (state == node_state::pending) {
        // rollback：把 target 重新挂回链表
        if ((v.misc & flag_at_head) != 0) {
            ctrl.head_offset = target_off;
        } else if (auto* prev = node_at(ctrl, prev_off); prev != nullptr) {
            prev->next_offset = target_off;
        }
        if ((v.misc & flag_at_tail) != 0) {
            ctrl.tail_offset = target_off;
        } else if (auto* next = node_at(ctrl, next_off); next != nullptr) {
            next->prev_offset = target_off;
        }
        node_header_transition(target->header, node_state::linked);
        return;
    }

    if (state == node_state::free) {
        // finish：链接已解除；target 的 allocator slot 没有归还，leak（GC 处理）
        // size 可能已 -1 或未，fsck 会重扫
        return;
    }

    node_header_mark_degraded(target->header);
    ctrl.degraded_count.fetch_add(1, std::memory_order_acq_rel);
}

// fsck: 遍历容器链并校验每个节点；损坏节点 isolate；重算 size
// 限制扫描轮数防御环
void fsck_scan(list_control& ctrl) noexcept
{
    std::size_t        counted         = 0;
    std::size_t        newly_degraded  = 0;
    std::int64_t       prev_off        = 0;
    std::int64_t       current_off     = ctrl.head_offset;
    const std::size_t  hint_size       = ctrl.size.load(std::memory_order_acquire);
    const std::size_t  scan_limit      = hint_size * 2 + 1024;

    while (current_off != 0) {
        if (counted + newly_degraded > scan_limit) {
            // 疑似环形 / 结构严重破坏：截断并重设 tail
            ctrl.tail_offset = prev_off;
            if (prev_off != 0) {
                if (auto* p = node_at(ctrl, prev_off); p != nullptr) {
                    p->next_offset = 0;
                }
            } else {
                ctrl.head_offset = 0;
            }
            break;
        }

        auto* node = node_at(ctrl, current_off);
        if (node == nullptr) {
            break;
        }

        const bool header_ok = node_header_check(node->header)
                               && node->header.magic == list_node_magic
                               && node->header.owner_offset == ctrl.self_tag;
        const bool state_ok  = header_ok
                               && node_header_load_state(node->header) == node_state::linked;
        const bool link_ok   = state_ok && node->prev_offset == prev_off;

        if (!header_ok || !state_ok || !link_ok) {
            // 尽量保留 next 指针 —— 即使 header 受损，next_offset 字段本身往往仍
            // 可读；scan_limit 保护环 / 野指针情形
            const std::int64_t next_off = node->next_offset;

            // isolate: 把当前节点从链上摘除
            if (prev_off == 0) {
                ctrl.head_offset = next_off;
            } else if (auto* prev = node_at(ctrl, prev_off); prev != nullptr) {
                prev->next_offset = next_off;
            }
            if (next_off == 0) {
                ctrl.tail_offset = prev_off;
            } else if (auto* next = node_at(ctrl, next_off); next != nullptr) {
                next->prev_offset = prev_off;
            }

            if (header_ok) {
                node_header_mark_degraded(node->header);
            }
            ++newly_degraded;
            current_off = next_off;
            continue;
        }

        prev_off    = current_off;
        current_off = node->next_offset;
        ++counted;
    }

    ctrl.size.store(counted, std::memory_order_release);
    if (newly_degraded > 0) {
        ctrl.degraded_count.fetch_add(newly_degraded, std::memory_order_acq_rel);
    }
}

} // namespace

void list_control_init(list_control& ctrl, std::uint32_t self_tag, std::size_t element_size,
                       std::size_t element_align) noexcept
{
    (void)element_align; // 记在 hint 里，真实对齐由 caller 给 shm_allocator
    journal_init(ctrl.journal);
    ctrl.head_offset = 0;
    ctrl.tail_offset = 0;
    ctrl.node_size   = sizeof(list_node_common) + element_size;
    ctrl.self_tag    = self_tag;
    ctrl._pad0       = 0;
    ctrl.size.store(0, std::memory_order_release);
    ctrl.degraded_count.store(0, std::memory_order_release);
    std::memset(ctrl._reserved, 0, sizeof(ctrl._reserved));
}

void list_recover(list_control& ctrl) noexcept
{
    container_journal_view view{};
    if (journal_load(ctrl.journal, view)) {
        switch (view.op) {
            case container_op::insert:
                recover_insert(ctrl, view);
                break;
            case container_op::erase:
                recover_erase(ctrl, view);
                break;
            case container_op::update:
            case container_op::none:
            default:
                break;
        }
        journal_end(ctrl.journal);
    }

    fsck_scan(ctrl);
}

std::size_t list_degraded_count(const list_control& ctrl) noexcept
{
    return ctrl.degraded_count.load(std::memory_order_acquire);
}

namespace detail {

void list_insert_after(list_control& ctrl, std::int64_t anchor_prev_offset,
                       list_node_common* new_node) noexcept
{
    const std::int64_t new_off = self_offset_from(&ctrl, new_node);

    list_node_common* anchor_prev      = node_at(ctrl, anchor_prev_offset);
    const std::int64_t anchor_next_off = (anchor_prev == nullptr) ? ctrl.head_offset
                                                                  : anchor_prev->next_offset;
    list_node_common*  anchor_next     = node_at(ctrl, anchor_next_off);

    // saved_a = 被修改前的 anchor_prev.next（或旧 head）
    // saved_b = 被修改前的 anchor_next.prev（或旧 tail）
    const std::int64_t saved_a = (anchor_prev == nullptr) ? ctrl.head_offset
                                                          : anchor_prev->next_offset;
    const std::int64_t saved_b = (anchor_next == nullptr) ? ctrl.tail_offset
                                                          : anchor_next->prev_offset;

    std::uint32_t flags = 0;
    if (anchor_prev_offset == 0) {
        flags |= flag_at_head;
    }
    if (anchor_next_off == 0) {
        flags |= flag_at_tail;
    }

    // Step A：new_node state → pending
    node_header_transition(new_node->header, node_state::pending);

    // Step B：journal_begin
    journal_begin(ctrl.journal, container_op::insert, static_cast<std::uint64_t>(new_off),
                  static_cast<std::uint64_t>(anchor_prev_offset),
                  static_cast<std::uint64_t>(saved_a), static_cast<std::uint64_t>(saved_b),
                  static_cast<std::uint64_t>(anchor_next_off), flags);

    // Step C：实际改链接
    new_node->prev_offset = anchor_prev_offset;
    new_node->next_offset = anchor_next_off;
    if (anchor_next != nullptr) {
        anchor_next->prev_offset = new_off;
    } else {
        ctrl.tail_offset = new_off;
    }
    if (anchor_prev != nullptr) {
        anchor_prev->next_offset = new_off;
    } else {
        ctrl.head_offset = new_off;
    }

    // Step D：state → linked
    node_header_transition(new_node->header, node_state::linked);

    // Step E：size ++
    ctrl.size.fetch_add(1, std::memory_order_acq_rel);

    // Step F：journal_end
    journal_end(ctrl.journal);
}

bool list_unlink(list_control& ctrl, list_node_common* node) noexcept
{
    if (node == nullptr || !node_header_check(node->header)) {
        return false;
    }
    if (node->header.magic != list_node_magic || node->header.owner_offset != ctrl.self_tag) {
        return false;
    }
    if (node_header_load_state(node->header) != node_state::linked) {
        return false;
    }

    const std::int64_t target_off = self_offset_from(&ctrl, node);
    const std::int64_t prev_off   = node->prev_offset;
    const std::int64_t next_off   = node->next_offset;
    list_node_common*  prev_node  = node_at(ctrl, prev_off);
    list_node_common*  next_node  = node_at(ctrl, next_off);

    std::uint32_t flags = 0;
    if (prev_off == 0) {
        flags |= flag_at_head;
    }
    if (next_off == 0) {
        flags |= flag_at_tail;
    }

    // Step A
    node_header_transition(node->header, node_state::pending);

    // Step B
    journal_begin(ctrl.journal, container_op::erase, static_cast<std::uint64_t>(target_off),
                  static_cast<std::uint64_t>(prev_off), static_cast<std::uint64_t>(prev_off),
                  static_cast<std::uint64_t>(next_off), /*extra=*/0, flags);

    // Step C
    if (prev_node != nullptr) {
        prev_node->next_offset = next_off;
    } else {
        ctrl.head_offset = next_off;
    }
    if (next_node != nullptr) {
        next_node->prev_offset = prev_off;
    } else {
        ctrl.tail_offset = prev_off;
    }

    // Step D：target → free（链接已解除，等待 caller 归还 allocator slot）
    node_header_transition(node->header, node_state::free);

    // Step E
    ctrl.size.fetch_sub(1, std::memory_order_acq_rel);

    // Step F
    journal_end(ctrl.journal);
    return true;
}

// ---- 非模板 helper（list<T> 模板 wrapper 统一调用）----

void list_recover_cb(void* data) noexcept
{
    list_recover(*static_cast<list_control*>(data));
}

list_node_common* list_prepare_node(list_control& ctrl, shm_allocator& alloc,
                                    std::size_t node_size, std::size_t node_align) noexcept
{
    void* mem = alloc.allocate(node_size, node_align);
    if (mem == nullptr) {
        return nullptr;
    }
    auto* node = static_cast<list_node_common*>(mem);
    node_header_init(node->header, list_node_magic, ctrl.self_tag);
    node->prev_offset = 0;
    node->next_offset = 0;
    return node;
}

void list_finish_push(list_control& ctrl, bool at_front, list_node_common* node) noexcept
{
    const std::int64_t anchor_prev = at_front ? 0 : ctrl.tail_offset;
    list_insert_after(ctrl, anchor_prev, node);
}

list_node_common* list_take_head(list_control& ctrl) noexcept
{
    const auto head_off = ctrl.head_offset;
    if (head_off == 0) {
        return nullptr;
    }
    auto* head = static_cast<list_node_common*>(from_offset(&ctrl, head_off));
    if (!list_unlink(ctrl, head)) {
        return nullptr;
    }
    return head;
}

list_node_common* list_take_tail(list_control& ctrl) noexcept
{
    const auto tail_off = ctrl.tail_offset;
    if (tail_off == 0) {
        return nullptr;
    }
    auto* tail = static_cast<list_node_common*>(from_offset(&ctrl, tail_off));
    if (!list_unlink(ctrl, tail)) {
        return nullptr;
    }
    return tail;
}

bool list_validate_and_unlink(list_control& ctrl, list_node_common* node) noexcept
{
    if (node == nullptr) {
        return false;
    }
    if (!node_header_check(node->header) || node->header.magic != list_node_magic
        || node->header.owner_offset != ctrl.self_tag) {
        return false;
    }
    return list_unlink(ctrl, node);
}

} // namespace detail

} // namespace mc::shm::container
