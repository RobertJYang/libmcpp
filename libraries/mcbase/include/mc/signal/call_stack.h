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

#ifndef MC_SIGNAL_CALL_STACK_H
#define MC_SIGNAL_CALL_STACK_H

#include <cstddef>

#include <mc/exception.h>

namespace mc {

struct signal_frame {
    const void* signal_addr{nullptr};
    const char* signal_name{nullptr};
};

class signal_call_stack_view {
public:
    using value_type     = signal_frame;
    using const_iterator = const value_type*;
    using size_type      = std::size_t;

    constexpr signal_call_stack_view() noexcept = default;
    constexpr signal_call_stack_view(const value_type* data, size_type size) noexcept : m_data(data), m_size(size)
    {}

    constexpr const value_type& operator[](size_type index) const noexcept
    {
        return m_data[index];
    }

    constexpr const_iterator begin() const noexcept
    {
        return m_data;
    }

    constexpr const_iterator end() const noexcept
    {
        return m_data + m_size;
    }

    constexpr const value_type* data() const noexcept
    {
        return m_data;
    }

    constexpr size_type size() const noexcept
    {
        return m_size;
    }

    constexpr bool empty() const noexcept
    {
        return m_size == 0;
    }

private:
    const value_type* m_data{nullptr};
    size_type         m_size{0};
};

MC_DECLARE_EXCEPTION_CLASS(signal_recursion_exception, signal_recursion_exception_code, "信号循环调用",
                           "signal_recursion_exception");

MC_API signal_call_stack_view current_signal_call_stack() noexcept;
MC_API std::size_t current_signal_depth() noexcept;

} // namespace mc

#endif // MC_SIGNAL_CALL_STACK_H
