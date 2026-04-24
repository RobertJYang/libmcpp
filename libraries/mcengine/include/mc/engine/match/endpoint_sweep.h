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

#ifndef MC_ENGINE_MATCH_ENDPOINT_SWEEP_H
#define MC_ENGINE_MATCH_ENDPOINT_SWEEP_H

#include <mc/common.h>
#include <mc/engine/match/table.h>

#include <cstddef>

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM

namespace mc::shm {
class shm_runtime;
} // namespace mc::shm

namespace mc::engine::match {

// 清除 shm_runtime 中已死亡 endpoint 在 table 中的残留订阅。
MC_API std::size_t reap_dead_endpoints(table& table, mc::shm::shm_runtime& runtime) noexcept;

} // namespace mc::engine::match

#endif // MCENGINE_USE_SHM

#endif // MC_ENGINE_MATCH_ENDPOINT_SWEEP_H
