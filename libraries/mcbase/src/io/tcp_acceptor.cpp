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

#include <mc/io/tcp_acceptor.h>

#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <mc/exception.h>

#include "runtime/include/any_executor_access.h"

namespace mc::io {
namespace {

using native_acceptor = boost::asio::ip::tcp::acceptor;
using native_socket   = boost::asio::ip::tcp::socket;

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

void throw_if_error(const boost::system::error_code& ec, const char* operation)
{
    if (!ec) {
        return;
    }

    MC_THROW(mc::runtime_exception, "tcp_acceptor ${operation} failed: ${message}",
             ("operation", operation)("message", ec.message()));
}

} // namespace

tcp_acceptor::tcp_acceptor(mc::runtime::any_executor executor) : m_executor(std::move(executor))
{
    static_assert(sizeof(native_acceptor) <= storage_size, "tcp_acceptor storage is too small");
    static_assert(alignof(native_acceptor) <= storage_align, "tcp_acceptor storage alignment is too small");

    new (m_storage.data())
        native_acceptor(mc::runtime::detail::any_executor_access::get_native_io_executor(m_executor));
}

tcp_acceptor::~tcp_acceptor()
{
    storage_as<native_acceptor>().~native_acceptor();
}

tcp_acceptor::tcp_acceptor(tcp_acceptor&& other) noexcept : m_executor(std::move(other.m_executor))
{
    new (m_storage.data()) native_acceptor(std::move(other.storage_as<native_acceptor>()));
}

tcp_acceptor& tcp_acceptor::operator=(tcp_acceptor&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    m_executor                    = std::move(other.m_executor);
    storage_as<native_acceptor>() = std::move(other.storage_as<native_acceptor>());
    return *this;
}

void tcp_acceptor::listen(uint16_t port, int backlog, bool reuse_address)
{
    auto& native = storage_as<native_acceptor>();

    boost::system::error_code ec;
    native.close(ec);
    ec.clear();

    native.open(boost::asio::ip::tcp::v4(), ec);
    throw_if_error(ec, "open");

    native.set_option(boost::asio::ip::tcp::acceptor::reuse_address(reuse_address), ec);
    throw_if_error(ec, "set_option");

    native.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port), ec);
    throw_if_error(ec, "bind");

    native.listen(backlog, ec);
    throw_if_error(ec, "listen");
}

uint16_t tcp_acceptor::local_port() const
{
    if (!is_open()) {
        return 0;
    }

    boost::system::error_code ec;
    auto                      endpoint = storage_as<native_acceptor>().local_endpoint(ec);
    if (ec) {
        return 0;
    }
    return endpoint.port();
}

bool tcp_acceptor::is_open() const noexcept
{
    return storage_as<native_acceptor>().is_open();
}

void tcp_acceptor::close()
{
    boost::system::error_code ec;
    storage_as<native_acceptor>().close(ec);
}

void tcp_acceptor::async_accept_impl(accept_handler_type handler)
{
    storage_as<native_acceptor>().async_accept([executor = m_executor, handler = std::move(handler)](
                                                   const auto& ec, native_socket accepted_native_socket) mutable {
        if (ec) {
            handler(to_std_error_code(ec), tcp_socket());
            return;
        }

        tcp_socket accepted_socket;
        accepted_socket.m_executor = std::move(executor);
        new (accepted_socket.m_storage.data()) native_socket(std::move(accepted_native_socket));
        accepted_socket.m_initialized = true;
        handler({}, std::move(accepted_socket));
    });
}

} // namespace mc::io
