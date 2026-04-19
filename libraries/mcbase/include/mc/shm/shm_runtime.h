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

#ifndef MC_SHM_SHM_RUNTIME_H
#define MC_SHM_SHM_RUNTIME_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

#include <mc/common.h>
#include <mc/shm/container/list.h>
#include <mc/shm/container/map.h>
#include <mc/shm/container/set.h>
#include <mc/shm/region.h>
#include <mc/string.h>
#include <mc/string_view.h>

namespace mc::shm {

class mq_queue;
class ipc_mutex;
class ipc_shared_mutex;

enum class endpoint_state : std::uint32_t {
    inactive   = 0,
    starting   = 1,
    running    = 2,
    aborting   = 3,
    recovering = 4,
};

struct runtime_options {
    mc::string                region_name;
    std::size_t               region_size     = 2 * 1024 * 1024;
    std::size_t               root_capacity   = 64;
    bool                      manager_process = false;
    std::chrono::milliseconds heartbeat_timeout{2000};
};

struct endpoint {
    std::uint16_t  endpoint_id = 0;
    std::uint32_t  instance_id = 0;
    endpoint_state state       = endpoint_state::inactive;
    std::uint32_t  slot_count  = 0;
    mc::string     endpoint_name;
    mc::string     queue_name;
    mc::string     notifier_name;

    bool is_valid() const noexcept
    {
        return endpoint_id != 0 && instance_id != 0;
    }
};

struct endpoint_info {
    std::uint16_t  endpoint_id = 0;
    std::uint32_t  instance_id = 0;
    endpoint_state state       = endpoint_state::inactive;
    std::uint32_t  slot_count  = 0;
    mc::string     endpoint_name;
    mc::string     queue_name;
    mc::string     notifier_name;
    std::int64_t   owner_pid             = 0;
    std::uint64_t  heartbeat_deadline_ns = 0;
};

class MC_API shm_runtime {
public:
    static constexpr std::size_t max_endpoints        = 32;
    static constexpr std::size_t max_named_locks      = 32;
    static constexpr std::size_t max_named_lock_name  = 64;

    explicit shm_runtime(const runtime_options& options);
    ~shm_runtime();

    shm_runtime(const shm_runtime&)            = delete;
    shm_runtime& operator=(const shm_runtime&) = delete;

    shm_runtime(shm_runtime&& other) noexcept;
    shm_runtime& operator=(shm_runtime&& other) noexcept;

    bool              is_valid() const noexcept;
    bool              is_manager_process() const noexcept;
    shm_region&       region() noexcept;
    const shm_region& region() const noexcept;
    shm_allocator     user_arena() const noexcept;

    std::optional<endpoint>      register_endpoint(mc::string_view endpoint_name, std::size_t slot_count = 256);
    bool                         mark_endpoint_running(const endpoint& endpoint);
    bool                         heartbeat(const endpoint& endpoint);
    bool                         abort_endpoint(const endpoint& endpoint);
    std::size_t                  recover_stale_endpoints();
    std::optional<endpoint_info> get_endpoint(std::uint16_t endpoint_id) const;
    bool                         writer_instance_is_current(std::uint16_t endpoint_id, std::uint64_t instance_id) const;

    mq_queue open_queue(const endpoint& endpoint) const;

    // 命名锁工厂：名字在本 runtime 内唯一，相同名字与类型的后续调用返回同一对象。
    // 返回的指针生命周期与 runtime（shm region）一致，用户不应自行释放。
    // 返回 nullptr 表示失败（名称无效、类型冲突、注册表已满或分配失败）。
    ipc_mutex*        get_or_create_mutex(mc::string_view name);
    ipc_shared_mutex* get_or_create_shared_mutex(mc::string_view name);

    // ======================================================================
    // 命名容器工厂：按 name 定位或创建共享内存中的 list / set / map。
    //
    // 语义：
    //   1) 同一 runtime 内以 name 唯一；重复调用返回同一控制块上的 wrapper；
    //   2) wrapper 是轻量 2 指针对象（ctrl + allocator），按值返回，销毁不
    //      影响共享内存中的数据；多次调用可得到多个等价的 wrapper；
    //   3) 容器控制块生命周期与 runtime 持有的 shm region 一致；显式销毁需
    //      调用 drop_named_container；
    //   4) 类型安全：首次创建时记录 (kind, sizeof(K), sizeof(V), align)
    //      作为签名写入 root_record.generation；重开时若不匹配返回 nullopt；
    //   5) 崩溃安全：首次创建过程中若进程崩溃，control 块可能残留但尚未登记
    //      到 root_table，下次打开会自动走"找不到 → 重新分配"路径（残留块
    //      成为 arena 内的孤儿字节，后续 allocator 回收周期清理）。
    //
    // name 约束：非空、长度 < root name 容量减去 8 字节前缀。
    // 返回 std::nullopt 表示：名称非法 / 类型签名不匹配 / 分配失败 / 注册失败。
    template <typename T>
    std::optional<container::list<T>> get_or_create_list(mc::string_view name);

