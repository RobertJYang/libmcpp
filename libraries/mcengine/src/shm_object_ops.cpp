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

#include "shm_object_ops.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

#include <mc/shm/container/journal.h>

namespace mc::engine {

namespace {

// ----------------------------------------------------------------------------
// Journal sub-op 编码（写入 container_journal.anchor_offset）
//
// container_journal.op 始终设为 container_op::update，sub_op 区分语义；
// 这样可以完全复用 mcbase journal_begin / journal_end / journal_load API
// 而不污染 container_op 枚举
// ----------------------------------------------------------------------------

namespace sub_op {
inline constexpr std::uint64_t byte_string_replace   = 100U; // class_name/name/path/position
inline constexpr std::uint64_t offset_ptr_replace    = 110U; // parent/service
inline constexpr std::uint64_t property_value_simple = 200U; // 已存 slot 的 int64/double 覆盖
inline constexpr std::uint64_t property_value_blob   = 201U; // 已存 slot 的 string/bytes 覆盖
inline constexpr std::uint64_t property_slot_create  = 202U; // 新增 slot（写 key+type+value）
inline constexpr std::uint64_t property_slot_remove  = 203U; // 删 slot（swap-and-pop）
inline constexpr std::uint64_t property_slab_replace = 210U; // 扩容：切换 shadow.properties
inline constexpr std::uint64_t child_slot_add        = 300U; // 单 slot push
inline constexpr std::uint64_t child_slot_remove     = 301U; // swap-and-pop
inline constexpr std::uint64_t child_slab_replace    = 310U; // 扩容：切换 shadow.children
} // namespace sub_op

// 计算 b 相对 base 的字节 offset（base 不可为 nullptr）
inline std::uint64_t rel_offset(const void* base, const void* b) noexcept
{
    return static_cast<std::uint64_t>(static_cast<const std::byte*>(b) - static_cast<const std::byte*>(base));
}

// 把 offset_ptr 内部 m_offset 字节复制成 uint64（保持位模式）
inline std::uint64_t read_offset_ptr_bits(const void* field) noexcept
{
    std::uint64_t out = 0;
    std::memcpy(&out, field, sizeof(out));
    return out;
}

// 把 uint64 位模式写回 offset_ptr 字段（绕过 reset，保持原始 m_offset）
inline void write_offset_ptr_bits(void* field, std::uint64_t bits) noexcept
{
    std::memcpy(field, &bits, sizeof(bits));
}

// 由 offset_ptr 字段地址 + m_offset 还原裸指针（self-relative 反算）；
// null sentinel = shm_ptr<>::null_offset = 1
template <typename T>
T* offset_ptr_decode(const void* field, std::int64_t m_offset) noexcept
{
    if (m_offset == shm_ptr<T>::null_offset) {
        return nullptr;
    }
    auto* base = static_cast<const std::byte*>(field);
    return reinterpret_cast<T*>(const_cast<std::byte*>(base + m_offset));
}

// FNV-1a 32bit：用作 property_slot.key_hash 的快速过滤
std::uint32_t fnv1a_32(std::string_view sv) noexcept
{
    constexpr std::uint32_t k_offset = 0x811C9DC5U;
    constexpr std::uint32_t k_prime  = 0x01000193U;
    std::uint32_t           h        = k_offset;
    for (unsigned char c : sv) {
        h ^= c;
        h *= k_prime;
    }
    return h;
}

std::string_view bs_view(shm_ptr<shm_byte_string> p) noexcept
{
    auto* bs = p.get();
    return bs == nullptr ? std::string_view{} : bs->as_string_view();
}

bool slot_key_equals(const property_slot& slot, std::uint32_t key_hash, std::string_view key) noexcept
{
    if (slot.key_hash != key_hash) {
        return false;
    }
    return bs_view(slot.key) == key;
}

void release_slot_byte_strings(shm_allocator& alloc, property_slot& slot) noexcept
{
    shm_byte_string_destroy(alloc, slot.key.get());
    slot.key = nullptr;
    if (slot.type == property_type_tag::string || slot.type == property_type_tag::bytes) {
        shm_byte_string_destroy(alloc, slot.v_blob.get());
        slot.v_blob = nullptr;
    }
}

void reset_property_slot(property_slot& slot) noexcept
{
    slot.key_hash = 0;
    slot._pad0    = 0;
    slot.key      = nullptr;
    slot.type     = property_type_tag::null;
    slot._pad1[0] = 0;
    slot._pad1[1] = 0;
    slot._pad1[2] = 0;
    slot.flags    = 0;
    slot.v_int64  = 0;
}

// 重算并写入 property_slab.crc32
void property_slab_refresh_crc(property_slab& slab) noexcept
{
    slab.crc32 = property_slab_compute_crc(slab);
}

// 重算并写入 child_slab.crc32
void child_slab_refresh_crc(child_slab& slab) noexcept
{
    slab.crc32 = child_slab_compute_crc(slab);
}

void slot_assign_int64(shm_allocator& alloc, property_slot& slot, std::int64_t value) noexcept
{
    if (slot.type == property_type_tag::string || slot.type == property_type_tag::bytes) {
        shm_byte_string_destroy(alloc, slot.v_blob.get());
        slot.v_blob = nullptr;
    }
    slot.type    = property_type_tag::int64;
    slot.v_int64 = value;
}

void slot_assign_double(shm_allocator& alloc, property_slot& slot, double value) noexcept
{
    if (slot.type == property_type_tag::string || slot.type == property_type_tag::bytes) {
        shm_byte_string_destroy(alloc, slot.v_blob.get());
        slot.v_blob = nullptr;
    }
    slot.type     = property_type_tag::double_;
    slot.v_double = value;
}

bool slot_assign_blob(shm_allocator& alloc, property_slot& slot, std::string_view blob, property_type_tag tag) noexcept
{
    auto* new_blob = shm_byte_string_create(alloc, blob);
    if (new_blob == nullptr && !blob.empty()) {
        return false;
    }
    if (slot.type == property_type_tag::string || slot.type == property_type_tag::bytes) {
        shm_byte_string_destroy(alloc, slot.v_blob.get());
    }
    slot.type   = tag;
    slot.v_blob = new_blob;
    return true;
}

// 将 src slab 的 [0, slot_count) 的所有 slot 转移到 dst slab。
// 注意：offset_ptr 是 self-relative，必须按 slot 重新 reset，不能 memcpy。
void transfer_property_slots(property_slab& src, property_slab& dst) noexcept
{
    const std::uint16_t n = src.slot_count;
    for (std::uint16_t i = 0; i < n; ++i) {
        const property_slot& s = src.slots[i];
        property_slot&       d = dst.slots[i];
        d.key_hash             = s.key_hash;
        d.key                  = s.key.get(); // offset_ptr 重新基于 d 自身地址计算
        d.type                 = s.type;
        switch (s.type) {
            case property_type_tag::int64:
                d.v_int64 = s.v_int64;
                break;
            case property_type_tag::double_:
                d.v_double = s.v_double;
                break;
            case property_type_tag::string:
            case property_type_tag::bytes:
                d.v_blob = s.v_blob.get();
                break;
            case property_type_tag::null:
            default:
                d.v_int64 = 0;
                break;
        }
    }
    dst.slot_count = n;
}

void transfer_child_slots(child_slab& src, child_slab& dst) noexcept
{
    const std::uint16_t n = src.slot_count;
    for (std::uint16_t i = 0; i < n; ++i) {
        dst.slots[i] = src.slots[i].get();
    }
    dst.slot_count = n;
}

// 把 property_slab 的 slot count "腾移"清零（用于扩容后释放旧 slab 时不重复释放
// 已转移到新 slab 的 byte_string）
void clear_property_slab_for_handoff(property_slab& slab) noexcept
{
    for (std::uint16_t i = 0; i < slab.slot_count; ++i) {
        slab.slots[i].key    = nullptr;
        slab.slots[i].v_blob = nullptr;
    }
    slab.slot_count = 0;
}

void clear_child_slab_for_handoff(child_slab& slab) noexcept
{
    slab.slot_count = 0;
}

} // namespace

// ============================================================================
// 容量增长
// ============================================================================

std::uint16_t slab_grow_capacity(std::uint16_t current, std::uint16_t needed) noexcept
{
    constexpr std::uint16_t k_min = 4U;
    constexpr auto          k_max = std::numeric_limits<std::uint16_t>::max();
    std::uint32_t           cap   = current == 0U ? k_min : current;
    while (cap < needed) {
        const std::uint32_t doubled = cap * 2U;
        if (doubled > k_max) {
            return needed;
        }
        cap = doubled;
    }
    return static_cast<std::uint16_t>(cap);
}

// ============================================================================
// byte_string 帮助
// ============================================================================

shm_byte_string* shm_byte_string_create(shm_allocator& alloc, std::string_view sv) noexcept
{
    void* mem = alloc.allocate(sizeof(shm_byte_string), alignof(shm_byte_string));
    if (mem == nullptr) {
        return nullptr;
    }
    auto* bs = ::new (mem) shm_byte_string(shm_byte_string::create(alloc, sv));
    if (!sv.empty() && bs->size() == 0U) {
        bs->~byte_string();
        alloc.deallocate(mem);
        return nullptr;
    }
    return bs;
}

void shm_byte_string_destroy(shm_allocator& alloc, shm_byte_string* bs) noexcept
{
    if (bs == nullptr) {
        return;
    }
    bs->~byte_string(); // 释放 payload buffer
    alloc.deallocate(bs);
}

// ============================================================================
// property_slab ops
// ============================================================================

property_slab* property_slab_create(shm_allocator& alloc, std::uint16_t capacity) noexcept
{
    const std::size_t bytes = property_slab_alloc_size(capacity);
    void*             mem   = alloc.allocate(bytes, alignof(property_slab));
    if (mem == nullptr) {
        return nullptr;
    }
    std::memset(mem, 0, bytes);
    auto* slab          = static_cast<property_slab*>(mem);
    slab->abi_version   = shm_object_abi_version;
    slab->slot_capacity = capacity;
    slab->slot_count    = 0;
    slab->flags         = 0;
    property_slab_refresh_crc(*slab);
    return slab;
}

void property_slab_destroy(shm_allocator& alloc, property_slab* slab) noexcept
{
    if (slab == nullptr) {
        return;
    }
    for (std::uint16_t i = 0; i < slab->slot_count; ++i) {
        release_slot_byte_strings(alloc, slab->slots[i]);
    }
    alloc.deallocate(slab);
}

int property_slab_find(const property_slab& slab, std::string_view key) noexcept
{
    const std::uint32_t h = fnv1a_32(key);
    for (std::uint16_t i = 0; i < slab.slot_count; ++i) {
        if (slot_key_equals(slab.slots[i], h, key)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

namespace {

// 共享逻辑：ensure 容量至少 needed_count；不足时分配新 slab，迁移所有 slot，
// 释放旧 slab；slab_inout 写新地址。失败 false（旧 slab/内容不变）。
bool ensure_property_slab_capacity(shm_allocator& alloc, property_slab*& slab_inout,
                                   std::uint16_t needed_count) noexcept
{
    if (slab_inout != nullptr && needed_count <= slab_inout->slot_capacity) {
        return true;
    }
    const std::uint16_t cur_cap = slab_inout == nullptr ? 0U : slab_inout->slot_capacity;
    const std::uint16_t new_cap = slab_grow_capacity(cur_cap, needed_count);
    if (new_cap == 0U) {
        return false;
    }
    auto* new_slab = property_slab_create(alloc, new_cap);
    if (new_slab == nullptr) {
        return false;
    }
    if (slab_inout != nullptr) {
        transfer_property_slots(*slab_inout, *new_slab);
        clear_property_slab_for_handoff(*slab_inout);
        property_slab_destroy(alloc, slab_inout);
    }
    property_slab_refresh_crc(*new_slab);
    slab_inout = new_slab;
    return true;
}

// 给定 slab，为 key 分配/找到一个 slot；找到现有返回 idx；新增需要扩容时
// 通过 ensure_property_slab_capacity 接管。
//
// out_idx 写入最终的 slot 索引；out_is_new 标记是否新增。
bool acquire_property_slot(shm_allocator& alloc, property_slab*& slab_inout, std::string_view key, int& out_idx,
                           bool& out_is_new) noexcept
{
    if (slab_inout != nullptr) {
        const int existing = property_slab_find(*slab_inout, key);
        if (existing >= 0) {
            out_idx    = existing;
            out_is_new = false;
            return true;
        }
    }
    const std::uint16_t cur_count = slab_inout == nullptr ? 0U : slab_inout->slot_count;
    if (!ensure_property_slab_capacity(alloc, slab_inout, static_cast<std::uint16_t>(cur_count + 1))) {
        return false;
    }
    auto* key_bs = shm_byte_string_create(alloc, key);
    if (key_bs == nullptr && !key.empty()) {
        return false;
    }
    const std::uint16_t idx  = slab_inout->slot_count;
    auto&               slot = slab_inout->slots[idx];
    slot.key_hash            = fnv1a_32(key);
    slot.key                 = key_bs;
    slot.type                = property_type_tag::null;
    slot.v_int64             = 0;
    slab_inout->slot_count   = static_cast<std::uint16_t>(idx + 1);
    out_idx                  = idx;
    out_is_new               = true;
    return true;
}

} // namespace

bool property_slab_set_int64(shm_allocator& alloc, property_slab*& slab_inout, std::string_view key,
                             std::int64_t value) noexcept
{
    int  idx    = -1;
    bool is_new = false;
    if (!acquire_property_slot(alloc, slab_inout, key, idx, is_new)) {
        return false;
    }
    slot_assign_int64(alloc, slab_inout->slots[idx], value);
    property_slab_refresh_crc(*slab_inout);
    return true;
}

bool property_slab_set_double(shm_allocator& alloc, property_slab*& slab_inout, std::string_view key,
                              double value) noexcept
{
    int  idx    = -1;
    bool is_new = false;
    if (!acquire_property_slot(alloc, slab_inout, key, idx, is_new)) {
        return false;
    }
    slot_assign_double(alloc, slab_inout->slots[idx], value);
    property_slab_refresh_crc(*slab_inout);
    return true;
}

bool property_slab_set_blob(shm_allocator& alloc, property_slab*& slab_inout, std::string_view key,
                            std::string_view blob, property_type_tag tag) noexcept
{
    if (tag != property_type_tag::string && tag != property_type_tag::bytes) {
        return false;
    }
    int  idx    = -1;
    bool is_new = false;
    if (!acquire_property_slot(alloc, slab_inout, key, idx, is_new)) {
        return false;
    }
    if (!slot_assign_blob(alloc, slab_inout->slots[idx], blob, tag)) {
        if (is_new) {
            // 回滚：刚刚 push 的 slot 移除（释放 key byte_string）
            release_slot_byte_strings(alloc, slab_inout->slots[idx]);
            slab_inout->slot_count = static_cast<std::uint16_t>(slab_inout->slot_count - 1);
            property_slab_refresh_crc(*slab_inout);
        }
        return false;
    }
    property_slab_refresh_crc(*slab_inout);
    return true;
}

bool property_slab_remove(shm_allocator& alloc, property_slab& slab, std::string_view key) noexcept
{
    const int idx = property_slab_find(slab, key);
    if (idx < 0) {
        return false;
    }
    release_slot_byte_strings(alloc, slab.slots[idx]);
    const std::uint16_t last = static_cast<std::uint16_t>(slab.slot_count - 1);
    if (static_cast<std::uint16_t>(idx) != last) {
        property_slot& dst = slab.slots[idx];
        property_slot& src = slab.slots[last];
        dst.key_hash       = src.key_hash;
        dst.key            = src.key.get();
        dst.type           = src.type;
        switch (src.type) {
            case property_type_tag::int64:
                dst.v_int64 = src.v_int64;
                break;
            case property_type_tag::double_:
                dst.v_double = src.v_double;
                break;
            case property_type_tag::string:
            case property_type_tag::bytes:
                dst.v_blob = src.v_blob.get();
                break;
            default:
                dst.v_int64 = 0;
                break;
        }
        // src 中的 offset_ptr 主权已转移；清空避免重复释放
        src.key    = nullptr;
        src.v_blob = nullptr;
    }
    slab.slot_count = last;
    property_slab_refresh_crc(slab);
    return true;
}

// ============================================================================
// child_slab ops
// ============================================================================

child_slab* child_slab_create(shm_allocator& alloc, std::uint16_t capacity) noexcept
{
    const std::size_t bytes = child_slab_alloc_size(capacity);
    void*             mem   = alloc.allocate(bytes, alignof(child_slab));
    if (mem == nullptr) {
        return nullptr;
    }
    std::memset(mem, 0, bytes);
    auto* slab          = static_cast<child_slab*>(mem);
    slab->abi_version   = shm_object_abi_version;
    slab->slot_capacity = capacity;
    slab->slot_count    = 0;
    slab->flags         = 0;
    child_slab_refresh_crc(*slab);
    return slab;
}

void child_slab_destroy(shm_allocator& alloc, child_slab* slab) noexcept
{
    if (slab == nullptr) {
        return;
    }
    alloc.deallocate(slab);
}

int child_slab_find(const child_slab& slab, shm_object* child) noexcept
{
    for (std::uint16_t i = 0; i < slab.slot_count; ++i) {
        if (slab.slots[i].get() == child) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool child_slab_add(shm_allocator& alloc, child_slab*& slab_inout, shm_object* child) noexcept
{
    if (slab_inout != nullptr && child_slab_find(*slab_inout, child) >= 0) {
        return true; // 幂等
    }
    const std::uint16_t cur_count = slab_inout == nullptr ? 0U : slab_inout->slot_count;
    const std::uint16_t needed    = static_cast<std::uint16_t>(cur_count + 1);
    if (slab_inout == nullptr || needed > slab_inout->slot_capacity) {
        const std::uint16_t cur_cap = slab_inout == nullptr ? 0U : slab_inout->slot_capacity;
        const std::uint16_t new_cap = slab_grow_capacity(cur_cap, needed);
        if (new_cap == 0U) {
            return false;
        }
        auto* new_slab = child_slab_create(alloc, new_cap);
        if (new_slab == nullptr) {
            return false;
        }
        if (slab_inout != nullptr) {
            transfer_child_slots(*slab_inout, *new_slab);
            clear_child_slab_for_handoff(*slab_inout);
            child_slab_destroy(alloc, slab_inout);
        }
        slab_inout = new_slab;
    }
    slab_inout->slots[slab_inout->slot_count] = child;
    slab_inout->slot_count                    = static_cast<std::uint16_t>(slab_inout->slot_count + 1);
    child_slab_refresh_crc(*slab_inout);
    return true;
}

bool child_slab_remove(child_slab& slab, shm_object* child) noexcept
{
    const int idx = child_slab_find(slab, child);
    if (idx < 0) {
        return false;
    }
    const std::uint16_t last = static_cast<std::uint16_t>(slab.slot_count - 1);
    if (static_cast<std::uint16_t>(idx) != last) {
        slab.slots[idx] = slab.slots[last].get();
    }
    slab.slots[last] = nullptr;
    slab.slot_count  = last;
    child_slab_refresh_crc(slab);
    return true;
}

// ============================================================================
// shm_object ops
// ============================================================================

void shm_object_refresh_crc(shm_object& shadow) noexcept
{
    shadow.crc32_self = shm_object_compute_crc(shadow);
}

namespace {

// 预计算"如果把 target 指针写入 field 字段，该 offset_ptr.m_offset 应有的位模式"
// （offset_ptr 是 self-relative，必须以 field 自身地址为基底）
std::uint64_t predict_offset_ptr_bits(const void* field, const void* target) noexcept
{
    using op_traits = shm_ptr<int>; // 只用其 null_offset，与 T 无关
    if (target == nullptr) {
        return static_cast<std::uint64_t>(op_traits::null_offset);
    }
    const auto diff = static_cast<const std::byte*>(target) - static_cast<const std::byte*>(field);
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(diff));
}

bool journaled_byte_string_replace(shm_allocator& alloc, shm_object& shadow, shm_ptr<shm_byte_string>& field,
                                   std::string_view value) noexcept
{
    auto* new_bs = shm_byte_string_create(alloc, value);
    if (new_bs == nullptr && !value.empty()) {
        return false;
    }
    auto*               old_bs  = field.get();
    const std::uint64_t saved_a = read_offset_ptr_bits(&field);
    const std::uint64_t saved_b = predict_offset_ptr_bits(&field, new_bs);

    mc::shm::container::journal_begin(shadow.journal, mc::shm::container::container_op::update,
                                      rel_offset(&shadow, &field), sub_op::byte_string_replace, saved_a, saved_b, 0, 0);

    field = new_bs;
    shm_object_refresh_crc(shadow);
    mc::shm::container::journal_end(shadow.journal);

    shm_byte_string_destroy(alloc, old_bs);
    return true;
}

} // namespace

shm_object* shm_object_create(shm_allocator& alloc, std::uint64_t object_id, std::string_view class_name,
                              std::string_view name, std::string_view path, std::string_view position) noexcept
{
    void* mem = alloc.allocate(sizeof(shm_object), alignof(shm_object));
    if (mem == nullptr) {
        return nullptr;
    }
    std::memset(mem, 0, sizeof(shm_object));
    auto* shadow        = static_cast<shm_object*>(mem);
    shadow->abi_version = shm_object_abi_version;
    shadow->flags       = shm_object_flags::alive;
    shadow->object_id   = object_id;
    shadow->class_name  = nullptr;
    shadow->name        = nullptr;
    shadow->path        = nullptr;
    shadow->position    = nullptr;
    shadow->service     = nullptr;
    shadow->parent      = nullptr;
    shadow->properties  = nullptr;
    shadow->children    = nullptr;
    mc::shm::container::journal_init(shadow->journal);

    auto rollback = [&]() noexcept {
        shm_byte_string_destroy(alloc, shadow->class_name.get());
        shm_byte_string_destroy(alloc, shadow->name.get());
        shm_byte_string_destroy(alloc, shadow->path.get());
        shm_byte_string_destroy(alloc, shadow->position.get());
        alloc.deallocate(shadow);
    };

    auto try_set = [&](shm_ptr<shm_byte_string>& field, std::string_view sv) noexcept -> bool {
        auto* bs = shm_byte_string_create(alloc, sv);
        if (bs == nullptr && !sv.empty()) {
            return false;
        }
        field = bs;
        return true;
    };

    if (!try_set(shadow->class_name, class_name) || !try_set(shadow->name, name) || !try_set(shadow->path, path) ||
        !try_set(shadow->position, position)) {
        rollback();
        return nullptr;
    }
    shm_object_refresh_crc(*shadow);
    return shadow;
}

void shm_object_destroy(shm_allocator& alloc, shm_object* shadow) noexcept
{
    if (shadow == nullptr) {
        return;
    }
    shm_byte_string_destroy(alloc, shadow->class_name.get());
    shm_byte_string_destroy(alloc, shadow->name.get());
    shm_byte_string_destroy(alloc, shadow->path.get());
    shm_byte_string_destroy(alloc, shadow->position.get());
    property_slab_destroy(alloc, shadow->properties.get());
    child_slab_destroy(alloc, shadow->children.get());
    alloc.deallocate(shadow);
}

bool shm_object_set_class_name(shm_allocator& alloc, shm_object& shadow, std::string_view value) noexcept
{
    return journaled_byte_string_replace(alloc, shadow, shadow.class_name, value);
}

bool shm_object_set_name(shm_allocator& alloc, shm_object& shadow, std::string_view value) noexcept
{
    return journaled_byte_string_replace(alloc, shadow, shadow.name, value);
}

bool shm_object_set_path(shm_allocator& alloc, shm_object& shadow, std::string_view value) noexcept
{
    return journaled_byte_string_replace(alloc, shadow, shadow.path, value);
}

bool shm_object_set_position(shm_allocator& alloc, shm_object& shadow, std::string_view value) noexcept
{
    return journaled_byte_string_replace(alloc, shadow, shadow.position, value);
}

namespace {

// offset_ptr setter（parent / service）：不涉及 byte_string ownership，
// recover 单纯按 saved_a/saved_b 决定 redo/undo 即可
template <typename T>
void journaled_offset_ptr_replace(shm_object& shadow, shm_ptr<T>& field, T* target) noexcept
{
    const std::uint64_t saved_a = read_offset_ptr_bits(&field);
    const std::uint64_t saved_b = predict_offset_ptr_bits(&field, target);

    mc::shm::container::journal_begin(shadow.journal, mc::shm::container::container_op::update,
                                      rel_offset(&shadow, &field), sub_op::offset_ptr_replace, saved_a, saved_b, 0, 0);

    field = target;
    shm_object_refresh_crc(shadow);
    mc::shm::container::journal_end(shadow.journal);
}

} // namespace

void shm_object_set_parent(shm_object& shadow, shm_object* parent) noexcept
{
    journaled_offset_ptr_replace(shadow, shadow.parent, parent);
}

void shm_object_set_service(shm_object& shadow, shm_service* service) noexcept
{
    journaled_offset_ptr_replace(shadow, shadow.service, service);
}
namespace {

// 把"按 type 取 slot 的 v 字段位模式"做成 helper
std::uint64_t read_slot_value_bits(const property_slot& slot) noexcept
{
    std::uint64_t bits = 0;
    std::memcpy(&bits, &slot.v_int64, sizeof(bits));
    return bits;
}

void write_slot_value_bits(property_slot& slot, std::uint64_t bits) noexcept
{
    std::memcpy(&slot.v_int64, &bits, sizeof(bits));
}

// 把 shadow.properties / shadow.children 切到新 slab，按 journal 三步协议；
// 调用方负责 alloc + transfer 完成后传入 new_slab，本函数负责 journal + CRC。
// 提交后调用方应做 clear_for_handoff(*old_slab) + destroy(old_slab)，
// 因为旧 slab 当前仍持有 byte_string ownership 双引用。
template <typename SlabT>
void journaled_slab_replace(shm_object& shadow, shm_ptr<SlabT>& field, SlabT* new_slab,
                            std::uint64_t sub_op_id) noexcept
{
    const std::uint64_t saved_a = read_offset_ptr_bits(&field);
    const std::uint64_t saved_b = predict_offset_ptr_bits(&field, new_slab);

    mc::shm::container::journal_begin(shadow.journal, mc::shm::container::container_op::update,
                                      rel_offset(&shadow, &field), sub_op_id, saved_a, saved_b, 0, 0);

    field = new_slab;
    shm_object_refresh_crc(shadow);
    mc::shm::container::journal_end(shadow.journal);
}

// 把 (old_type, new_type, slot_index, key_hash) 打包进 extra (uint64)
std::uint64_t pack_property_extra(property_type_tag old_type, property_type_tag new_type, std::uint16_t slot_index,
                                  std::uint32_t key_hash) noexcept
{
    return static_cast<std::uint64_t>(static_cast<std::uint8_t>(old_type)) |
           (static_cast<std::uint64_t>(static_cast<std::uint8_t>(new_type)) << 8) |
           (static_cast<std::uint64_t>(slot_index) << 16) | (static_cast<std::uint64_t>(key_hash) << 32);
}

struct property_extra_view {
    property_type_tag old_type;
    property_type_tag new_type;
    std::uint16_t     slot_index;
    std::uint32_t     key_hash;
};

property_extra_view unpack_property_extra(std::uint64_t e) noexcept
{
    property_extra_view v{};
    v.old_type   = static_cast<property_type_tag>(e & 0xFFU);
    v.new_type   = static_cast<property_type_tag>((e >> 8) & 0xFFU);
    v.slot_index = static_cast<std::uint16_t>((e >> 16) & 0xFFFFU);
    v.key_hash   = static_cast<std::uint32_t>(e >> 32);
    return v;
}

void journaled_replace_property_value(shm_allocator& alloc, shm_object& shadow, property_slab& slab, std::uint16_t idx,
                                      property_type_tag new_type, std::uint64_t new_value_bits) noexcept
{
    auto&                   slot     = slab.slots[idx];
    const std::uint64_t     saved_a  = read_slot_value_bits(slot);
    const property_type_tag old_type = slot.type;
    auto* const             old_blob =
        (old_type == property_type_tag::string || old_type == property_type_tag::bytes) ? slot.v_blob.get() : nullptr;

    const std::uint64_t sub = (new_type == property_type_tag::string || new_type == property_type_tag::bytes ||
                               old_type == property_type_tag::string || old_type == property_type_tag::bytes)
                                  ? sub_op::property_value_blob
                                  : sub_op::property_value_simple;

    mc::shm::container::journal_begin(shadow.journal, mc::shm::container::container_op::update,
                                      rel_offset(&shadow, &slot), sub, saved_a, new_value_bits,
                                      pack_property_extra(old_type, new_type, idx, slot.key_hash), 0);

    write_slot_value_bits(slot, new_value_bits);
    slot.type = new_type;
    property_slab_refresh_crc(slab);
    mc::shm::container::journal_end(shadow.journal);

    if (old_blob != nullptr) {
        shm_byte_string_destroy(alloc, old_blob);
    }
}

void journaled_create_property_slot(shm_allocator& alloc, shm_object& shadow, property_slab& slab, std::uint16_t idx,
                                    std::uint32_t key_hash, shm_byte_string* key_bs, property_type_tag new_type,
                                    std::uint64_t new_value_bits) noexcept
{
    auto&               slot      = slab.slots[idx];
    const std::uint64_t saved_key = predict_offset_ptr_bits(&slot.key, key_bs);

    mc::shm::container::journal_begin(shadow.journal, mc::shm::container::container_op::update,
                                      rel_offset(&shadow, &slab), sub_op::property_slot_create, saved_key,
                                      new_value_bits,
                                      pack_property_extra(property_type_tag::null, new_type, idx, key_hash), 0);

    slot.key_hash = key_hash;
    slot.key      = key_bs;
    slot.type     = new_type;
    slot.flags    = 0;
    write_slot_value_bits(slot, new_value_bits);
    slab.slot_count = static_cast<std::uint16_t>(idx + 1);
    property_slab_refresh_crc(slab);
    mc::shm::container::journal_end(shadow.journal);
    (void)alloc;
}

// 高层"找/扩容"封装；如需扩容则走 slab 替换 journal；返回成功后 slab 指向最终容器
// （可能是 new_slab）。
bool ensure_slot_capacity_for_high_level(shm_allocator& alloc, shm_object& shadow, property_slab*& slab_inout,
                                         std::uint16_t needed) noexcept
{
    if (slab_inout != nullptr && needed <= slab_inout->slot_capacity) {
        return true;
    }
    const std::uint16_t cur_cap = slab_inout == nullptr ? 0U : slab_inout->slot_capacity;
    const std::uint16_t new_cap = slab_grow_capacity(cur_cap, needed);
    if (new_cap == 0U) {
        return false;
    }
    auto* new_slab = property_slab_create(alloc, new_cap);
    if (new_slab == nullptr) {
        return false;
    }
    if (slab_inout != nullptr) {
        transfer_property_slots(*slab_inout, *new_slab);
        property_slab_refresh_crc(*new_slab);
    }
    auto* const old_slab = slab_inout;
    journaled_slab_replace(shadow, shadow.properties, new_slab, sub_op::property_slab_replace);
    if (old_slab != nullptr) {
        clear_property_slab_for_handoff(*old_slab);
        property_slab_destroy(alloc, old_slab);
    }
    slab_inout = new_slab;
    return true;
}

bool ensure_child_capacity_for_high_level(shm_allocator& alloc, shm_object& shadow, child_slab*& slab_inout,
                                          std::uint16_t needed) noexcept
{
    if (slab_inout != nullptr && needed <= slab_inout->slot_capacity) {
        return true;
    }
    const std::uint16_t cur_cap = slab_inout == nullptr ? 0U : slab_inout->slot_capacity;
    const std::uint16_t new_cap = slab_grow_capacity(cur_cap, needed);
    if (new_cap == 0U) {
        return false;
    }
    auto* new_slab = child_slab_create(alloc, new_cap);
    if (new_slab == nullptr) {
        return false;
    }
    if (slab_inout != nullptr) {
        transfer_child_slots(*slab_inout, *new_slab);
        child_slab_refresh_crc(*new_slab);
    }
    auto* const old_slab = slab_inout;
    journaled_slab_replace(shadow, shadow.children, new_slab, sub_op::child_slab_replace);
    if (old_slab != nullptr) {
        clear_child_slab_for_handoff(*old_slab);
        child_slab_destroy(alloc, old_slab);
    }
    slab_inout = new_slab;
    return true;
}

// 把 int64 / double 装进 8B 位模式
std::uint64_t bits_from_int64(std::int64_t v) noexcept
{
    std::uint64_t b = 0;
    std::memcpy(&b, &v, sizeof(b));
    return b;
}

std::uint64_t bits_from_double(double v) noexcept
{
    std::uint64_t b = 0;
    std::memcpy(&b, &v, sizeof(b));
    return b;
}

} // namespace

bool shm_object_set_property_int64(shm_allocator& alloc, shm_object& shadow, std::string_view key,
                                   std::int64_t value) noexcept
{
    auto*     slab = shadow.properties.get();
    const int idx  = (slab == nullptr) ? -1 : property_slab_find(*slab, key);
    if (idx >= 0) {
        journaled_replace_property_value(alloc, shadow, *slab, static_cast<std::uint16_t>(idx),
                                         property_type_tag::int64, bits_from_int64(value));
        return true;
    }
    const std::uint16_t cur_count = (slab == nullptr) ? 0U : slab->slot_count;
    if (!ensure_slot_capacity_for_high_level(alloc, shadow, slab, static_cast<std::uint16_t>(cur_count + 1))) {
        return false;
    }
    auto* key_bs = shm_byte_string_create(alloc, key);
    if (key_bs == nullptr && !key.empty()) {
        return false;
    }
    journaled_create_property_slot(alloc, shadow, *slab, cur_count, fnv1a_32(key), key_bs, property_type_tag::int64,
                                   bits_from_int64(value));
    return true;
}

bool shm_object_set_property_double(shm_allocator& alloc, shm_object& shadow, std::string_view key,
                                    double value) noexcept
{
    auto*     slab = shadow.properties.get();
    const int idx  = (slab == nullptr) ? -1 : property_slab_find(*slab, key);
    if (idx >= 0) {
        journaled_replace_property_value(alloc, shadow, *slab, static_cast<std::uint16_t>(idx),
                                         property_type_tag::double_, bits_from_double(value));
        return true;
    }
    const std::uint16_t cur_count = (slab == nullptr) ? 0U : slab->slot_count;
    if (!ensure_slot_capacity_for_high_level(alloc, shadow, slab, static_cast<std::uint16_t>(cur_count + 1))) {
        return false;
    }
    auto* key_bs = shm_byte_string_create(alloc, key);
    if (key_bs == nullptr && !key.empty()) {
        return false;
    }
    journaled_create_property_slot(alloc, shadow, *slab, cur_count, fnv1a_32(key), key_bs, property_type_tag::double_,
                                   bits_from_double(value));
    return true;
}

bool shm_object_set_property_blob(shm_allocator& alloc, shm_object& shadow, std::string_view key, std::string_view blob,
                                  property_type_tag tag) noexcept
{
    if (tag != property_type_tag::string && tag != property_type_tag::bytes) {
        return false;
    }
    auto*     slab = shadow.properties.get();
    const int idx  = (slab == nullptr) ? -1 : property_slab_find(*slab, key);
    if (idx >= 0) {
        auto* new_blob = shm_byte_string_create(alloc, blob);
        if (new_blob == nullptr && !blob.empty()) {
            return false;
        }
        const std::uint64_t new_bits = predict_offset_ptr_bits(&slab->slots[idx].v_blob, new_blob);
        journaled_replace_property_value(alloc, shadow, *slab, static_cast<std::uint16_t>(idx), tag, new_bits);
        return true;
    }
    const std::uint16_t cur_count = (slab == nullptr) ? 0U : slab->slot_count;
    if (!ensure_slot_capacity_for_high_level(alloc, shadow, slab, static_cast<std::uint16_t>(cur_count + 1))) {
        return false;
    }
    auto* key_bs = shm_byte_string_create(alloc, key);
    if (key_bs == nullptr && !key.empty()) {
        return false;
    }
    auto* blob_bs = shm_byte_string_create(alloc, blob);
    if (blob_bs == nullptr && !blob.empty()) {
        shm_byte_string_destroy(alloc, key_bs);
        return false;
    }
    const std::uint64_t new_bits = predict_offset_ptr_bits(&slab->slots[cur_count].v_blob, blob_bs);
    journaled_create_property_slot(alloc, shadow, *slab, cur_count, fnv1a_32(key), key_bs, tag, new_bits);
    return true;
}

bool shm_object_set_property_nocache_marker(shm_allocator& alloc, shm_object& shadow, std::string_view key) noexcept
{
    auto* slab = shadow.properties.get();
    if (slab != nullptr) {
        const int idx = property_slab_find(*slab, key);
        if (idx >= 0) {
            auto& slot = slab->slots[idx];
            if (slot.flags & property_slot_flags::nocache) {
                return true;
            }
            // 值 slot 转为 nocache marker 前需释放 blob 资源。
            if (slot.type == property_type_tag::string || slot.type == property_type_tag::bytes) {
                if (auto* blob = slot.v_blob.get(); blob != nullptr) {
                    shm_byte_string_destroy(alloc, blob);
                    slot.v_blob = nullptr;
                }
            }
            slot.type    = property_type_tag::null;
            slot.flags   = property_slot_flags::nocache;
            slot.v_int64 = 0;
            property_slab_refresh_crc(*slab);
            return true;
        }
    }
    const std::uint16_t cur_count = (slab == nullptr) ? 0U : slab->slot_count;
    if (!ensure_slot_capacity_for_high_level(alloc, shadow, slab, static_cast<std::uint16_t>(cur_count + 1))) {
        return false;
    }
    auto* key_bs = shm_byte_string_create(alloc, key);
    if (key_bs == nullptr && !key.empty()) {
        return false;
    }
    auto& slot       = slab->slots[cur_count];
    slot.key_hash    = fnv1a_32(key);
    slot.key         = key_bs;
    slot.type        = property_type_tag::null;
    slot.flags       = property_slot_flags::nocache;
    slot.v_int64     = 0;
    slab->slot_count = static_cast<std::uint16_t>(cur_count + 1);
    property_slab_refresh_crc(*slab);
    return true;
}

bool shm_object_remove_property(shm_allocator& alloc, shm_object& shadow, std::string_view key) noexcept
{
    auto* slab = shadow.properties.get();
    if (slab == nullptr) {
        return false;
    }
    return property_slab_remove(alloc, *slab, key);
}

bool shm_object_add_child(shm_allocator& alloc, shm_object& shadow, shm_object* child) noexcept
{
    auto* slab = shadow.children.get();
    if (slab != nullptr && child_slab_find(*slab, child) >= 0) {
        return true;
    }
    const std::uint16_t cur_count = (slab == nullptr) ? 0U : slab->slot_count;
    if (!ensure_child_capacity_for_high_level(alloc, shadow, slab, static_cast<std::uint16_t>(cur_count + 1))) {
        return false;
    }
    // child slot push：journal 编码 sub_op::child_slot_add
    //   target = rel_offset(shadow, slab)
    //   saved_a = predict_offset_ptr_bits(&slab->slots[cur_count], child)
    //   saved_b = static_cast<uint64_t>(cur_count)  -- recover 用作 slot_index
    //   extra   = 0; misc = 0
    const std::uint64_t saved_key = predict_offset_ptr_bits(&slab->slots[cur_count], child);

    mc::shm::container::journal_begin(shadow.journal, mc::shm::container::container_op::update,
                                      rel_offset(&shadow, slab), sub_op::child_slot_add, saved_key,
                                      static_cast<std::uint64_t>(cur_count), 0, 0);

    slab->slots[cur_count] = child;
    slab->slot_count       = static_cast<std::uint16_t>(cur_count + 1);
    child_slab_refresh_crc(*slab);
    mc::shm::container::journal_end(shadow.journal);
    return true;
}

bool shm_object_remove_child(shm_object& shadow, shm_object* child) noexcept
{
    auto* slab = shadow.children.get();
    if (slab == nullptr) {
        return false;
    }
    return child_slab_remove(*slab, child);
}

// ============================================================================
// reader
// ============================================================================

std::string_view shm_object_class_name(const shm_object& shadow) noexcept
{
    return bs_view(shadow.class_name);
}

std::string_view shm_object_name(const shm_object& shadow) noexcept
{
    return bs_view(shadow.name);
}

std::string_view shm_object_path(const shm_object& shadow) noexcept
{
    return bs_view(shadow.path);
}

std::string_view shm_object_position(const shm_object& shadow) noexcept
{
    return bs_view(shadow.position);
}

namespace {

const property_slot* find_slot(const shm_object& shadow, std::string_view key) noexcept
{
    auto* slab = shadow.properties.get();
    if (slab == nullptr) {
        return nullptr;
    }
    const int idx = property_slab_find(*slab, key);
    return idx < 0 ? nullptr : &slab->slots[idx];
}

} // namespace

bool shm_object_get_property_int64(const shm_object& shadow, std::string_view key, std::int64_t& out) noexcept
{
    const auto* slot = find_slot(shadow, key);
    if (slot == nullptr || slot->type != property_type_tag::int64) {
        return false;
    }
    out = slot->v_int64;
    return true;
}

bool shm_object_get_property_double(const shm_object& shadow, std::string_view key, double& out) noexcept
{
    const auto* slot = find_slot(shadow, key);
    if (slot == nullptr || slot->type != property_type_tag::double_) {
        return false;
    }
    out = slot->v_double;
    return true;
}

bool shm_object_get_property_blob(const shm_object& shadow, std::string_view key, std::string_view& out,
                                  property_type_tag& tag) noexcept
{
    const auto* slot = find_slot(shadow, key);
    if (slot == nullptr) {
        return false;
    }
    if (slot->type != property_type_tag::string && slot->type != property_type_tag::bytes) {
        return false;
    }
    out = bs_view(slot->v_blob);
    tag = slot->type;
    return true;
}

std::uint16_t shm_object_property_count(const shm_object& shadow) noexcept
{
    auto* slab = shadow.properties.get();
    return slab == nullptr ? 0U : slab->slot_count;
}

std::uint16_t shm_object_child_count(const shm_object& shadow) noexcept
{
    auto* slab = shadow.children.get();
    return slab == nullptr ? 0U : slab->slot_count;
}

shm_object* shm_object_parent(const shm_object& shadow) noexcept
{
    return shadow.parent.get();
}

shm_service* shm_object_service(const shm_object& shadow) noexcept
{
    return shadow.service.get();
}

// ============================================================================
// Crash recovery
// ============================================================================

namespace {

// 在 shadow 内按 byte offset 取目标地址（offset 0 表示 nullptr 语义；本工程
// 内 0 不会出现：journal 写入的 target 都是非零 field offset）
void* shadow_at(shm_object& shadow, std::uint64_t off) noexcept
{
    return reinterpret_cast<std::byte*>(&shadow) + off;
}

// 从 (field 字段地址 + 编码的 m_offset 位模式) 反算实际 byte_string 地址
shm_byte_string* decode_byte_string(void* field, std::uint64_t bits) noexcept
{
    std::int64_t diff = 0;
    std::memcpy(&diff, &bits, sizeof(diff));
    return offset_ptr_decode<shm_byte_string>(field, diff);
}

void recover_byte_string_replace(shm_allocator& alloc, shm_object& shadow,
                                 const mc::shm::container::container_journal_view& v) noexcept
{
    void* const         field = shadow_at(shadow, v.target_node_offset);
    const std::uint64_t cur   = read_offset_ptr_bits(field);
    if (cur == v.saved_link_b) {
        // commit 已生效，可能 CRC 也写过；保险起见 refresh
        shm_object_refresh_crc(shadow);
    } else {
        // commit 未发生：写回旧值；新 byte_string 已分配但未引用 → destroy
        write_offset_ptr_bits(field, v.saved_link_a);
        shm_object_refresh_crc(shadow);
        auto* new_bs = decode_byte_string(field, v.saved_link_b);
        shm_byte_string_destroy(alloc, new_bs);
    }
}

void recover_offset_ptr_replace(shm_object& shadow, const mc::shm::container::container_journal_view& v) noexcept
{
    void* const         field = shadow_at(shadow, v.target_node_offset);
    const std::uint64_t cur   = read_offset_ptr_bits(field);
    if (cur != v.saved_link_b) {
        write_offset_ptr_bits(field, v.saved_link_a);
    }
    shm_object_refresh_crc(shadow);
}

// slab 替换 recover：
//   - 若 shadow.<slab field>.m_offset == saved_b → commit 生效。旧 slab 未来得及
//     destroy，但旧 slab 仍持有对相同 byte_string 的双引用 → 安全 destroy 旧
//     （clear_for_handoff 防止 free byte_string）
//   - 若 == saved_a → undo: free 新 slab（同样 clear_for_handoff，旧 slab 仍持有 ownership）
template <typename SlabT, typename ClearFn, typename DestroyFn>
void recover_slab_replace(shm_allocator& alloc, shm_object& shadow, shm_ptr<SlabT>& field,
                          const mc::shm::container::container_journal_view& v, ClearFn clear_fn,
                          DestroyFn destroy_fn) noexcept
{
    void* const         field_addr = &field;
    const std::uint64_t cur        = read_offset_ptr_bits(field_addr);
    if (cur == v.saved_link_b) {
        // commit 生效；旧 slab 由 saved_a 解码
        std::int64_t old_diff = 0;
        std::memcpy(&old_diff, &v.saved_link_a, sizeof(old_diff));
        auto* old_slab = offset_ptr_decode<SlabT>(field_addr, old_diff);
        if (old_slab != nullptr) {
            clear_fn(*old_slab);
            destroy_fn(alloc, old_slab);
        }
        shm_object_refresh_crc(shadow);
    } else {
        // undo：当前指向旧 slab；新 slab 由 saved_b 解码后释放
        std::int64_t new_diff = 0;
        std::memcpy(&new_diff, &v.saved_link_b, sizeof(new_diff));
        auto* new_slab = offset_ptr_decode<SlabT>(field_addr, new_diff);
        if (new_slab != nullptr) {
            clear_fn(*new_slab);
            destroy_fn(alloc, new_slab);
        }
        write_offset_ptr_bits(field_addr, v.saved_link_a);
        shm_object_refresh_crc(shadow);
    }
}

// 找到 slot 所属的 slab；按 shadow.properties 解引用，扫描所有 slot 比较地址
property_slab* property_slab_for_slot(shm_object& shadow, property_slot& slot) noexcept
{
    auto* slab = shadow.properties.get();
    if (slab == nullptr) {
        return nullptr;
    }
    auto* slots_begin = &slab->slots[0];
    auto* slots_end   = slots_begin + slab->slot_capacity;
    if (&slot >= slots_begin && &slot < slots_end) {
        return slab;
    }
    return nullptr;
}

// property_value (covers simple + blob) recover
void recover_property_value(shm_allocator& alloc, shm_object& shadow,
                            const mc::shm::container::container_journal_view& v) noexcept
{
    auto* const slot_ptr = static_cast<property_slot*>(shadow_at(shadow, v.target_node_offset));
    auto&       slot     = *slot_ptr;
    const auto  ev       = unpack_property_extra(v.extra);

    // 校验 slot 仍在原位（未被 swap-and-pop）
    if (slot.key_hash != ev.key_hash) {
        // slot 被外部 remove，跳过 redo/undo（由 reader CRC 校验决定 isolated）
        return;
    }

    auto* const         slab       = property_slab_for_slot(shadow, slot);
    const std::uint64_t cur        = read_slot_value_bits(slot);
    const bool          is_blob_op = (v.anchor_offset == sub_op::property_value_blob);
    if (cur == v.saved_link_b && slot.type == ev.new_type) {
        // commit 生效；如果是 blob 且 旧 type==blob，旧 byte_string 可能未来得及
        // destroy（end 之后 destroy 之前崩溃）→ 此时旧 byte_string 仍可达？
        // 不，commit 后 reader 已看到新 v_blob，旧 byte_string 已无引用，但 slot.v_blob
        // 当前指向新；saved_link_a 持有旧 v_blob 位模式。我们选择不在 recover 中
        // destroy 旧 byte_string，因为无法区分"已 destroy 但 op 还没置 none"与
        // "未 destroy"两种情况——更安全：接受最多 1 个 alloc 单元的泄漏
        if (slab != nullptr) {
            property_slab_refresh_crc(*slab);
        }
    } else {
        // undo
        if (is_blob_op && (ev.new_type == property_type_tag::string || ev.new_type == property_type_tag::bytes)) {
            // 新 byte_string 已分配但未引用（或者引用了但要 undo）→ destroy
            std::int64_t new_diff = 0;
            std::memcpy(&new_diff, &v.saved_link_b, sizeof(new_diff));
            auto* new_blob = offset_ptr_decode<shm_byte_string>(&slot.v_blob, new_diff);
            shm_byte_string_destroy(alloc, new_blob);
        }
        write_slot_value_bits(slot, v.saved_link_a);
        slot.type = ev.old_type;
        if (slab != nullptr) {
            property_slab_refresh_crc(*slab);
        }
    }
}

// property_slot_create recover：判断 slot_count 是否已增到 idx+1 + slot.key_hash 是否一致
void recover_property_slot_create(shm_allocator& alloc, shm_object& shadow,
                                  const mc::shm::container::container_journal_view& v) noexcept
{
    auto* const         slab = static_cast<property_slab*>(shadow_at(shadow, v.target_node_offset));
    const auto          ev   = unpack_property_extra(v.extra);
    const std::uint16_t idx  = ev.slot_index;

    auto&      slot      = slab->slots[idx];
    const bool committed = (slab->slot_count > idx) && (slot.key_hash == ev.key_hash);
    if (committed) {
        property_slab_refresh_crc(*slab);
        return;
    }
    // undo：destroy 已分配但未引用的 key + (blob if applicable)
    std::int64_t key_diff = 0;
    std::memcpy(&key_diff, &v.saved_link_a, sizeof(key_diff));
    auto* key_bs = offset_ptr_decode<shm_byte_string>(&slot.key, key_diff);
    shm_byte_string_destroy(alloc, key_bs);
    if (ev.new_type == property_type_tag::string || ev.new_type == property_type_tag::bytes) {
        std::int64_t v_diff = 0;
        std::memcpy(&v_diff, &v.saved_link_b, sizeof(v_diff));
        auto* blob_bs = offset_ptr_decode<shm_byte_string>(&slot.v_blob, v_diff);
        shm_byte_string_destroy(alloc, blob_bs);
    }
    // slot_count 没改 → slot 不可见；保守清零 slot 字段（防止 reader 误读 type）
    reset_property_slot(slot);
    property_slab_refresh_crc(*slab);
}

// child_slot_add recover：判断 slab.slot_count 是否已增到 idx+1
void recover_child_slot_add(shm_object& shadow, const mc::shm::container::container_journal_view& v) noexcept
{
    auto* const         slab = static_cast<child_slab*>(shadow_at(shadow, v.target_node_offset));
    const std::uint16_t idx  = static_cast<std::uint16_t>(v.saved_link_b);
    if (slab->slot_count > idx) {
        // commit 生效
        child_slab_refresh_crc(*slab);
    } else {
        // undo：清空 slot[idx]（slot_count 未增，slot 不可见，保守清零）
        slab->slots[idx] = nullptr;
        child_slab_refresh_crc(*slab);
    }
}

} // namespace

void shm_object_journal_recover(shm_allocator& alloc, shm_object& shadow) noexcept
{
    mc::shm::container::container_journal_view v{};
    if (!mc::shm::container::journal_load(shadow.journal, v)) {
        return;
    }

    switch (v.anchor_offset) {
        case sub_op::byte_string_replace:
            recover_byte_string_replace(alloc, shadow, v);
            break;
        case sub_op::offset_ptr_replace:
            recover_offset_ptr_replace(shadow, v);
            break;
        case sub_op::property_value_simple:
        case sub_op::property_value_blob:
            recover_property_value(alloc, shadow, v);
            break;
        case sub_op::property_slot_create:
            recover_property_slot_create(alloc, shadow, v);
            break;
        case sub_op::property_slab_replace:
            recover_slab_replace(alloc, shadow, shadow.properties, v, [](property_slab& s) noexcept {
                clear_property_slab_for_handoff(s);
            }, [](shm_allocator& a, property_slab* s) noexcept {
                property_slab_destroy(a, s);
            });
            break;
        case sub_op::child_slab_replace:
            recover_slab_replace(alloc, shadow, shadow.children, v, [](child_slab& s) noexcept {
                clear_child_slab_for_handoff(s);
            }, [](shm_allocator& a, child_slab* s) noexcept {
                child_slab_destroy(a, s);
            });
            break;
        case sub_op::child_slot_add:
            recover_child_slot_add(shadow, v);
            break;
        default:
            // 未知 sub_op：保守 no-op，仍清 journal.op
            break;
    }
    mc::shm::container::journal_end(shadow.journal);
}

} // namespace mc::engine
