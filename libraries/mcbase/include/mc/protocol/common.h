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

#include <mc/string.h>
#include <mc/string_view.h>

#include <cstddef>
#include <cstdint>

namespace mc::proto {

class instance;

namespace detail {
class request_core;
}

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

enum class step_kind : uint8_t {
    push_next = 0,
    pop_next,
    suspend,
    complete,
    fail,
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

struct command {
    step_kind kind{step_kind::push_next};
};

class MC_API protocol {
public:
    virtual ~protocol();

    void set_parent(protocol* parent) noexcept
    {
        m_parent = parent;
    }

    [[nodiscard]] protocol* parent() const noexcept
    {
        return m_parent;
    }

    virtual bool on_continue(detail::request_core& request) noexcept
    {
        return m_parent != nullptr ? m_parent->on_continue(request) : false;
    }

    virtual bool on_failed(detail::request_core& request) noexcept
    {
        return m_parent != nullptr ? m_parent->on_failed(request) : false;
    }

    virtual void on_start(instance&)
    {}

    virtual void on_stop(instance&)
    {}

    void set_layer_index(std::size_t index) noexcept
    {
        m_layer_index = index;
    }

    [[nodiscard]] std::size_t layer_index() const noexcept
    {
        return m_layer_index;
    }

private:
    protocol*   m_parent{nullptr};
    std::size_t m_layer_index{0};
};

} // namespace mc::proto

#endif // MC_PROTOCOL_COMMON_H