    template <typename Key, typename Compare = std::less<Key>>
    std::optional<container::set<Key, Compare>> get_or_create_set(mc::string_view name);

    template <typename Key, typename Value, typename Compare = std::less<Key>>
    std::optional<container::map<Key, Value, Compare>> get_or_create_map(mc::string_view name);

    // 从命名注册表移除。**调用者必须保证没有任何进程仍在使用该容器**。
    // 本方法会清空容器所有元素后释放 control 块；失败返回 false。
    bool drop_named_container(mc::string_view name) noexcept;

    // ---------------- 底层工厂 helper（非模板，供模板 wrapper 调用） -------
    //
    // kind_code：'L' / 'S' / 'M' 表示 list / set / map（未来若扩展容器类型
    // 请拓展此 enum）。同一 name 不同 kind 属于不同命名空间。
    struct named_container_spec {
        char            kind_code;
        mc::string_view name;
        std::size_t     ctrl_size;
        std::size_t     ctrl_align;
        std::uint64_t   signature;  // 跨进程类型一致性校验
        void          (*init_fn)(void* control, std::uint32_t self_tag);
    };
    struct named_container_handle {
        void*          control   = nullptr;
        shm_allocator* allocator = nullptr;
        explicit operator bool() const noexcept { return control != nullptr; }
    };
    named_container_handle ensure_named_container(const named_container_spec& spec) noexcept;

    bool drop_named_container_impl(char kind_code, mc::string_view name) noexcept;

private:
    struct impl;

    static std::uint64_t make_container_signature(char kind_code, std::uint32_t key_size,
                                                  std::uint32_t value_size, std::uint32_t key_align,
                                                  std::uint32_t value_align) noexcept;

    std::shared_ptr<impl> m_impl;
};

// ==========================================================================
// 模板实现（放在头末尾，避免侵入 public 区块）
// ==========================================================================

template <typename T>
std::optional<container::list<T>> shm_runtime::get_or_create_list(mc::string_view name)
{
    named_container_spec spec;
    spec.kind_code  = 'L';
    spec.name       = name;
    spec.ctrl_size  = sizeof(container::list_control);
    spec.ctrl_align = alignof(container::list_control);
    spec.signature  = make_container_signature('L', static_cast<std::uint32_t>(sizeof(T)), 0,
                                               static_cast<std::uint32_t>(alignof(T)), 0);
    spec.init_fn    = +[](void* ctrl, std::uint32_t tag) {
        container::list<T>::init(*static_cast<container::list_control*>(ctrl), tag);
    };

    auto handle = ensure_named_container(spec);
    if (!handle) {
        return std::nullopt;
    }
    return container::list<T>(*static_cast<container::list_control*>(handle.control),
                              *handle.allocator);
}

template <typename Key, typename Compare>
std::optional<container::set<Key, Compare>> shm_runtime::get_or_create_set(mc::string_view name)
{
    named_container_spec spec;
    spec.kind_code  = 'S';
    spec.name       = name;
    spec.ctrl_size  = sizeof(container::set_control);
    spec.ctrl_align = alignof(container::set_control);
    spec.signature  = make_container_signature('S', static_cast<std::uint32_t>(sizeof(Key)), 0,
                                               static_cast<std::uint32_t>(alignof(Key)), 0);
    spec.init_fn    = +[](void* ctrl, std::uint32_t tag) {
        container::set<Key, Compare>::init(*static_cast<container::set_control*>(ctrl), tag);
    };

    auto handle = ensure_named_container(spec);
    if (!handle) {
        return std::nullopt;
    }
    return container::set<Key, Compare>(*static_cast<container::set_control*>(handle.control),
                                        *handle.allocator);
}

template <typename Key, typename Value, typename Compare>
std::optional<container::map<Key, Value, Compare>>
shm_runtime::get_or_create_map(mc::string_view name)
{
    named_container_spec spec;
    spec.kind_code  = 'M';
    spec.name       = name;
    spec.ctrl_size  = sizeof(container::map_control);
    spec.ctrl_align = alignof(container::map_control);
    spec.signature  = make_container_signature(
        'M', static_cast<std::uint32_t>(sizeof(Key)), static_cast<std::uint32_t>(sizeof(Value)),
        static_cast<std::uint32_t>(alignof(Key)), static_cast<std::uint32_t>(alignof(Value)));
    spec.init_fn = +[](void* ctrl, std::uint32_t tag) {
        container::map<Key, Value, Compare>::init(*static_cast<container::map_control*>(ctrl), tag);
    };

    auto handle = ensure_named_container(spec);
    if (!handle) {
        return std::nullopt;
    }
    return container::map<Key, Value, Compare>(
        *static_cast<container::map_control*>(handle.control), *handle.allocator);
}

} // namespace mc::shm

#endif // MC_SHM_SHM_RUNTIME_H
