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

#include <mc/engine/shm_object_ops.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>

namespace mc::engine {

namespace {

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

// 将 byte_string offset_ptr 解引用为 string_view（支持 null）
std::string_view bs_view(mc::intrusive::offset_ptr<mc::shm::byte_string> p) noexcept
{
    auto* bs = p.get();
    return bs == nullptr ? std::string_view{} : bs->as_string_view();
}

// property_slot 与 key 比较：先 key_hash 过滤，再字节对比
bool slot_key_equals(const property_slot& slot, std::uint32_t key_hash,
                     std::string_view key) noexcept
{
    if (slot.key_hash != key_hash) {
        return false;
    }
    return bs_view(slot.key) == key;
}

// 释放 property_slot 内持有的 byte_string（key + 可能的 v_blob），不动 slot 本身字节
void release_slot_byte_strings(mc::shm::shm_allocator& alloc, property_slot& slot) noexcept
{
    shm_byte_string_destroy(alloc, slot.key.get());
    slot.key = nullptr;
    if (slot.type == property_type_tag::string || slot.type == property_type_tag::bytes) {
        shm_byte_string_destroy(alloc, slot.v_blob.get());
        slot.v_blob = nullptr;
    }
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

// 给 property_slot 写入 int64 类型的值（清理旧 blob）
void slot_assign_int64(mc::shm::shm_allocator& alloc, property_slot& slot,
                       std::int64_t value) noexcept
{
    if (slot.type == property_type_tag::string || slot.type == property_type_tag::bytes) {
        shm_byte_string_destroy(alloc, slot.v_blob.get());
        slot.v_blob = nullptr;
    }
    slot.type    = property_type_tag::int64;
    slot.v_int64 = value;
}

void slot_assign_double(mc::shm::shm_allocator& alloc, property_slot& slot, double value) noexcept
{
    if (slot.type == property_type_tag::string || slot.type == property_type_tag::bytes) {
        shm_byte_string_destroy(alloc, slot.v_blob.get());
        slot.v_blob = nullptr;
    }
    slot.type     = property_type_tag::double_;
    slot.v_double = value;
}

bool slot_assign_blob(mc::shm::shm_allocator& alloc, property_slot& slot, std::string_view blob,
                      property_type_tag tag) noexcept
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
        d.key                  = s.key.get();  // offset_ptr 重新基于 d 自身地址计算
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

}  // namespace

// ============================================================================
// 容量增长
// ============================================================================

std::uint16_t slab_grow_capacity(std::uint16_t current, std::uint16_t needed) noexcept
{
    constexpr std::uint16_t k_min = 4U;
    std::uint32_t           cap   = current == 0U ? k_min : current;
    while (cap < needed) {
        const std::uint32_t doubled = cap * 2U;
        if (doubled > 0xFFFFU) {
            return needed > 0xFFFFU ? std::uint16_t{0} : needed;
        }
        cap = doubled;
    }
    return static_cast<std::uint16_t>(cap);
}

// ============================================================================
// byte_string 帮助
// ============================================================================

mc::shm::byte_string* shm_byte_string_create(mc::shm::shm_allocator& alloc,
                                             std::string_view        sv) noexcept
{
    void* mem = alloc.allocate(sizeof(mc::shm::byte_string), alignof(mc::shm::byte_string));
    if (mem == nullptr) {
        return nullptr;
    }
    auto* bs = ::new (mem) mc::shm::byte_string(mc::shm::byte_string::create(alloc, sv));
    if (!sv.empty() && bs->size() == 0U) {
        bs->~byte_string();
        alloc.deallocate(mem);
        return nullptr;
    }
    return bs;
}

void shm_byte_string_destroy(mc::shm::shm_allocator& alloc, mc::shm::byte_string* bs) noexcept
{
    if (bs == nullptr) {
        return;
    }
    bs->~byte_string();  // 释放 payload buffer
    alloc.deallocate(bs);
}

// ============================================================================
// property_slab ops
// ============================================================================

property_slab* property_slab_create(mc::shm::shm_allocator& alloc, std::uint16_t capacity) noexcept
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

void property_slab_destroy(mc::shm::shm_allocator& alloc, property_slab* slab) noexcept
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
bool ensure_property_slab_capacity(mc::shm::shm_allocator& alloc, property_slab*& slab_inout,
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
bool acquire_property_slot(mc::shm::shm_allocator& alloc, property_slab*& slab_inout,
                           std::string_view key, int& out_idx, bool& out_is_new) noexcept
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
    if (!ensure_property_slab_capacity(alloc, slab_inout,
                                       static_cast<std::uint16_t>(cur_count + 1))) {
        return false;
    }
    auto* key_bs = shm_byte_string_create(alloc, key);
    if (key_bs == nullptr && !key.empty()) {
        return false;
    }
    const std::uint16_t idx = slab_inout->slot_count;
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

}  // namespace

bool property_slab_set_int64(mc::shm::shm_allocator& alloc, property_slab*& slab_inout,
                             std::string_view key, std::int64_t value) noexcept
{
    int  idx     = -1;
    bool is_new  = false;
    if (!acquire_property_slot(alloc, slab_inout, key, idx, is_new)) {
        return false;
    }
    slot_assign_int64(alloc, slab_inout->slots[idx], value);
    property_slab_refresh_crc(*slab_inout);
    return true;
}

bool property_slab_set_double(mc::shm::shm_allocator& alloc, property_slab*& slab_inout,
                              std::string_view key, double value) noexcept
{
    int  idx     = -1;
    bool is_new  = false;
    if (!acquire_property_slot(alloc, slab_inout, key, idx, is_new)) {
        return false;
    }
    slot_assign_double(alloc, slab_inout->slots[idx], value);
    property_slab_refresh_crc(*slab_inout);
    return true;
}

bool property_slab_set_blob(mc::shm::shm_allocator& alloc, property_slab*& slab_inout,
                            std::string_view key, std::string_view blob,
                            property_type_tag tag) noexcept
{
    if (tag != property_type_tag::string && tag != property_type_tag::bytes) {
        return false;
    }
    int  idx     = -1;
    bool is_new  = false;
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

bool property_slab_remove(mc::shm::shm_allocator& alloc, property_slab& slab,
                          std::string_view key) noexcept
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

child_slab* child_slab_create(mc::shm::shm_allocator& alloc, std::uint16_t capacity) noexcept
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

void child_slab_destroy(mc::shm::shm_allocator& alloc, child_slab* slab) noexcept
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

bool child_slab_add(mc::shm::shm_allocator& alloc, child_slab*& slab_inout,
                    shm_object* child) noexcept
{
    if (slab_inout != nullptr && child_slab_find(*slab_inout, child) >= 0) {
        return true;  // 幂等
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
    slab_inout->slot_count = static_cast<std::uint16_t>(slab_inout->slot_count + 1);
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

// 通用 identity setter：替换 shadow 中某个 byte_string 字段
bool set_identity_field(mc::shm::shm_allocator&                          alloc,
                        shm_object&                                   shadow,
                        mc::intrusive::offset_ptr<mc::shm::byte_string>& field,
                        std::string_view                                 value) noexcept
{
    auto* new_bs = shm_byte_string_create(alloc, value);
    if (new_bs == nullptr && !value.empty()) {
        return false;
    }
    shm_byte_string_destroy(alloc, field.get());
    field = new_bs;
    shm_object_refresh_crc(shadow);
    return true;
}

}  // namespace

shm_object* shm_object_create(mc::shm::shm_allocator& alloc, std::uint64_t object_id,
                                    std::string_view class_name, std::string_view name,
                                    std::string_view path, std::string_view position) noexcept
{
    void* mem = alloc.allocate(sizeof(shm_object), alignof(shm_object));
    if (mem == nullptr) {
        return nullptr;
    }
    std::memset(mem, 0, sizeof(shm_object));
    auto* shadow         = static_cast<shm_object*>(mem);
    shadow->abi_version  = shm_object_abi_version;
    shadow->flags        = shm_object_flags::alive;
    shadow->object_id    = object_id;
    shadow->class_name   = nullptr;
    shadow->name         = nullptr;
    shadow->path         = nullptr;
    shadow->position     = nullptr;
    shadow->service      = nullptr;
    shadow->parent       = nullptr;
    shadow->properties   = nullptr;
    shadow->children     = nullptr;

    auto rollback = [&]() noexcept {
        shm_byte_string_destroy(alloc, shadow->class_name.get());
        shm_byte_string_destroy(alloc, shadow->name.get());
        shm_byte_string_destroy(alloc, shadow->path.get());
        shm_byte_string_destroy(alloc, shadow->position.get());
        alloc.deallocate(shadow);
    };

    auto try_set = [&](mc::intrusive::offset_ptr<mc::shm::byte_string>& field,
                       std::string_view                                 sv) noexcept -> bool {
        auto* bs = shm_byte_string_create(alloc, sv);
        if (bs == nullptr && !sv.empty()) {
            return false;
        }
        field = bs;
        return true;
    };

    if (!try_set(shadow->class_name, class_name) || !try_set(shadow->name, name)
        || !try_set(shadow->path, path) || !try_set(shadow->position, position)) {
        rollback();
        return nullptr;
    }
    shm_object_refresh_crc(*shadow);
    return shadow;
}

void shm_object_destroy(mc::shm::shm_allocator& alloc, shm_object* shadow) noexcept
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

bool shm_object_set_class_name(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                  std::string_view value) noexcept
{
    return set_identity_field(alloc, shadow, shadow.class_name, value);
}

bool shm_object_set_name(mc::shm::shm_allocator& alloc, shm_object& shadow,
                            std::string_view value) noexcept
{
    return set_identity_field(alloc, shadow, shadow.name, value);
}

bool shm_object_set_path(mc::shm::shm_allocator& alloc, shm_object& shadow,
                            std::string_view value) noexcept
{
    return set_identity_field(alloc, shadow, shadow.path, value);
}

bool shm_object_set_position(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                std::string_view value) noexcept
{
    return set_identity_field(alloc, shadow, shadow.position, value);
}

void shm_object_set_parent(shm_object& shadow, shm_object* parent) noexcept
{
    shadow.parent = parent;
    shm_object_refresh_crc(shadow);
}

void shm_object_set_service(shm_object& shadow, shm_service* service) noexcept
{
    shadow.service = service;
    shm_object_refresh_crc(shadow);
}

bool shm_object_set_property_int64(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                      std::string_view key, std::int64_t value) noexcept
{
    auto* slab = shadow.properties.get();
    if (!property_slab_set_int64(alloc, slab, key, value)) {
        return false;
    }
    if (slab != shadow.properties.get()) {
        shadow.properties = slab;
        shm_object_refresh_crc(shadow);
    }
    return true;
}

bool shm_object_set_property_double(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                       std::string_view key, double value) noexcept
{
    auto* slab = shadow.properties.get();
    if (!property_slab_set_double(alloc, slab, key, value)) {
        return false;
    }
    if (slab != shadow.properties.get()) {
        shadow.properties = slab;
        shm_object_refresh_crc(shadow);
    }
    return true;
}

bool shm_object_set_property_blob(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                     std::string_view key, std::string_view blob,
                                     property_type_tag tag) noexcept
{
    auto* slab = shadow.properties.get();
    if (!property_slab_set_blob(alloc, slab, key, blob, tag)) {
        return false;
    }
    if (slab != shadow.properties.get()) {
        shadow.properties = slab;
        shm_object_refresh_crc(shadow);
    }
    return true;
}

bool shm_object_remove_property(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                   std::string_view key) noexcept
{
    auto* slab = shadow.properties.get();
    if (slab == nullptr) {
        return false;
    }
    return property_slab_remove(alloc, *slab, key);
}

bool shm_object_add_child(mc::shm::shm_allocator& alloc, shm_object& shadow,
                             shm_object* child) noexcept
{
    auto* slab = shadow.children.get();
    if (!child_slab_add(alloc, slab, child)) {
        return false;
    }
    if (slab != shadow.children.get()) {
        shadow.children = slab;
        shm_object_refresh_crc(shadow);
    }
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

}  // namespace

bool shm_object_get_property_int64(const shm_object& shadow, std::string_view key,
                                      std::int64_t& out) noexcept
{
    const auto* slot = find_slot(shadow, key);
    if (slot == nullptr || slot->type != property_type_tag::int64) {
        return false;
    }
    out = slot->v_int64;
    return true;
}

bool shm_object_get_property_double(const shm_object& shadow, std::string_view key,
                                       double& out) noexcept
{
    const auto* slot = find_slot(shadow, key);
    if (slot == nullptr || slot->type != property_type_tag::double_) {
        return false;
    }
    out = slot->v_double;
    return true;
}

bool shm_object_get_property_blob(const shm_object& shadow, std::string_view key,
                                     std::string_view& out, property_type_tag& tag) noexcept
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

}  // namespace mc::engine
