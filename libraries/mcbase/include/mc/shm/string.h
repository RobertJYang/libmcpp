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

#ifndef MC_SHM_STRING_H
#define MC_SHM_STRING_H

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <mc/common.h>
#include <mc/intrusive/offset_ptr.h>
#include <mc/shm/string_view.h>

namespace mc::shm {

class shm_allocator;

// mc::shm::string
//
// 共享内存中的 owning 字符串。
//
// 设计要点：
//   - 使用 mc::intrusive::offset_ptr<const char> 保存 buffer 指针：
//       * self-relative，跨进程 mmap 基址不同但相对距离恒定
//       * move/copy 自动重算 offset（由 offset_ptr::reset 实现）
//   - 不存 shm_allocator* 裸指针：析构 / destroy() 时通过进程级 region
//     registry 反查所属 region（ptr → user_base/user_size → 重建 allocator）
//   - Move-only：显式禁止拷贝；跨 region 复制必须通过 clone(alloc) 选址
//   - RAII：~string() 自动 destroy() 释放 buffer。若 string 不在任何已注册
//     region 中（例如在栈上），析构会静默清零字段，buffer 成为 leaked，
//     依赖 region-level fsck 扫描回收（同 set / list 等容器的降级语义）
//   - buffer 带 4B magic "MCSS"：destroy 幂等、可校验多进程竞争
//   - 不可增长：capacity == size；修改走 destroy + create 重建
//
// 典型用法：
//   // 1. 在 shm region 里分配（推荐）
//   mc::shm::shm_region region(...);
//   auto alloc = region.user_arena();
//   auto s = mc::shm::string::create(alloc, "/xyz/openbmc/obj");
//   // s 析构时自动 destroy()
//
//   // 2. 和容器配合（set / list 内部）
//   // 容器 insert / erase 会调 create / destroy(alloc)，用户无需手工管理

class MC_API string {
public:
    // buffer 头 magic："MCSS" little-endian uint32
    static constexpr std::uint32_t buffer_magic       = 0x5353434DU;
    // buffer 头尺寸（magic + size）
    static constexpr std::size_t   buffer_header_size = 8U;

    // 默认构造：空串
    constexpr string() noexcept = default;

    string(const string&)            = delete;
    string& operator=(const string&) = delete;

    // move：offset_ptr::reset 会根据新位置重算 offset；源置为空（防止双重 destroy）
    string(string&& other) noexcept;
    string& operator=(string&& other) noexcept;

    // RAII：若在已注册的 region 中，自动释放 buffer；
    // 否则静默清零字段，buffer 视为 leaked（由 region fsck 后续回收）
    ~string();

    // ---- 工厂 ----
    // 通过 alloc 分配 buffer 并复制 sv。sv 空时不分配 buffer。分配失败返回空串
    static string create(shm_allocator& alloc, std::string_view sv) noexcept;

    // 克隆：通过 alloc 分配新 buffer 复制本串内容
    string clone(shm_allocator& alloc) const noexcept;

    // ---- 显式释放 ----
    // 无参版：通过 registry 反查所属 region，重建 allocator 后释放。
    // 未注册（栈 / 已关闭 region）时静默清零字段，buffer 成 leaked
    void destroy() noexcept;

    // 有参版：显式传入 allocator。当调用者已持有 allocator（容器内部、
    // 或跳过 registry 的高级场景）时使用，不依赖 registry
    void destroy(shm_allocator& alloc) noexcept;

    // ---- 只读访问 ----
    [[nodiscard]] string_view        view() const noexcept;
    [[nodiscard]] std::string_view   std_view() const noexcept;
    [[nodiscard]] const char*        data() const noexcept;
    [[nodiscard]] std::size_t        size() const noexcept { return m_size; }
    [[nodiscard]] std::size_t        capacity() const noexcept { return m_capacity; }
    [[nodiscard]] bool               empty() const noexcept { return m_size == 0U; }

    // ---- 比较 ----
    bool operator==(const string& other) const noexcept;
    bool operator!=(const string& other) const noexcept { return !(*this == other); }
    bool operator<(const string& other) const noexcept;
    bool operator<=(const string& other) const noexcept { return !(other < *this); }
    bool operator>(const string& other) const noexcept { return other < *this; }
    bool operator>=(const string& other) const noexcept { return !(*this < other); }

    bool operator==(std::string_view sv) const noexcept;
    bool operator!=(std::string_view sv) const noexcept { return !(*this == sv); }
    bool operator<(std::string_view sv) const noexcept;

    // ---- 诊断 ----
    // 校验 buffer 头 magic 是否有效（空串恒 true）
    [[nodiscard]] bool buffer_intact() const noexcept;

private:
    // offset_ptr 指向 buffer header（payload 前 8B 是 magic+size）
    // 空串时为 null_offset
    mc::intrusive::offset_ptr<const std::byte> m_data;
    std::uint32_t                              m_size     = 0;
    std::uint32_t                              m_capacity = 0;
};

static_assert(sizeof(string) == 16U, "mc::shm::string 必须是 16B");
static_assert(alignof(string) == 8U, "mc::shm::string 必须是 8B 对齐");

// std::string_view 与 shm::string 的反向比较
inline bool operator==(std::string_view lhs, const string& rhs) noexcept { return rhs == lhs; }
inline bool operator!=(std::string_view lhs, const string& rhs) noexcept { return rhs != lhs; }
inline bool operator<(std::string_view lhs, const string& rhs) noexcept
{
    return !(rhs < lhs) && !(rhs == lhs);
}

// heterogeneous 比较器：给 container::set<string, string_less> 用，使 find/erase
// 既支持 string 键也支持 std::string_view 探针
struct string_less {
    bool operator()(const string& a, const string& b) const noexcept { return a < b; }
    bool operator()(const string& a, std::string_view b) const noexcept { return a < b; }
    bool operator()(std::string_view a, const string& b) const noexcept { return a < b; }
};

} // namespace mc::shm

#endif // MC_SHM_STRING_H
