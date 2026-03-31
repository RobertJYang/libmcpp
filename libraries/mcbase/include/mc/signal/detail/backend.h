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

#ifndef MC_SIGNAL_DETAIL_BACKEND_H
#define MC_SIGNAL_DETAIL_BACKEND_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include <mc/common.h>
#include <mc/signal/call_stack.h>
#include <mc/signal/connection.h>

namespace mc::detail {

class MC_API connection_state {
public:
    connection_state() noexcept = default;

    bool connected() const noexcept;
    void disconnect() noexcept;

private:
    std::atomic<bool> m_connected{true};
};

struct slot_record {
    int                               priority{0};
    std::uint64_t                     id{0};
    std::shared_ptr<void>             callable;
    std::shared_ptr<connection_state> state;
};

using slot_record_list = std::vector<slot_record>;

class MC_API signal_emit_guard {
public:
    signal_emit_guard(const void* signal_addr, const char* signal_name,
                      std::size_t max_depth = 64);
    ~signal_emit_guard() noexcept;

    std::size_t depth() const noexcept;

private:
    std::size_t m_depth{0};
};

class MC_API signal_backend {
public:
    explicit signal_backend(const char* name = nullptr, std::size_t max_depth = 64);
    ~signal_backend();

    const char* name() const noexcept;

    connection  connect_erased(std::shared_ptr<void> callable, int priority);
    void        disconnect_all();
    bool        empty() const;
    std::size_t slot_count() const;
    std::size_t max_depth() const noexcept;

    std::shared_ptr<const slot_record_list> snapshot() const;

private:
    void        ensure_unique_slots_locked();
    void        compact_locked();
    std::size_t active_slot_count_locked() const;

    const char*                       m_name{nullptr};
    std::size_t                       m_max_depth{64};
    mutable std::mutex                m_mutex;
    std::shared_ptr<slot_record_list> m_slots;
    std::uint64_t                     m_next_id{0};
};

} // namespace mc::detail

#endif // MC_SIGNAL_DETAIL_BACKEND_H
