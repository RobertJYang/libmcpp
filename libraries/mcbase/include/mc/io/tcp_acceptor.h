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

#ifndef MC_IO_TCP_ACCEPTOR_H
#define MC_IO_TCP_ACCEPTOR_H

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <type_traits>
#include <utility>

#include <mc/common.h>
#include <mc/io/tcp_socket.h>
#include <mc/runtime/any_executor.h>
#include <mc/runtime/detail/completion_handler.h>
#include <mc/runtime/detail/aligned_buffer.h>

namespace mc::io {

class MC_API tcp_acceptor {
public:
    using accept_handler_type = mc::runtime::detail::completion_handler<void(const std::error_code&, tcp_socket)>;

    explicit tcp_acceptor(mc::runtime::any_executor executor);

    template <typename Executor,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<Executor>, tcp_acceptor> &&
                                          std::is_constructible_v<mc::runtime::any_executor, std::decay_t<Executor>>>>
    explicit tcp_acceptor(Executor&& executor)
        : tcp_acceptor(mc::runtime::any_executor(std::forward<Executor>(executor)))
    {}

    ~tcp_acceptor();

    tcp_acceptor(tcp_acceptor&& other) noexcept;
    tcp_acceptor& operator=(tcp_acceptor&& other) noexcept;

    tcp_acceptor(const tcp_acceptor&)            = delete;
    tcp_acceptor& operator=(const tcp_acceptor&) = delete;

    mc::runtime::any_executor get_executor() const noexcept
    {
        return m_executor;
    }

    void     listen(uint16_t port, int backlog = 16, bool reuse_address = true);
    uint16_t local_port() const;
    bool     is_open() const noexcept;
    void     close();

    template <typename Handler>
    void async_accept(Handler&& handler)
    {
        async_accept_impl(accept_handler_type(std::forward<Handler>(handler)));
    }

private:
    static constexpr std::size_t storage_size  = 96;
    static constexpr std::size_t storage_align = alignof(std::max_align_t);

    template <typename T>
    T& storage_as() noexcept
    {
        return *m_storage.template get<T>();
    }

    template <typename T>
    const T& storage_as() const noexcept
    {
        return *m_storage.template get<T>();
    }

    void async_accept_impl(accept_handler_type handler);

    mc::runtime::any_executor                                        m_executor;
    mc::runtime::detail::aligned_buffer<storage_size, storage_align> m_storage;
};

} // namespace mc::io

#endif // MC_IO_TCP_ACCEPTOR_H
