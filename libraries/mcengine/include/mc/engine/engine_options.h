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

#ifndef MC_ENGINE_ENGINE_OPTIONS_H
#define MC_ENGINE_ENGINE_OPTIONS_H

#include <mc/string.h>

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
#include <memory>

namespace mc::shm {
class shm_runtime;
} // namespace mc::shm
#endif

namespace mc::engine {

struct engine_options {
    mc::string endpoint_name;
#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
    std::shared_ptr<mc::shm::shm_runtime> shm_runtime_override;
#endif
};

} // namespace mc::engine

#endif // MC_ENGINE_ENGINE_OPTIONS_H
