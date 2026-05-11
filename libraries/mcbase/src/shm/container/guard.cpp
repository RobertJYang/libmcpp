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

#include "mc/shm/container/guard.h"

namespace mc::shm::container {

container_guard::container_guard(ipc_mutex& mutex, recover_fn recover, void* user_data) noexcept
    : m_mutex(&mutex), m_recovered(false)
{
    const auto state = m_mutex->lock_ex();
    if (state == lock_acquire_state::recovered) {
        m_recovered = true;
        if (recover != nullptr) {
            recover(user_data);
        }
    }
}

container_guard::~container_guard() noexcept
{
    if (m_mutex != nullptr) {
        m_mutex->unlock();
    }
}

} // namespace mc::shm::container
