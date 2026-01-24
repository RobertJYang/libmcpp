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

#ifndef MC_LDBUS_SKYNET_SYMS_H
#define MC_LDBUS_SKYNET_SYMS_H

#include <cstdint>
#include<stddef.h>
#include<mc/common.h>
#include<utility>

struct skynet_monitor;
struct message_queue;
struct skynet_context;

namespace mc::dbus::lua {

constexpr int PTYPE_TAG_DONTCOPY = 0x10000;
constexpr int PTYPE_SIGNAL_PROCESS = 223;
using t_skynet_current_handle = uint32_t (*)(void);
using t_skynet_send = int(*)(skynet_context *context, uint32_t source, uint32_t destination, int type, int session, void *msg, size_t sz);
using t_skynet_send_with_priority = int(*)(skynet_context *context, uint32_t source, uint32_t destination, int type, int session, void *msg, size_t sz, uint8_t priority);
using t_skynet_error = void(*)(skynet_context *context, const char *msg, ...);
using t_skynet_context_message_dispatch = message_queue*(*)(skynet_monitor*,message_queue*,int);
using t_skynet_monitor_new = skynet_monitor*(*)(void);
using t_skynet_globalmq_push = void(*)(message_queue*);

class MC_API SkynetSyms{
public:
    t_skynet_send                       skynet_send;
    t_skynet_send_with_priority         skynet_send_with_priority;
    t_skynet_error                      skynet_error;
    t_skynet_current_handle             skynet_current_handle;
    t_skynet_context_message_dispatch   skynet_context_message_dispatch;
    t_skynet_globalmq_push              skynet_globalmq_push;

    void run_skynet_context_message_dispatch(uint32_t step_interval, int32_t weight)const;
    template<typename... Args>
    void error(const char *msg,Args &&...args)const{
        if(skynet_error){
            skynet_error(nullptr,msg,std::forward<Args>(args)...);
        }
    }
    static SkynetSyms& get_instance();
private:
    SkynetSyms();
    skynet_monitor* m_sm;
};

}// namespace mc::dbus

#endif