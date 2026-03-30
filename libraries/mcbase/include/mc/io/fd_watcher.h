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

#ifndef MC_IO_FD_WATCHER_H
#define MC_IO_FD_WATCHER_H

#include <cstddef>
#include <system_error>
#include <type_traits>
#include <utility>

#include <mc/common.h>
#include <mc/runtime/any_executor.h>
#include <mc/runtime/detail/completion_handler.h>
#include <mc/runtime/detail/aligned_buffer.h>

namespace mc::io {

class MC_API fd_watcher {
public:
    enum class wait_type {
        read,
        write,
    };

    using handler_type = mc::runtime::detail::completion_handler<void(const std::error_code&)>;

    fd_watcher(mc::runtime::any_executor executor, int fd);

    template <typename Executor,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<Executor>, fd_watcher> &&
                                          std::is_constructible_v<mc::runtime::any_executor, std::decay_t<Executor>>>>
    fd_watcher(Executor&& executor, int fd)
        : fd_watcher(mc::runtime::any_executor(std::forward<Executor>(executor)), fd)
    {}

    ~fd_watcher();

    fd_watcher(fd_watcher&&) noexcept;
    fd_watcher& operator=(fd_watcher&&) noexcept;

    fd_watcher(const fd_watcher&)            = delete;
    fd_watcher& operator=(const fd_watcher&) = delete;

    void close();

    template <typename Handler>
    void async_wait(wait_type type, Handler&& handler)
    {
        async_wait_impl(type, handler_type(std::forward<Handler>(handler)));
    }

private:
    static constexpr std::size_t storage_size  = 96;
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

#endif // MC_IO_FD_WATCHER_H
