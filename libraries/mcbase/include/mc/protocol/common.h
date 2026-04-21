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

#ifndef MC_PROTOCOL_COMMON_H
#define MC_PROTOCOL_COMMON_H

#include <mc/common.h>
#include <mc/string.h>
#include <mc/string_view.h>

#include <cstdint>

namespace mc::proto {

class protocol;

enum class flow_direction : uint8_t {
    push = 0,
    pop  = 1,
};

enum class execution_state : uint8_t {
    idle = 0,
    running,
    suspended,
    completed,
    failed,
};

struct protocol_error {
    mc::string name;
    mc::string message;

    void clear()
    {
        name.clear();
        message.clear();
    }
};

class protocol_list_view {
public:
    using value_type = protocol*;
    using iterator   = protocol* const*;

    constexpr protocol_list_view() noexcept = default;

    constexpr protocol_list_view(protocol* const* data, std::size_t size) noexcept : m_data(data), m_size(size)
    {}

    constexpr iterator begin() const noexcept
    {
        return m_data;
    }

    constexpr iterator end() const noexcept
    {
        return m_data + m_size;
    }

    constexpr protocol* operator[](std::size_t index) const noexcept
    {
        return m_data[index];
    }

    constexpr protocol* const* data() const noexcept
    {
        return m_data;
    }

    constexpr std::size_t size() const noexcept
    {
        return m_size;
    }

    constexpr bool empty() const noexcept
    {
        return m_size == 0;
    }

private:
    protocol* const* m_data{nullptr};
    std::size_t      m_size{0};
};

} // namespace mc::proto

#endif // MC_PROTOCOL_COMMON_H
