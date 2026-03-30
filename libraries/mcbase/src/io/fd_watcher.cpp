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

#include <mc/io/fd_watcher.h>

#include <boost/asio/error.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

#include "runtime/include/any_executor_access.h"

namespace mc::io {
namespace {

using native_descriptor = boost::asio::posix::stream_descriptor;

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

} // namespace

fd_watcher::fd_watcher(mc::runtime::any_executor executor, int fd)
{
    static_assert(sizeof(native_descriptor) <= storage_size, "fd_watcher storage is too small");
    static_assert(alignof(native_descriptor) <= storage_align, "fd_watcher storage alignment is too small");

    new (m_storage.data()) native_descriptor(mc::runtime::detail::any_executor_access::get_native_io_executor(executor), fd);
}

fd_watcher::~fd_watcher()
{
    storage_as<native_descriptor>().~native_descriptor();
}

fd_watcher::fd_watcher(fd_watcher&& other) noexcept
{
    new (m_storage.data()) native_descriptor(std::move(other.storage_as<native_descriptor>()));
}

fd_watcher& fd_watcher::operator=(fd_watcher&& other) noexcept
{
    storage_as<native_descriptor>() = std::move(other.storage_as<native_descriptor>());
    return *this;
}

void fd_watcher::close()
{
    boost::system::error_code ec;
    storage_as<native_descriptor>().close(ec);
}

void fd_watcher::async_wait_impl(wait_type type, handler_type handler)
{
    auto wait_token = type == wait_type::read ? boost::asio::posix::descriptor_base::wait_read
                                              : boost::asio::posix::descriptor_base::wait_write;

    storage_as<native_descriptor>().async_wait(wait_token, [handler = std::move(handler)](const auto& ec) mutable {
        handler(to_std_error_code(ec));
    });
}

} // namespace mc::io
