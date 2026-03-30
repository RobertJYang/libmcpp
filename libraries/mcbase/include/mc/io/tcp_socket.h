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

#ifndef MC_IO_TCP_SOCKET_H
#define MC_IO_TCP_SOCKET_H

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <type_traits>
#include <utility>

#include <mc/common.h>
#include <mc/memory.h>
#include <mc/runtime/any_executor.h>
#include <mc/runtime/detail/completion_handler.h>
#include <mc/runtime/detail/aligned_buffer.h>

namespace mc::io {

class tcp_acceptor;

class MC_API tcp_socket : public mc::shared_base {
public:
    using connect_handler_type = mc::runtime::detail::completion_handler<void(const std::error_code&)>;
    using io_handler_type      = mc::runtime::detail::completion_handler<void(const std::error_code&, std::size_t)>;

    tcp_socket() noexcept = default;
    explicit tcp_socket(mc::runtime::any_executor executor);

    template <typename Executor,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<Executor>, tcp_socket> &&
                                          std::is_constructible_v<mc::runtime::any_executor, std::decay_t<Executor>>>>
    explicit tcp_socket(Executor&& executor) : tcp_socket(mc::runtime::any_executor(std::forward<Executor>(executor)))
    {}

    ~tcp_socket();

    tcp_socket(tcp_socket&& other) noexcept;
    tcp_socket& operator=(tcp_socket&& other) noexcept;

    tcp_socket(const tcp_socket&)            = delete;
    tcp_socket& operator=(const tcp_socket&) = delete;

    mc::runtime::any_executor get_executor() const noexcept
    {
        return m_executor;
    }

    bool is_open() const noexcept;
    void close();

    template <typename Handler>
    void async_connect(mc::string_view address, uint16_t port, Handler&& handler)
    {
        async_connect_impl(address, port, connect_handler_type(std::forward<Handler>(handler)));
    }

    template <typename Handler>
    void async_read_some(void* data, std::size_t length, Handler&& handler)
    {
        async_read_some_impl(data, length, io_handler_type(std::forward<Handler>(handler)));
    }

    template <typename Handler>
    void async_write_some(const void* data, std::size_t length, Handler&& handler)
    {
        async_write_some_impl(data, length, io_handler_type(std::forward<Handler>(handler)));
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

    void async_connect_impl(mc::string_view address, uint16_t port, connect_handler_type handler);
    void async_read_some_impl(void* data, std::size_t length, io_handler_type handler);
    void async_write_some_impl(const void* data, std::size_t length, io_handler_type handler);

    friend class tcp_acceptor;

    mc::runtime::any_executor                                        m_executor;
    bool                                                             m_initialized{false};
    mc::runtime::detail::aligned_buffer<storage_size, storage_align> m_storage;
};

} // namespace mc::io

#endif // MC_IO_TCP_SOCKET_H
