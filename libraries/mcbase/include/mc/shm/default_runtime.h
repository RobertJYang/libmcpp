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

/**
 * @file default_runtime.h
 * @brief 进程内全局共享的 shm_runtime facade
 */
#ifndef MC_SHM_DEFAULT_RUNTIME_H
#define MC_SHM_DEFAULT_RUNTIME_H

#include <mc/common.h>
#include <mc/shm/shm_runtime.h>

namespace mc::shm {

/** @brief 显式初始化默认 runtime */
MC_API bool init_default_runtime(const runtime_options& options);

/**
 * @brief 取默认 runtime 引用
 *
 * 环境变量：
 *   MC_SHM_DEFAULT_REGION  - region 名，缺省 "mc.default"
 *   MC_SHM_DEFAULT_SIZE    - region 字节数，缺省 64 MiB
 *   MC_SHM_DEFAULT_MANAGER - "1" 表示本进程为 manager
 */
MC_API shm_runtime& default_runtime();

/** @brief 默认 runtime 是否已初始化 */
MC_API bool default_runtime_initialized() noexcept;

/** @brief 销毁默认 runtime */
MC_API void shutdown_default_runtime() noexcept;

} // namespace mc::shm

#endif // MC_SHM_DEFAULT_RUNTIME_H
