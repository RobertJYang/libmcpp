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

#ifndef MC_ENGINE_SHM_OBJECT_OPS_H
#define MC_ENGINE_SHM_OBJECT_OPS_H

#include <cstdint>
#include <string_view>

#include <mc/common.h>
#include <mc/engine/shm_object.h>
#include <mc/shm/allocator.h>

// shm_object / property_slab / child_slab 写读释放工具集。
// 所有函数 noexcept；分配失败返回 false / nullptr，可观测状态保持不变。

namespace mc::engine {

using shm_allocator = mc::shm::shm_allocator;

inline constexpr std::uint16_t property_slab_initial_capacity = 4U;
inline constexpr std::uint16_t child_slab_initial_capacity    = 4U;

MC_API std::uint16_t slab_grow_capacity(std::uint16_t current, std::uint16_t needed) noexcept;

MC_API shm_byte_string* shm_byte_string_create(shm_allocator& alloc, std::string_view sv) noexcept;
MC_API void shm_byte_string_destroy(shm_allocator& alloc, shm_byte_string* bs) noexcept;

// ---------------- property_slab ops ----------------

MC_API property_slab* property_slab_create(shm_allocator& alloc, std::uint16_t capacity) noexcept;
MC_API void property_slab_destroy(shm_allocator& alloc, property_slab* slab) noexcept;
MC_API int  property_slab_find(const property_slab& slab, std::string_view key) noexcept;

// 容量不足自动 grow，新地址写回 slab_inout，旧 slab 释放。
MC_API bool property_slab_set_int64(shm_allocator& alloc, property_slab*& slab_inout,
                                    std::string_view key, std::int64_t value) noexcept;
MC_API bool property_slab_set_double(shm_allocator& alloc, property_slab*& slab_inout,
                                     std::string_view key, double value) noexcept;
// tag 必须是 string 或 bytes。
MC_API bool property_slab_set_blob(shm_allocator& alloc, property_slab*& slab_inout,
                                   std::string_view key, std::string_view blob,
                                   property_type_tag tag) noexcept;
MC_API bool property_slab_remove(shm_allocator& alloc, property_slab& slab,
                                 std::string_view key) noexcept;

// ---------------- child_slab ops ----------------

MC_API child_slab* child_slab_create(shm_allocator& alloc, std::uint16_t capacity) noexcept;
// 仅释放 slab 自身，不级联销毁 child shadow。
MC_API void        child_slab_destroy(shm_allocator& alloc, child_slab* slab) noexcept;
MC_API int         child_slab_find(const child_slab& slab, shm_object* child) noexcept;
// 重复添加同一 child 视作 no-op。
MC_API bool        child_slab_add(shm_allocator& alloc, child_slab*& slab_inout,
                                  shm_object* child) noexcept;
MC_API bool        child_slab_remove(child_slab& slab, shm_object* child) noexcept;

// ---------------- shm_object 高层 ops ----------------

// service / parent 默认 null，需通过 setter 设置。失败回滚已分配资源。
MC_API shm_object* shm_object_create(shm_allocator& alloc, std::uint64_t object_id,
                                     std::string_view class_name, std::string_view name,
                                     std::string_view path, std::string_view position) noexcept;
// 释放 identity + property/child slab。不级联销毁其他 shm_object，不动 service POD。
MC_API void shm_object_destroy(shm_allocator& alloc, shm_object* shadow) noexcept;

MC_API bool shm_object_set_class_name(shm_allocator& alloc, shm_object& shadow,
                                      std::string_view value) noexcept;
MC_API bool shm_object_set_name(shm_allocator& alloc, shm_object& shadow,
                                std::string_view value) noexcept;
MC_API bool shm_object_set_path(shm_allocator& alloc, shm_object& shadow,
                                std::string_view value) noexcept;
MC_API bool shm_object_set_position(shm_allocator& alloc, shm_object& shadow,
                                    std::string_view value) noexcept;

// parent / service 可为 nullptr。
MC_API void shm_object_set_parent(shm_object& shadow, shm_object* parent) noexcept;
MC_API void shm_object_set_service(shm_object& shadow, shm_service* service) noexcept;

MC_API bool shm_object_set_property_int64(shm_allocator& alloc, shm_object& shadow,
                                          std::string_view key, std::int64_t value) noexcept;
MC_API bool shm_object_set_property_double(shm_allocator& alloc, shm_object& shadow,
                                           std::string_view key, double value) noexcept;
MC_API bool shm_object_set_property_blob(shm_allocator& alloc, shm_object& shadow,
                                         std::string_view key, std::string_view blob,
                                         property_type_tag tag) noexcept;
MC_API bool shm_object_remove_property(shm_allocator& alloc, shm_object& shadow,
                                       std::string_view key) noexcept;

MC_API bool shm_object_add_child(shm_allocator& alloc, shm_object& shadow,
                                 shm_object* child) noexcept;
MC_API bool shm_object_remove_child(shm_object& shadow, shm_object* child) noexcept;

// 仅在绕过 ops API 直改 shadow 字段后调用。
MC_API void shm_object_refresh_crc(shm_object& shadow) noexcept;

// 重放/回滚最后一次未完成写入；失败由调用方打 isolated 标志。
MC_API void shm_object_journal_recover(shm_allocator& alloc, shm_object& shadow) noexcept;

// ---------------- 读取 ----------------

MC_API std::string_view shm_object_class_name(const shm_object& shadow) noexcept;
MC_API std::string_view shm_object_name(const shm_object& shadow) noexcept;
MC_API std::string_view shm_object_path(const shm_object& shadow) noexcept;
MC_API std::string_view shm_object_position(const shm_object& shadow) noexcept;

// 返回 true 表示 key 存在且类型匹配。
MC_API bool shm_object_get_property_int64(const shm_object& shadow, std::string_view key,
                                          std::int64_t& out) noexcept;
MC_API bool shm_object_get_property_double(const shm_object& shadow, std::string_view key,
                                           double& out) noexcept;
MC_API bool shm_object_get_property_blob(const shm_object& shadow, std::string_view key,
                                         std::string_view& out, property_type_tag& tag) noexcept;

MC_API std::uint16_t shm_object_property_count(const shm_object& shadow) noexcept;
MC_API std::uint16_t shm_object_child_count(const shm_object& shadow) noexcept;

MC_API shm_object*  shm_object_parent(const shm_object& shadow) noexcept;
MC_API shm_service* shm_object_service(const shm_object& shadow) noexcept;

}  // namespace mc::engine

#endif  // MC_ENGINE_SHM_OBJECT_OPS_H
