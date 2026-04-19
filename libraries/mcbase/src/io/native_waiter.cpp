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

#include <mc/io/native_waiter.h>

#include <boost/asio/error.hpp>
#ifndef _WIN32
#include <boost/asio/posix/stream_descriptor.hpp>
#else
#include <boost/asio/windows/object_handle.hpp>
#include <windows.h>
#endif

#include "runtime/include/any_executor_access.h"

namespace mc::io {
namespace {

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

#ifndef _WIN32
using native_backend = boost::asio::posix::stream_descriptor;
#else
using native_backend = boost::asio::windows::object_handle;
#endif

} // namespace

native_waiter::native_target native_waiter::from_descriptor(int fd) noexcept
{
    return native_target{
        native_target::kind::descriptor,
        static_cast<std::uintptr_t>(fd),
    };
}

native_waiter::native_target native_waiter::from_waitable_handle(void* handle) noexcept
{
    return native_target{
        native_target::kind::waitable_handle,
        reinterpret_cast<std::uintptr_t>(handle),
    };
}

native_waiter::native_waiter(mc::runtime::any_executor executor, native_target target)
{
    static_assert(sizeof(native_backend) <= storage_size, "native_waiter storage is too small");
    static_assert(alignof(native_backend) <= storage_align, "native_waiter storage alignment is too small");

#ifndef _WIN32
    const auto descriptor_value =
        target.target_kind == native_target::kind::descriptor ? static_cast<int>(target.value) : -1;
    new (m_storage.data())
        native_backend(mc::runtime::detail::any_executor_access::get_native_io_executor(executor), descriptor_value);
#else
    const auto handle_value =
        target.target_kind == native_target::kind::waitable_handle ? reinterpret_cast<HANDLE>(target.value) : nullptr;
    new (m_storage.data())
        native_backend(mc::runtime::detail::any_executor_access::get_native_io_executor(executor), handle_value);
#endif
}

native_waiter::~native_waiter()
{
    storage_as<native_backend>().~native_backend();
}

native_waiter::native_waiter(native_waiter&& other) noexcept
{
    new (m_storage.data()) native_backend(std::move(other.storage_as<native_backend>()));
}

native_waiter& native_waiter::operator=(native_waiter&& other) noexcept
{
    storage_as<native_backend>() = std::move(other.storage_as<native_backend>());
    return *this;
}

void native_waiter::close()
{
    boost::system::error_code ec;
#ifndef _WIN32
    storage_as<native_backend>().cancel(ec);
#else
    storage_as<native_backend>().cancel(ec);
#endif
    storage_as<native_backend>().close(ec);
}

void native_waiter::async_wait_impl(wait_type type, handler_type handler)
{
#ifndef _WIN32
    if (type == wait_type::signal) {
        handler(std::make_error_code(std::errc::operation_not_supported));
        return;
    }

    const auto wait_token = type == wait_type::read ? boost::asio::posix::descriptor_base::wait_read
                                                    : boost::asio::posix::descriptor_base::wait_write;

    storage_as<native_backend>().async_wait(wait_token, [handler = std::move(handler)](const auto& ec) mutable {
        handler(to_std_error_code(ec));
    });
#else
    if (type != wait_type::signal) {
        handler(std::make_error_code(std::errc::operation_not_supported));
        return;
    }

    storage_as<native_backend>().async_wait([handler = std::move(handler)](const auto& ec) mutable {
        handler(to_std_error_code(ec));
    });
#endif
}

} // namespace mc::io
