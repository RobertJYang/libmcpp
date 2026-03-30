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

#include <mc/runtime/steady_timer.h>

#include <boost/asio/error.hpp>
#include <boost/asio/steady_timer.hpp>

#include "runtime/include/any_executor_access.h"

namespace mc::runtime {
namespace {

using native_timer = boost::asio::steady_timer;

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

steady_timer::steady_timer(any_executor executor)
{
    static_assert(sizeof(native_timer) <= storage_size, "steady_timer storage is too small");
    static_assert(alignof(native_timer) <= storage_align, "steady_timer storage alignment is too small");

    new (m_storage.data()) native_timer(detail::any_executor_access::get_native_io_executor(executor));
}

steady_timer::~steady_timer()
{
    storage_as<native_timer>().~native_timer();
}

steady_timer::steady_timer(steady_timer&& other) noexcept
{
    new (m_storage.data()) native_timer(std::move(other.storage_as<native_timer>()));
}

steady_timer& steady_timer::operator=(steady_timer&& other) noexcept
{
    storage_as<native_timer>() = std::move(other.storage_as<native_timer>());
    return *this;
}

void steady_timer::expires_after_impl(clock_type::duration duration)
{
    storage_as<native_timer>().expires_after(duration);
}

steady_timer::clock_type::time_point steady_timer::expiry() const
{
    return storage_as<native_timer>().expiry();
}

void steady_timer::cancel()
{
    storage_as<native_timer>().cancel();
}

void steady_timer::async_wait_impl(handler_type handler)
{
    storage_as<native_timer>().async_wait([handler = std::move(handler)](const auto& ec) mutable {
        handler(to_std_error_code(ec));
    });
}

} // namespace mc::runtime
