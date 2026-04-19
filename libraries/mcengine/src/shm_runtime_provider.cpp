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

#include <mc/engine/shm_runtime_provider.h>

#include <cstdlib>
#include <mutex>

#include <mc/exception.h>
#include <mc/shm/detail/shared_memory_backend.h>

namespace mc::engine {

namespace {

struct provider_state {
    std::mutex                            mutex;
    std::shared_ptr<mc::shm::shm_runtime> runtime;
    mc::string                            last_region_name;
};

provider_state& _state() noexcept
{
    static provider_state* s = new provider_state();
    return *s;
}

constexpr const char* k_default_region_fallback = "mcengine.default";
constexpr std::size_t k_default_region_size     = 4U * 1024U * 1024U;
constexpr std::size_t k_default_root_capacity   = 256U;

std::shared_ptr<mc::shm::shm_runtime> _create_default_runtime(mc::string& region_name_out)
{
    mc::shm::runtime_options opts;
    opts.region_name     = mc::string(shm_runtime_provider::default_region_name());
    opts.region_size     = k_default_region_size;
    opts.root_capacity   = k_default_root_capacity;
    opts.manager_process = false;

    region_name_out = opts.region_name;

    auto rt = std::make_shared<mc::shm::shm_runtime>(opts);
    if (!rt->is_valid()) {
        MC_THROW(mc::invalid_arg_exception,
                 "shm_runtime_provider: 无法 lazy 创建 shm_runtime（region=${region}）",
                 ("region", opts.region_name));
    }
    return rt;
}

}  // namespace

mc::shm::shm_runtime& shm_runtime_provider::instance()
{
    auto&                       st = _state();
    std::lock_guard<std::mutex> lock(st.mutex);
    if (!st.runtime) {
        st.runtime = _create_default_runtime(st.last_region_name);
    }
    return *st.runtime;
}

bool shm_runtime_provider::has_instance() noexcept
{
    auto&                       st = _state();
    std::lock_guard<std::mutex> lock(st.mutex);
    return static_cast<bool>(st.runtime);
}

void shm_runtime_provider::install_runtime(std::shared_ptr<mc::shm::shm_runtime> runtime) noexcept
{
    if (!runtime) {
        return;
    }
    auto&                       st = _state();
    std::lock_guard<std::mutex> lock(st.mutex);
    st.runtime          = std::move(runtime);
    st.last_region_name = mc::string();  // 外部 install：region_name 由调用方掌控
}

void shm_runtime_provider::reset_for_test() noexcept
{
    mc::string region_to_unlink;
    {
        auto&                       st = _state();
        std::lock_guard<std::mutex> lock(st.mutex);
        st.runtime.reset();
        region_to_unlink = std::move(st.last_region_name);
        st.last_region_name.clear();
    }

    // 同进程下次 lazy 创建会从干净 region 开始，避免上一个 case 的残留状态。
    // 无论本进程是否已创建过 runtime，都尝试 unlink 默认 region：
    // 防止上一进程崩溃残留的 backing file 污染当前进程的 lazy 创建。
    if (!region_to_unlink.empty()) {
        mc::shm::detail::shared_memory_backend::remove(mc::string_view(region_to_unlink));
    }
    auto default_name = default_region_name();
    if (!default_name.empty() && mc::string_view(region_to_unlink) != default_name) {
        mc::shm::detail::shared_memory_backend::remove(default_name);
    }
}

mc::string_view shm_runtime_provider::default_region_name() noexcept
{
    if (const char* env = std::getenv("MC_SHM_REGION"); env != nullptr && *env != '\0') {
        return mc::string_view(env);
    }
    return mc::string_view(k_default_region_fallback);
}

}  // namespace mc::engine
