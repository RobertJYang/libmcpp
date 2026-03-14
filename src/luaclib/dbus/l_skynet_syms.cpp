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

#define LUA_LIB

#include "l_skynet_syms.h"
#include <chrono>
#include <dlfcn.h>

using namespace std::chrono;

namespace mc::dbus::lua {
SkynetSyms::SkynetSyms()
    : skynet_send(nullptr), skynet_send_with_priority(nullptr), skynet_error(nullptr), skynet_current_handle(nullptr),
      skynet_context_message_dispatch(nullptr), skynet_globalmq_push(nullptr), m_sm(nullptr)
{
    void* handle              = dlopen(nullptr, RTLD_NOW);
    skynet_send               = (t_skynet_send)dlsym(handle, "skynet_send");
    skynet_send_with_priority = (t_skynet_send_with_priority)dlsym(handle, "skynet_send_with_priority");
    skynet_error              = (t_skynet_error)dlsym(handle, "skynet_error");
    skynet_current_handle     = (t_skynet_current_handle)dlsym(handle, "skynet_current_handle");
    skynet_context_message_dispatch =
        (t_skynet_context_message_dispatch)dlsym(handle, "skynet_context_message_dispatch");
    t_skynet_monitor_new skynet_monitor_new = (t_skynet_monitor_new)dlsym(handle, "skynet_monitor_new");
    skynet_globalmq_push                    = (t_skynet_globalmq_push)dlsym(handle, "skynet_globalmq_push");
    dlclose(handle);
    if (skynet_monitor_new) {
        m_sm = skynet_monitor_new();
    }
}
SkynetSyms& SkynetSyms::get_instance()
{
    static SkynetSyms instance;
    return instance;
}

void SkynetSyms::run_skynet_context_message_dispatch(uint32_t step_interval, int32_t weight) const
{
    message_queue* q     = nullptr;
    auto           start = high_resolution_clock::now();
    while ((q = skynet_context_message_dispatch(m_sm, q, weight)) != nullptr) {
        auto end      = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        if (duration.count() > step_interval) {
            skynet_globalmq_push(q);
            break;
        }
    }
}

} // namespace mc::dbus::lua