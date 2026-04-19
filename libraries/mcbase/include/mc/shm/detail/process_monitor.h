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

#ifndef MC_SHM_DETAIL_PROCESS_MONITOR_H
#define MC_SHM_DETAIL_PROCESS_MONITOR_H

#include <cerrno>
#include <csignal>
#include <sys/types.h>

namespace mc::shm::detail {

inline bool is_process_alive(pid_t pid) noexcept
{
    if (pid <= 0) {
        return false;
    }

    const int rc = kill(pid, 0);
    return rc == 0 || errno == EPERM;
}

} // namespace mc::shm::detail

#endif // MC_SHM_DETAIL_PROCESS_MONITOR_H
