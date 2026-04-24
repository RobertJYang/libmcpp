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

#ifndef MC_SHM_REGION_H
#define MC_SHM_REGION_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <mc/common.h>
#include <mc/shm/allocator.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/shared_mapping_view.h>
#include <mc/string.h>
#include <mc/string_view.h>

namespace mc::shm {

enum class root_kind : std::uint32_t {
    opaque          = 0,
    runtime_control = 1,
    intrusive_root  = 2,
};

struct shm_region_options {
    mc::string        segment_name;
    std::size_t       total_size    = 2 * 1024 * 1024;
    std::size_t       root_capacity = 64;
    detail::open_mode open_mode     = detail::open_mode::create_or_open;
};

struct root_record {
    mc::string    name;
    std::uint64_t offset     = 0;
    std::uint64_t size       = 0;
    root_kind     kind       = root_kind::opaque;
    std::uint64_t generation = 0;
};

class MC_API shm_region {
public:
    static constexpr std::uint32_t region_magic   = 0x4d435247;
    static constexpr std::uint32_t region_version = 2;
    // 容纳 8 字节命名容器前缀 + 完整 D-Bus 服务名 + ".object_table.idx<N>" 后缀
    static constexpr std::size_t max_root_name_size = 192;

    shm_region() noexcept;
    explicit shm_region(const shm_region_options& options);
    ~shm_region();

    shm_region(const shm_region&)            = delete;
    shm_region& operator=(const shm_region&) = delete;

    shm_region(shm_region&& other) noexcept;
    shm_region& operator=(shm_region&& other) noexcept;

    bool              is_valid() const noexcept;
    bool              created() const noexcept;
    const mc::string& name() const noexcept;
    void*             base() const noexcept;
    std::size_t       size() const noexcept;

    void*         user_base() const noexcept;
    std::size_t   user_size() const noexcept;
    shm_allocator user_arena() const noexcept;

    bool          contains(const void* ptr) const noexcept;
    std::uint64_t offset_of(const void* ptr) const noexcept;
    void*         address_from_offset(std::uint64_t offset) const noexcept;
    template <typename T>
    T* ptr_from_offset(std::uint64_t offset) const noexcept
    {
        return static_cast<T*>(address_from_offset(offset));
    }

    bool upsert_root(mc::string_view name, std::uint64_t offset, std::uint64_t size, root_kind kind = root_kind::opaque,
                     std::uint64_t generation = 0) noexcept;
    std::optional<root_record> insert_root_if_absent(mc::string_view name, std::uint64_t offset, std::uint64_t size,
                                                     root_kind     kind       = root_kind::opaque,
                                                     std::uint64_t generation = 0) noexcept;
    std::optional<root_record> find_root(mc::string_view name) const noexcept;
    bool                       erase_root(mc::string_view name) noexcept;
    std::vector<root_record>   list_roots() const;

private:
    struct impl;

    std::shared_ptr<impl> m_impl;
};

} // namespace mc::shm

#endif // MC_SHM_REGION_H
