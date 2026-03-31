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

#ifndef MC_SIGNAL_SIGNAL_H
#define MC_SIGNAL_SIGNAL_H

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include <mc/signal/detail/backend.h>

namespace mc {

namespace signal_priority {
constexpr int critical = -1000;
constexpr int high     = -100;
constexpr int normal   = 0;
constexpr int low      = 100;
} // namespace signal_priority

template <typename Signature>
class signal;

template <typename R, typename... Args>
class signal<R(Args...)> {
public:
    using slot_type = std::function<R(Args...)>;

    explicit signal(const char* name = nullptr, std::size_t max_depth = 64)
        : m_backend(name, max_depth)
    {}

    ~signal() = default;

    signal(const signal&)            = delete;
    signal& operator=(const signal&) = delete;
    signal(signal&&)                 = delete;
    signal& operator=(signal&&)      = delete;

    const char* name() const noexcept
    {
        return m_backend.name();
    }

    template <typename SlotType>
    connection connect(SlotType&& slot, int priority = signal_priority::normal)
    {
        auto callable = std::make_shared<slot_type>(std::forward<SlotType>(slot));
        return m_backend.connect_erased(callable, priority);
    }

    void disconnect_all()
    {
        m_backend.disconnect_all();
    }

    bool empty() const
    {
        return m_backend.empty();
    }

    std::size_t slot_count() const
    {
        return m_backend.slot_count();
    }

    std::conditional_t<std::is_void_v<R>, void, std::optional<R>> operator()(Args... args) const
    {
        detail::signal_emit_guard guard(this, name(), m_backend.max_depth());
        auto                      snapshot = m_backend.snapshot();

        if constexpr (std::is_void_v<R>) {
            for (const auto& entry : *snapshot) {
                if (entry.state->connected()) {
                    auto* callable = static_cast<const slot_type*>(entry.callable.get());
                    (*callable)(args...);
                }
            }
            return;
        } else {
            std::optional<R> result;
            for (const auto& entry : *snapshot) {
                if (entry.state->connected()) {
                    auto* callable = static_cast<const slot_type*>(entry.callable.get());
                    result         = (*callable)(args...);
                }
            }
            return result;
        }
    }

private:
    detail::signal_backend m_backend;
};

} // namespace mc

#endif // MC_SIGNAL_SIGNAL_H
