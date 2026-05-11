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

#ifndef MC_RUNTIME_STEADY_TIMER_H
#define MC_RUNTIME_STEADY_TIMER_H

#include <chrono>
#include <cstddef>
#include <system_error>
#include <type_traits>
#include <utility>

#include <mc/common.h>
#include <mc/runtime/any_executor.h>
#include <mc/runtime/detail/completion_handler.h>
#include <mc/runtime/detail/aligned_buffer.h>

namespace mc::runtime {

class MC_API steady_timer {
public:
    using clock_type   = std::chrono::steady_clock;
    using handler_type = detail::completion_handler<void(const std::error_code&)>;

    explicit steady_timer(any_executor executor);

    template <typename Executor,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<Executor>, steady_timer> &&
                                          std::is_constructible_v<any_executor, std::decay_t<Executor>>>>
    explicit steady_timer(Executor&& executor) : steady_timer(any_executor(std::forward<Executor>(executor)))
    {}

    ~steady_timer();

    steady_timer(steady_timer&&) noexcept;
    steady_timer& operator=(steady_timer&&) noexcept;

    steady_timer(const steady_timer&)            = delete;
    steady_timer& operator=(const steady_timer&) = delete;

    template <typename Rep, typename Period>
    void expires_after(const std::chrono::duration<Rep, Period>& duration)
    {
        expires_after_impl(std::chrono::duration_cast<clock_type::duration>(duration));
    }

    clock_type::time_point expiry() const;
    void                   cancel();

    template <typename Handler>
    void async_wait(Handler&& handler)
    {
        async_wait_impl(handler_type(std::forward<Handler>(handler)));
    }

private:
    static constexpr std::size_t storage_size  = 128;
    static constexpr std::size_t storage_align = alignof(std::max_align_t);

    void expires_after_impl(clock_type::duration duration);
    void async_wait_impl(handler_type handler);

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

    detail::aligned_buffer<storage_size, storage_align> m_storage;
};

} // namespace mc::runtime

#endif // MC_RUNTIME_STEADY_TIMER_H
