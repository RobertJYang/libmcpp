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

#ifndef MC_IO_NATIVE_WAITER_H
#define MC_IO_NATIVE_WAITER_H

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <type_traits>
#include <utility>

#include <mc/common.h>
#include <mc/runtime/any_executor.h>
#include <mc/runtime/detail/aligned_buffer.h>
#include <mc/runtime/detail/completion_handler.h>

namespace mc::io {

class MC_API native_waiter {
public:
    enum class wait_type {
        read,
        write,
        signal,
    };

    struct native_target {
        enum class kind {
            descriptor,
            waitable_handle,
        };

        kind           target_kind = kind::descriptor;
        std::uintptr_t value       = 0;
    };

    using handler_type = mc::runtime::detail::completion_handler<void(const std::error_code&)>;

    static native_target from_descriptor(int fd) noexcept;
    static native_target from_waitable_handle(void* handle) noexcept;

    template <typename Executor,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<Executor>, native_waiter> &&
                                          std::is_constructible_v<mc::runtime::any_executor, std::decay_t<Executor>>>>
    native_waiter(Executor&& executor, native_target target)
        : native_waiter(mc::runtime::any_executor(std::forward<Executor>(executor)), target)
    {}

    native_waiter(mc::runtime::any_executor executor, native_target target);

    ~native_waiter();

    native_waiter(native_waiter&& other) noexcept;
    native_waiter& operator=(native_waiter&& other) noexcept;

    native_waiter(const native_waiter&)            = delete;
    native_waiter& operator=(const native_waiter&) = delete;

    void close();

    template <typename Handler>
    void async_wait(wait_type type, Handler&& handler)
    {
        async_wait_impl(type, handler_type(std::forward<Handler>(handler)));
    }

private:
    static constexpr std::size_t storage_size  = 128;
    static constexpr std::size_t storage_align = alignof(std::max_align_t);

    void async_wait_impl(wait_type type, handler_type handler);

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

    mc::runtime::detail::aligned_buffer<storage_size, storage_align> m_storage;
};

} // namespace mc::io

#endif // MC_IO_NATIVE_WAITER_H
