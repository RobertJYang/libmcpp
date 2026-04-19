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
#include <mc/intrusive/offset_ptr.h>
#include <mc/shm/allocator.h>
#include <mc/shm/byte_string.h>

// mc::engine::shm_object_ops
//
// 围绕 shm_object / property_slab / child_slab 的写入/读取/释放工具集。
// 本文件提供两层 API：
//
//   1. 底层 slab ops：直接操作 property_slab / child_slab 指针，可用于精确控制
//      容量、CRC 时序的高级场景。set/add 在容量不足时**会重新分配 slab**，
//      通过 slab*& 入参回写新地址；旧 slab 在分配成功后被释放。
//
//   2. 高层 shm_object ops：对外暴露的"业务级"接口，自动处理 shadow.properties /
//      shadow.children 的更新 + CRC 刷新。绝大多数场景应使用此层。
//
// 设计契约：
//   - 失败语义：返回 false 时，可观测状态保持不变（要么全部完成要么全部不变）。
//     扩容失败不会损坏旧 slab 与 shadow。
//   - byte_string 生命周期：identity 字段 / property blob 的 byte_string 由 ops
//     全权管理；调用方只传 string_view，不需要也不应直接 destroy。
//   - children 的级联销毁是更高层（mcengine ownership 树）的责任，本层 destroy
//     只释放 slab 自身，不递归销毁 child shadow。
//
// 错误处理：所有函数 noexcept；分配失败统一返回 false / nullptr，绝不抛异常。

