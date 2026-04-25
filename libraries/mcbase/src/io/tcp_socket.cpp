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

#include <mc/io/tcp_socket.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "runtime/include/any_executor_access.h"

namespace mc::io {
namespace {

using native_socket = boost::asio::ip::tcp::socket;

std::error_code to_std_error_code(const boost::system::error_code& ec)
{
    if (!ec) {
        return {};
    }

    if (ec == boost::asio::error::operation_aborted) {
        return std::make_error_code(std::errc::operation_canceled);
    }

    return std::error_code(ec.value(), std::system_category());
}

std::error_code bad_socket_error()
{
    return std::make_error_code(std::errc::bad_file_descriptor);
}

} // namespace

tcp_socket::tcp_socket(mc::runtime::any_executor executor) : m_executor(std::move(executor)), m_initialized(true)
{
    static_assert(sizeof(native_socket) <= storage_size, "tcp_socket storage is too small");
    static_assert(alignof(native_socket) <= storage_align, "tcp_socket storage alignment is too small");

    new (m_storage.data()) native_socket(mc::runtime::detail::any_executor_access::get_native_io_executor(m_executor));
}

tcp_socket::~tcp_socket()
{
    if (m_initialized) {
        storage_as<native_socket>().~native_socket();
    }
}

tcp_socket::tcp_socket(tcp_socket&& other) noexcept
    : m_executor(std::move(other.m_executor)), m_initialized(other.m_initialized)
{
    if (m_initialized) {
        new (m_storage.data()) native_socket(std::move(other.storage_as<native_socket>()));
    }
}

tcp_socket& tcp_socket::operator=(tcp_socket&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    if (m_initialized) {
        storage_as<native_socket>().~native_socket();
    }

    m_executor    = std::move(other.m_executor);
    m_initialized = other.m_initialized;
    if (m_initialized) {
        new (m_storage.data()) native_socket(std::move(other.storage_as<native_socket>()));
    }

    return *this;
}

bool tcp_socket::is_open() const noexcept
{
    return m_initialized && storage_as<native_socket>().is_open();
}

void tcp_socket::close()
{
    if (!m_initialized) {
        return;
    }

    boost::system::error_code ec;
    storage_as<native_socket>().close(ec);
}

void tcp_socket::async_connect_impl(mc::string_view address, uint16_t port, connect_handler_type handler)
{
    if (!m_initialized) {
        handler(bad_socket_error());
        return;
    }

    boost::system::error_code ec;
    auto native_address = boost::asio::ip::make_address(std::string(address.data(), address.size()), ec);
    if (ec) {
        handler(to_std_error_code(ec));
        return;
    }

    storage_as<native_socket>().async_connect(boost::asio::ip::tcp::endpoint(native_address, port),
                                              [handler = std::move(handler)](const auto& connect_ec) mutable {
        handler(to_std_error_code(connect_ec));
    });
}

void tcp_socket::async_read_some_impl(void* data, std::size_t length, io_handler_type handler)
{
    if (!m_initialized) {
        handler(bad_socket_error(), 0);
        return;
    }

    storage_as<native_socket>().async_read_some(
        boost::asio::buffer(data, length),
        [handler = std::move(handler)](const auto& ec, std::size_t transferred) mutable {
        handler(to_std_error_code(ec), transferred);
    });
}

void tcp_socket::async_write_some_impl(const void* data, std::size_t length, io_handler_type handler)
{
    if (!m_initialized) {
        handler(bad_socket_error(), 0);
        return;
    }

    storage_as<native_socket>().async_write_some(
        boost::asio::buffer(data, length),
        [handler = std::move(handler)](const auto& ec, std::size_t transferred) mutable {
        handler(to_std_error_code(ec), transferred);
    });
}

} // namespace mc::io
