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

#ifndef MC_SHM_BYTE_STRING_H
#define MC_SHM_BYTE_STRING_H

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <mc/common.h>
#include <mc/intrusive/offset_ptr.h>

namespace mc::shm {

class shm_allocator;

// mc::shm::byte_string
//
// 共享内存中的 owning 字节序列（不透明 BLOB）。
//
// 与 mc::shm::string 的关系：
//   - 两者底层架构完全对称：buffer header(magic + size) + payload，self-relative
//     offset_ptr，move-only，auto-destroy-via-registry。
//   - 唯一差异是语义：
//       * string       = 逻辑上是字符串（通常带可读编码，比较时也是字符串语义）
//       * byte_string  = 逻辑上是不透明字节序列，允许内嵌 '\0'，比较纯粹按字节
//                        memcmp。用途：拼接索引键（interface + '\0' + path），
//                        序列化过的业务键等
//   - magic 不同："MCBY" vs "MCSS"。同一 region 里两种串不会互相误用
//
// 典型用法：
//   mc::shm::shm_region region(...);
//   auto alloc = region.user_arena();
//   // 直接从 span-like data 建串
//   auto k = mc::shm::byte_string::create(alloc, "org.xxx.Interface\0/a/b/c", 24);
//   // 或从 string_view 建串（长度按 sv.size()，允许内含 '\0'）
//   auto k2 = mc::shm::byte_string::create(alloc, sv);

class MC_API byte_string {
public:
    // buffer 头 magic："MCBY" little-endian uint32
    static constexpr std::uint32_t buffer_magic       = 0x5942434DU;
    static constexpr std::size_t   buffer_header_size = 8U;

    constexpr byte_string() noexcept = default;

    byte_string(const byte_string&)            = delete;
    byte_string& operator=(const byte_string&) = delete;

    byte_string(byte_string&& other) noexcept;
    byte_string& operator=(byte_string&& other) noexcept;

    ~byte_string();

    // ---- 工厂 ----
    // 指定指针 + 长度；data 可为 nullptr 当且仅当 size == 0
    static byte_string create(shm_allocator& alloc, const void* data, std::size_t size) noexcept;

    // 便利版：以 std::string_view 为数据源（视 sv 为不透明字节序列）
    static byte_string create(shm_allocator& alloc, std::string_view sv) noexcept;

    // clone：用 alloc 复制本串内容
    byte_string clone(shm_allocator& alloc) const noexcept;

    // ---- 显式释放 ----
    void destroy() noexcept;
    void destroy(shm_allocator& alloc) noexcept;

    // ---- 只读访问 ----
    [[nodiscard]] const std::byte* data() const noexcept;
    [[nodiscard]] std::size_t      size() const noexcept { return m_size; }
    [[nodiscard]] std::size_t      capacity() const noexcept { return m_capacity; }
    [[nodiscard]] bool             empty() const noexcept { return m_size == 0U; }

    // 以 std::string_view 形态暴露字节序列（便于与文本键/日志互转）
    [[nodiscard]] std::string_view as_string_view() const noexcept;

    // ---- 比较（按字节 memcmp）----
    bool operator==(const byte_string& other) const noexcept;
    bool operator!=(const byte_string& other) const noexcept { return !(*this == other); }
    bool operator<(const byte_string& other) const noexcept;
    bool operator<=(const byte_string& other) const noexcept { return !(other < *this); }
    bool operator>(const byte_string& other) const noexcept { return other < *this; }
    bool operator>=(const byte_string& other) const noexcept { return !(*this < other); }

    bool operator==(std::string_view sv) const noexcept;
    bool operator!=(std::string_view sv) const noexcept { return !(*this == sv); }
    bool operator<(std::string_view sv) const noexcept;

    // ---- 诊断 ----
    [[nodiscard]] bool buffer_intact() const noexcept;

private:
    mc::intrusive::offset_ptr<const std::byte> m_data;
    std::uint32_t                              m_size     = 0;
    std::uint32_t                              m_capacity = 0;
};

static_assert(sizeof(byte_string) == 16U, "mc::shm::byte_string 必须是 16B");
static_assert(alignof(byte_string) == 8U, "mc::shm::byte_string 必须是 8B 对齐");

inline bool operator==(std::string_view lhs, const byte_string& rhs) noexcept { return rhs == lhs; }
inline bool operator!=(std::string_view lhs, const byte_string& rhs) noexcept { return rhs != lhs; }
inline bool operator<(std::string_view lhs, const byte_string& rhs) noexcept
{
    return !(rhs < lhs) && !(rhs == lhs);
}

// heterogeneous 比较器：给 container::set<byte_string, byte_string_less> 用
struct byte_string_less {
    bool operator()(const byte_string& a, const byte_string& b) const noexcept { return a < b; }
    bool operator()(const byte_string& a, std::string_view b) const noexcept { return a < b; }
    bool operator()(std::string_view a, const byte_string& b) const noexcept { return a < b; }
};

} // namespace mc::shm

#endif // MC_SHM_BYTE_STRING_H