namespace mc::engine {

// ============================================================================
// 容量起步与增长策略
// ============================================================================

inline constexpr std::uint16_t property_slab_initial_capacity = 4U;
inline constexpr std::uint16_t child_slab_initial_capacity    = 4U;

// 按"翻倍 + 上限保护"扩容；返回 0 表示溢出（理论不可能）
MC_API std::uint16_t slab_grow_capacity(std::uint16_t current, std::uint16_t needed) noexcept;

// ============================================================================
// 底层 byte_string 帮助函数（在 SHM 内分配 byte_string struct + buffer）
// ============================================================================

// 分配并构造 byte_string struct（持有指向 SHM payload 的 offset_ptr）。
// sv 可为空，此时返回的 byte_string 表示空串（仍是 valid 对象）。
// 失败返回 nullptr。
MC_API mc::shm::byte_string* shm_byte_string_create(mc::shm::shm_allocator& alloc,
                                                    std::string_view       sv) noexcept;

// 析构 byte_string（自动释放 payload buffer）+ 释放 byte_string struct 自身。
// bs 为 nullptr 时为 no-op。
MC_API void shm_byte_string_destroy(mc::shm::shm_allocator& alloc,
                                    mc::shm::byte_string*   bs) noexcept;

// ============================================================================
// property_slab 底层 ops
// ============================================================================

// 在 SHM 中分配一个 property_slab（含 capacity 个 slot 的容量），CRC 已写入
// （初始 slot_count=0）。失败返回 nullptr。
MC_API property_slab* property_slab_create(mc::shm::shm_allocator& alloc,
                                           std::uint16_t           capacity) noexcept;

// 释放 slab：先逐个释放 slot 中持有的 key + v_blob byte_string，再 free slab 自身。
// slab 为 nullptr 时为 no-op。
MC_API void property_slab_destroy(mc::shm::shm_allocator& alloc,
                                  property_slab*          slab) noexcept;

// 在 slab 中按 key 线性查找（先比 key_hash，命中再比字节）。
// 找到返回 [0, slot_count)；未找到返回 -1。
MC_API int property_slab_find(const property_slab& slab, std::string_view key) noexcept;

// 设置 key→int64：覆盖 or 追加。容量不足时自动 grow（slab_inout 写新地址）。
// 失败返回 false（slab_inout 与 slab 内容均不变）。
MC_API bool property_slab_set_int64(mc::shm::shm_allocator& alloc, property_slab*& slab_inout,
                                    std::string_view key, std::int64_t value) noexcept;

// 设置 key→double：同上。
MC_API bool property_slab_set_double(mc::shm::shm_allocator& alloc, property_slab*& slab_inout,
                                     std::string_view key, double value) noexcept;

// 设置 key→string/bytes：blob 拷贝到新 byte_string；旧 v_blob（若存在）会被释放。
// tag 必须是 property_type_tag::string 或 property_type_tag::bytes。
MC_API bool property_slab_set_blob(mc::shm::shm_allocator& alloc, property_slab*& slab_inout,
                                   std::string_view key, std::string_view blob,
                                   property_type_tag tag) noexcept;

// 删除 key 对应的 slot（释放 key + v_blob byte_string）。
// 用 swap-and-pop 维持 [0, slot_count) 的紧凑布局。
// 不存在返回 false。
MC_API bool property_slab_remove(mc::shm::shm_allocator& alloc, property_slab& slab,
                                 std::string_view key) noexcept;

// ============================================================================
// child_slab 底层 ops
// ============================================================================

MC_API child_slab* child_slab_create(mc::shm::shm_allocator& alloc,
                                     std::uint16_t           capacity) noexcept;

// 仅释放 slab 自身；不级联销毁 child shadow。
MC_API void child_slab_destroy(mc::shm::shm_allocator& alloc, child_slab* slab) noexcept;

// 按指针等价性查找。找到返回索引；未找到返回 -1。
MC_API int child_slab_find(const child_slab& slab, shm_object* child) noexcept;

// 添加一个 child（重复添加同一个 child 视作 no-op，返回 true）。
// 容量不足时自动 grow。
MC_API bool child_slab_add(mc::shm::shm_allocator& alloc, child_slab*& slab_inout,
                           shm_object* child) noexcept;

// 删除指定 child（swap-and-pop）。不存在返回 false。
MC_API bool child_slab_remove(child_slab& slab, shm_object* child) noexcept;

// ============================================================================
// shm_object 高层 ops
// ============================================================================

// 创建新 shm_object：identity 字段 4 个 byte_string 已就绪；
// service / parent / properties / children 初始 null；flags=alive；CRC 已计算。
// 任一子分配失败则回滚已分配资源并返回 nullptr。
//
// service 归属与 parent 关系都通过单独的 setter 在 create 之后设置，避免 create 接口
// 参数列表过长，同时反映"对象创建早于 service attach 完成"的常见时序。
MC_API shm_object* shm_object_create(mc::shm::shm_allocator& alloc, std::uint64_t object_id,
                                           std::string_view class_name, std::string_view name,
                                           std::string_view path,
                                           std::string_view position) noexcept;

// 销毁 shm_object：释放 4 个 identity byte_string + properties slab(+slot blobs) + children slab。
// 不级联销毁 children/parent 指向的其他 shm_object，也不动 service POD。
// shadow 为 nullptr 时为 no-op。
MC_API void shm_object_destroy(mc::shm::shm_allocator& alloc, shm_object* shadow) noexcept;

// 单字段 setter：释放旧 byte_string，分配新的，更新 shadow CRC。
MC_API bool shm_object_set_class_name(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                         std::string_view value) noexcept;
MC_API bool shm_object_set_name(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                   std::string_view value) noexcept;
MC_API bool shm_object_set_path(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                   std::string_view value) noexcept;
MC_API bool shm_object_set_position(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                       std::string_view value) noexcept;

// 设置 parent（生命周期 owner，更新 offset_ptr + 刷新 CRC）。
// parent 可为 nullptr 表示根。
MC_API void shm_object_set_parent(shm_object& shadow, shm_object* parent) noexcept;

// 设置 service 归属（更新 offset_ptr + 刷新 CRC）。service 可为 nullptr 表示未归属
// （例如全局 object_table 中的 index-only 记录尚未关联到具体 service）。
MC_API void shm_object_set_service(shm_object& shadow, shm_service* service) noexcept;

// 高层 property setter：自动处理 shadow.properties null/扩容/CRC 刷新。
MC_API bool shm_object_set_property_int64(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                             std::string_view key, std::int64_t value) noexcept;
MC_API bool shm_object_set_property_double(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                              std::string_view key, double value) noexcept;
MC_API bool shm_object_set_property_blob(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                            std::string_view key, std::string_view blob,
                                            property_type_tag tag) noexcept;
MC_API bool shm_object_remove_property(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                          std::string_view key) noexcept;

// 高层 child ops：自动处理 shadow.children null/扩容/CRC 刷新。
MC_API bool shm_object_add_child(mc::shm::shm_allocator& alloc, shm_object& shadow,
                                    shm_object* child) noexcept;
MC_API bool shm_object_remove_child(shm_object& shadow, shm_object* child) noexcept;

// 重算并写入 shadow.crc32_self。
// 只有直接操纵 shadow 字段（绕过 ops API）后才需要手动调用。
MC_API void shm_object_refresh_crc(shm_object& shadow) noexcept;

// ============================================================================
// 读取辅助（reader）
// ============================================================================

// 读取 identity 字段为 string_view（shadow 中 byte_string 为空时返回空 view）。
MC_API std::string_view shm_object_class_name(const shm_object& shadow) noexcept;
MC_API std::string_view shm_object_name(const shm_object& shadow) noexcept;
MC_API std::string_view shm_object_path(const shm_object& shadow) noexcept;
MC_API std::string_view shm_object_position(const shm_object& shadow) noexcept;

// 读取 property（按 key）。
// 返回值：true=key 存在且类型匹配；false=key 不存在或类型不匹配。
MC_API bool shm_object_get_property_int64(const shm_object& shadow, std::string_view key,
                                             std::int64_t& out) noexcept;
MC_API bool shm_object_get_property_double(const shm_object& shadow, std::string_view key,
                                              double& out) noexcept;
// 对于 string/bytes：返回 view 指向 SHM 内 byte_string payload。
MC_API bool shm_object_get_property_blob(const shm_object& shadow, std::string_view key,
                                            std::string_view& out, property_type_tag& tag) noexcept;

// property 数量（slab 为 null 返回 0）。
MC_API std::uint16_t shm_object_property_count(const shm_object& shadow) noexcept;

// children 数量（slab 为 null 返回 0）。
MC_API std::uint16_t shm_object_child_count(const shm_object& shadow) noexcept;

// 关系字段读取（裸指针），未设置返回 nullptr。
MC_API shm_object*  shm_object_parent(const shm_object& shadow) noexcept;
MC_API shm_service* shm_object_service(const shm_object& shadow) noexcept;

}  // namespace mc::engine

#endif  // MC_ENGINE_SHM_OBJECT_OPS_H
