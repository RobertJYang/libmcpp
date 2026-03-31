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

#ifndef MC_EVENT_H
#define MC_EVENT_H

#include <mc/common.h>

namespace mc {

using event_type_id = uint32_t;

constexpr event_type_id invalid_event_type           = 0;
constexpr event_type_id event_type_id_first_builtin  = 0x00000001u;
constexpr event_type_id event_type_id_last_builtin   = 0x00ffffffu;
constexpr event_type_id event_type_id_first_reserved = event_type_id_last_builtin + 1;
constexpr event_type_id event_type_id_first_user     = 0x80000000u;
constexpr event_type_id event_type_id_last_reserved  = event_type_id_first_user - 1;

namespace detail {
constexpr event_type_id event_hash_step(event_type_id hash, char ch) noexcept
{
    return (hash ^ static_cast<uint8_t>(ch)) * 16777619u;
}

constexpr event_type_id event_hash(const char* name) noexcept
{
    event_type_id hash = 2166136261u;
    while (name != nullptr && *name != '\0') {
        hash = event_hash_step(hash, *name);
        ++name;
    }
    return hash;
}

constexpr event_type_id normalize_builtin_event_type(event_type_id hash) noexcept
{
    constexpr event_type_id builtin_count = event_type_id_last_builtin - event_type_id_first_builtin + 1;
    return builtin_count == 0 ? invalid_event_type : (hash % builtin_count) + event_type_id_first_builtin;
}
} // namespace detail

class MC_API event {
public:
    explicit constexpr event(event_type_id type) noexcept : m_type(type)
    {}

    virtual ~event();

    constexpr event_type_id type() const noexcept
    {
        return m_type;
    }

    constexpr void accept() noexcept
    {
        m_accepted = true;
    }

    constexpr bool is_accepted() const noexcept
    {
        return m_accepted;
    }

private:
    event_type_id m_type{invalid_event_type};
    bool          m_accepted{false};
};

constexpr bool is_builtin_event_type(event_type_id id) noexcept
{
    return id >= event_type_id_first_builtin && id <= event_type_id_last_builtin;
}

constexpr bool is_reserved_event_type(event_type_id id) noexcept
{
    return id >= event_type_id_first_reserved && id <= event_type_id_last_reserved;
}

constexpr bool is_user_event_type(event_type_id id) noexcept
{
    return id >= event_type_id_first_user;
}

constexpr event_type_id builtin_event_type(const char* name) noexcept
{
    return detail::normalize_builtin_event_type(detail::event_hash(name));
}

constexpr event_type_id event_type(const char* name) noexcept
{
    return builtin_event_type(name);
}

constexpr event_type_id deferred_delete_event_type = builtin_event_type("mc.deferred_delete");

MC_API event_type_id register_event_type(mc::string_view name = {});

} // namespace mc

#endif // MC_EVENT_H
