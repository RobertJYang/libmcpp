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

#include <mc/engine/match/endpoint_sweep.h>

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM

#include <cstdint>
#include <limits>

#include <mc/shm/shm_runtime.h>

namespace mc::engine::match {

std::size_t reap_dead_endpoints(table& tbl, mc::shm::shm_runtime& runtime) noexcept
{
    if (!runtime.is_valid()) {
        return 0U;
    }

    // 先把心跳超时或 owner 进程已死的 endpoint 从 running/starting 切到 aborting。
    // 这一步是幂等的，多次调用不会重复回收。
    (void)runtime.recover_stale_endpoints();

    std::size_t total_reaped = 0U;

    // sweep 把 owner_instance_id < min_alive_instance_id 的全部清掉；这里我们
    // 用 max(uint32_t)：对于死亡 endpoint，所有残留都应该清除，无论 instance。
    constexpr std::uint32_t kAllInstances = std::numeric_limits<std::uint32_t>::max();

    // 枚举所有可能的 endpoint slot；shm_runtime endpoint_id 从 1 起。
    for (std::uint16_t ep = 1U;
         ep <= static_cast<std::uint16_t>(mc::shm::shm_runtime::max_endpoints);
         ++ep) {
        auto info = runtime.get_endpoint(ep);
        if (!info.has_value()) {
            // slot 未占用 / 永未注册：没东西可扫。
            continue;
        }

        const auto state = info->state;
        if (state == mc::shm::endpoint_state::starting
            || state == mc::shm::endpoint_state::running) {
            // 端点还活着或正在启动：不动。
            continue;
        }

        // 其余状态（inactive / aborting / recovering）都视为"该端点的旧实例
        // 已经退出"，挂在 SHM trie 上的订阅按死亡处理。
        total_reaped += tbl.sweep_dead_endpoint(ep, kAllInstances);
    }

    return total_reaped;
}

} // namespace mc::engine::match

#endif // MCENGINE_USE_SHM
