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

#ifndef MC_ENGINE_MATCH_INTERNAL_SHARED_TRIE_LAYOUT_H
#define MC_ENGINE_MATCH_INTERNAL_SHARED_TRIE_LAYOUT_H

#include <mc/shm/byte_string.h>
#include <mc/shm/container/map.h>
#include <mc/shm/string.h>

#include <atomic>
#include <cstdint>

// SHM 层次化 trie 布局（shared_table 的物理存储）。
//
// 编码顺序：type → sender → destination → path-segments... → PATH_END → interface → member。
// 叶子链挂在 member 层 trie_node 上。全局 ipc_shared_mutex 保护所有写操作。

namespace mc::engine::match::detail {

constexpr std::uint32_t shared_root_magic   = 0x4954524DU; // "MTRI" (Match TRie Init)
constexpr std::uint32_t shared_root_version = 1U;

inline constexpr mc::string_view shared_root_name = "mcengine.match.shared_root";
inline constexpr mc::string_view shared_lock_name = "mcengine.match.shared_lock";

// path 段走完后插入的 sentinel，让 trie 在 path 之后转去 iface 层。
inline constexpr mc::string_view path_end_sentinel = "$E";

inline constexpr mc::string_view wildcard_single = "*";
inline constexpr mc::string_view wildcard_multi  = "**";

// trie 的字段层 kind。
enum class trie_level : std::uint8_t {
    type           = 0,
    sender         = 1,
    destination    = 2,
    path           = 3, // 变深；child key 是 path segment / "*" / "**" / PATH_END
    interface_name = 4,
    member_name    = 5,
};

// trie_node：层次化 trie 节点。POD 布局，所有引用通过 self-relative offset。
struct trie_node {
    mc::shm::container::map_control children;
    std::int64_t                    leaf_list_head;
    std::uint8_t                    level;           // trie_level
    std::uint8_t                    _pad[7];
};

// 单个 region 内最多挂的 endpoint 数。
constexpr std::uint32_t shared_max_endpoints = 32U;

// subscription_entry：trie 叶子链的订阅条目。双向 leaf 链 + 双向 endpoint 链。
struct subscription_entry {
    std::uint64_t           match_id;           // 全局唯一，0 非法
    std::uint16_t           owner_endpoint_id;  // shm_runtime endpoint id，0=本地默认
    std::uint16_t           _pad0;
    std::uint32_t           owner_instance_id;
    std::uint32_t           filter_backend_type; // mc::engine::match::no_filter 等
    std::uint32_t           _pad1;
    mc::shm::byte_string    service_name;        // owning，arena 内
    mc::shm::byte_string    filter_text;         // owning，arena 内（spec.text 的拷贝）
    std::int64_t            leaf_node_offset;    // back-ptr 到所属 leaf trie_node
    std::int64_t            prev_in_leaf;        // self-relative offset，0=表头
    std::int64_t            next_in_leaf;        // self-relative offset，0=表尾
    std::int64_t            prev_in_endpoint;    // self-relative offset，0=表头
    std::int64_t            next_in_endpoint;    // self-relative offset，0=表尾
};

// shared_root：trie 根 + 全局 ID 计数器。挂在 shm_region 的 root_table 上。
struct shared_root {
    std::uint32_t              magic;             // shared_root_magic
    std::uint32_t              version;           // shared_root_version
    std::int64_t               root_node_offset;  // self-relative 相对本 shared_root，指向 type 层 trie_node
    std::atomic<std::uint64_t> next_match_id;     // 全局递增；0 不分配
    std::int64_t               endpoint_lists[shared_max_endpoints];
    std::uint8_t               _reserved[40];
};

} // namespace mc::engine::match::detail

#endif // MC_ENGINE_MATCH_INTERNAL_SHARED_TRIE_LAYOUT_H
