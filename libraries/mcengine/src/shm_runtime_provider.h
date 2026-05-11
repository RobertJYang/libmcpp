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

#ifndef MC_ENGINE_SHM_RUNTIME_PROVIDER_H
#define MC_ENGINE_SHM_RUNTIME_PROVIDER_H

#include <cstddef>
#include <functional>
#include <memory>

#include <mc/common.h>
#include <mc/shm/shm_runtime.h>
#include <mc/string_view.h>

// 进程内共享的 shm_runtime 单例入口。lazy 创建失败抛 mc::invalid_arg_exception。

namespace mc::engine {

class MC_API shm_runtime_provider {
public:
    static mc::shm::shm_runtime& instance();
    static bool                  has_instance() noexcept;
    static void install_runtime(std::shared_ptr<mc::shm::shm_runtime> runtime) noexcept;
    static void reset_for_test() noexcept;
    static mc::string_view default_region_name() noexcept;

    using arena_threshold_callback =
        std::function<void(std::size_t used_bytes, std::size_t threshold_bytes)>;
    static void set_arena_threshold(std::size_t              threshold_bytes,
                                    arena_threshold_callback callback) noexcept;
    static void check_arena_usage() noexcept;

private:
    shm_runtime_provider() = delete;
};

}  // namespace mc::engine

#endif  // MC_ENGINE_SHM_RUNTIME_PROVIDER_H
