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

#ifndef MC_SIGNAL_CONNECTION_H
#define MC_SIGNAL_CONNECTION_H

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

#include <mc/common.h>

namespace mc {
namespace detail {
class connection_state;
class signal_backend;
} // namespace detail

class MC_API connection {
public:
    connection() noexcept = default;
    ~connection()         = default;

    void disconnect() noexcept;
    bool connected() const noexcept;

    explicit operator bool() const noexcept
    {
        return connected();
    }

private:
    explicit connection(std::shared_ptr<detail::connection_state> state) noexcept;
    std::uint64_t add_disconnect_callback(std::function<void()> callback) const;
    void          remove_disconnect_callback(std::uint64_t callback_id) const noexcept;

    std::shared_ptr<detail::connection_state> m_state;

    friend class detail::signal_backend;
    friend class connection_manager;
};

using connection_type = connection;

class MC_API scoped_connection {
public:
    scoped_connection() noexcept = default;
    explicit scoped_connection(connection conn) noexcept;
    ~scoped_connection();

    scoped_connection(scoped_connection&& other) noexcept;
    scoped_connection& operator=(scoped_connection&& other) noexcept;
    scoped_connection(const scoped_connection&)            = delete;
    scoped_connection& operator=(const scoped_connection&) = delete;

    connection release() noexcept;
    void       disconnect() noexcept;
    bool       connected() const noexcept;

private:
    connection m_connection;
};

class MC_API connection_manager {
public:
    connection_manager();
    ~connection_manager();
    connection_manager(const connection_manager&)                = delete;
    connection_manager& operator=(const connection_manager&)     = delete;
    connection_manager(connection_manager&& other) noexcept;
    connection_manager& operator=(connection_manager&& other) noexcept;

    void        add(connection conn);
    void        disconnect_all();
    bool        empty() const;
    std::size_t size() const;

private:
    struct state;
    std::shared_ptr<state> m_state;
};

} // namespace mc

#endif // MC_SIGNAL_CONNECTION_H
