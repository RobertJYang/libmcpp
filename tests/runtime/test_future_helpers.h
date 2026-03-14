/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_TEST_RUNTIME_TEST_FUTURE_HELPERS_H
#define MC_TEST_RUNTIME_TEST_FUTURE_HELPERS_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>

namespace mc::test::runtime {

class future_flag {
public:
    future_flag() : m_state(std::make_shared<state>())
    {}

    void set() const
    {
        if (!m_state->signaled.exchange(true, std::memory_order_acq_rel)) {
            m_state->promise.set_value();
        }
    }

    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout) const
    {
        return m_state->future.wait_for(timeout) == std::future_status::ready;
    }

private:
    struct state {
        state() : future(promise.get_future().share())
        {}

        std::promise<void>       promise;
        std::shared_future<void> future;
        std::atomic<bool>        signaled{false};
    };

    std::shared_ptr<state> m_state;
};

class countdown_future {
public:
    explicit countdown_future(int target) : m_state(std::make_shared<state>(target))
    {}

    void arrive() const
    {
        if (m_state->remaining.fetch_sub(1, std::memory_order_acq_rel) <= 1) {
            m_state->flag.set();
        }
    }

    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout) const
    {
        if (m_state->remaining.load(std::memory_order_acquire) <= 0) {
            m_state->flag.set();
        }
        return m_state->flag.wait_for(timeout);
    }

    int remaining() const
    {
        return std::max(0, m_state->remaining.load(std::memory_order_acquire));
    }

private:
    struct state {
        explicit state(int target) : remaining(target)
        {}

        std::atomic<int> remaining;
        future_flag      flag;
    };

    std::shared_ptr<state> m_state;
};

} // namespace mc::test::runtime

#endif // MC_TEST_RUNTIME_TEST_FUTURE_HELPERS_H
