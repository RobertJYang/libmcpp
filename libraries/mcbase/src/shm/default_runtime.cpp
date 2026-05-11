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
 * @file default_runtime.cpp
 * @brief mc::shm::default_runtime 单例实现，复用 mc::singleton<T>
 */

#include <mc/shm/default_runtime.h>

#include <cstdlib>
#include <cstring>

#include <mc/singleton.h>
#include <mc/string.h>

namespace mc::shm {

namespace {

constexpr std::size_t k_default_region_bytes = 64ULL * 1024ULL * 1024ULL;

struct default_runtime_tag {};
using runtime_singleton = mc::singleton<shm_runtime, default_runtime_tag>;

runtime_options _options_from_env()
{
    runtime_options opts;

    if (const char* name = std::getenv("MC_SHM_DEFAULT_REGION"); name != nullptr && *name != '\0') {
        opts.region_name = mc::string(name);
    } else {
        opts.region_name = mc::string("mc.default");
    }

    if (const char* size_env = std::getenv("MC_SHM_DEFAULT_SIZE"); size_env != nullptr && *size_env != '\0') {
        char*                    end  = nullptr;
        const unsigned long long size = std::strtoull(size_env, &end, 10);
        if (end != size_env && size > 0U) {
            opts.region_size = static_cast<std::size_t>(size);
        }
    } else {
        opts.region_size = k_default_region_bytes;
    }

    if (const char* mgr = std::getenv("MC_SHM_DEFAULT_MANAGER"); mgr != nullptr) {
        opts.manager_process = (std::strcmp(mgr, "1") == 0);
    }

    return opts;
}

} // namespace

bool init_default_runtime(const runtime_options& options)
{
    bool created_now = false;
    runtime_singleton::instance_with_creator([&]() {
        created_now = true;
        return new shm_runtime(options);
    });
    return created_now;
}

shm_runtime& default_runtime()
{
    return runtime_singleton::instance_with_creator([]() {
        return new shm_runtime(_options_from_env());
    });
}

bool default_runtime_initialized() noexcept
{
    return runtime_singleton::created();
}

void shutdown_default_runtime() noexcept
{
    runtime_singleton::reset_for_test();
}

} // namespace mc::shm
