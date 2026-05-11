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

/**
 * @file quark.h
 * @brief mc::quark —— 全局唯一的常量字符串 atom，比较与拷贝 O(1)
 *
 * 仅字面量入口可写表；运行期仅支持 try_from() 查询。
 */
#ifndef MC_QUARK_H
#define MC_QUARK_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>

#include <mc/common.h>
#include <mc/string_view.h>

namespace mc {

class string;

class MC_API quark {
public:
    using id_type = std::uint32_t;

    // 0 = invalid, 1 = empty
    static constexpr id_type invalid_id = 0;
    static constexpr id_type empty_id   = 1;

    constexpr quark() noexcept = default;
    constexpr explicit quark(id_type id) noexcept : m_id{id}
    {}

    /// 运行期查询入口；找不到返回 invalid
    static quark try_from(mc::string_view value) noexcept;

    constexpr id_type id() const noexcept { return m_id; }
    constexpr bool    valid() const noexcept { return m_id != invalid_id; }
    constexpr explicit operator bool() const noexcept { return valid(); }
    bool              empty() const noexcept { return size() == 0U; }

    mc::string_view view() const noexcept;
    const char*     c_str() const noexcept;
    std::size_t     size() const noexcept;
    std::size_t     hash() const noexcept;

    /// view + hash 的打包快照
    struct descriptor_t {
        mc::string_view view{};
        std::size_t     hash{0U};
        bool            valid() const noexcept { return view.data() != nullptr; }
        std::size_t     size() const noexcept { return view.size(); }
    };

    descriptor_t descriptor() const noexcept;

    constexpr bool operator==(quark other) const noexcept
    {
        return m_id == other.m_id;
    }

    constexpr bool operator!=(quark other) const noexcept
    {
        return m_id != other.m_id;
    }

    constexpr bool operator<(quark other) const noexcept
    {
        return m_id < other.m_id;
    }

    operator mc::string_view() const noexcept
    {
        return view();
    }

    operator std::string_view() const noexcept
    {
        const auto v = view();
        return std::string_view(v.data(), v.size());
    }

    operator std::string() const
    {
        const auto v = view();
        return std::string(v.data(), v.size());
    }

    bool operator==(mc::string_view rhs) const noexcept
    {
        return view() == rhs;
    }

    bool operator!=(mc::string_view rhs) const noexcept
    {
        return !(*this == rhs);
    }

    bool operator==(const char* rhs) const noexcept
    {
        return view() == mc::string_view(rhs);
    }

    bool operator!=(const char* rhs) const noexcept
    {
        return !(*this == rhs);
    }

    bool operator==(const std::string& rhs) const noexcept
    {
        return view() == mc::string_view(rhs.data(), rhs.size());
    }

    bool operator!=(const std::string& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    bool operator==(std::string_view rhs) const noexcept
    {
        return view() == mc::string_view(rhs.data(), rhs.size());
    }

    bool operator!=(std::string_view rhs) const noexcept
    {
        return !(*this == rhs);
    }

private:
    id_type m_id{invalid_id};
};

inline std::ostream& operator<<(std::ostream& os, quark q)
{
    const auto v = q.view();
    return os.write(v.data(), static_cast<std::streamsize>(v.size()));
}

inline bool operator==(mc::string_view lhs, quark rhs) noexcept
{
    return rhs == lhs;
}

inline bool operator!=(mc::string_view lhs, quark rhs) noexcept
{
    return rhs != lhs;
}

inline bool operator==(const char* lhs, quark rhs) noexcept
{
    return rhs == lhs;
}

inline bool operator!=(const char* lhs, quark rhs) noexcept
{
    return rhs != lhs;
}

MC_API quark operator""_q(const char* literal, std::size_t length);

namespace detail {

struct quark_slot {
    const char*                literal;
    std::uint32_t              length;
    std::atomic<std::uint32_t> id;
};

static_assert(sizeof(quark_slot) == 16U, "quark_slot 必须保持 16B 以稳定段内布局");

/// 仅供字面量入口使用
MC_API quark::id_type intern_trusted_literal(mc::string_view value) noexcept;

MC_API quark::id_type slow_intern_slot(quark_slot& slot) noexcept;

inline quark::id_type fast_quark_id(quark_slot& slot) noexcept
{
    auto cached = slot.id.load(std::memory_order_acquire);
    if (MC_LIKELY(cached != quark::invalid_id)) {
        return cached;
    }
    return slow_intern_slot(slot);
}

} // namespace detail
} // namespace mc

// Linux ELF 用链接段批量注册；其他平台首次调用时注册
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
#define MC_QUARK_SECTION __attribute__((used, section("mc_quark_slots"), aligned(8)))
#else
#define MC_QUARK_SECTION
#endif

#define MC_QUARK(literal_str)                                                                                          \
    ([]() noexcept -> ::mc::quark {                                                                                    \
        static ::mc::detail::quark_slot _mc_quark_slot MC_QUARK_SECTION{(literal_str),                                 \
                                                                        sizeof(literal_str) - 1U, {0U}};               \
        return ::mc::quark{::mc::detail::fast_quark_id(_mc_quark_slot)};                                               \
    }())

namespace std {

template <>
struct hash<mc::quark> {
    std::size_t operator()(mc::quark q) const noexcept
    {
        return q.hash();
    }
};

} // namespace std

#endif // MC_QUARK_H
