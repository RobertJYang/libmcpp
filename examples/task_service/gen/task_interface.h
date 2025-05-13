/**
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef GEN_TASK_INTERFACE_H
#define GEN_TASK_INTERFACE_H

#include <mc/engine/interface.h>

namespace test {

enum class task_state {
    PENDING,
    RUNNING,
    PAUSED,
    COMPLETED,
    FAILED,
};

struct task_interface : public mc::engine::interface<task_interface> {
    MC_INTERFACE("bmc.kepler.TaskService.Task")

    virtual ~task_interface()         = default;
    virtual void       start()        = 0;
    virtual void       stop()         = 0;
    virtual void       pause()        = 0;
    virtual void       resume()       = 0;
    virtual uint32_t   get_progress() = 0;
    virtual task_state get_state()    = 0;

    mc::engine::property<uint32_t>    m_id;
    mc::engine::property<std::string> m_name;
    mc::engine::property<std::string> m_startTime;
    mc::engine::property<std::string> m_endTime;
    mc::engine::property<uint32_t>    m_progress;
    mc::engine::property<task_state>  m_state;
    mc::engine::property<mc::variant> m_result;
};

} // namespace test

MC_REFLECT_ENUM(test::task_state, (PENDING)(RUNNING)(PAUSED)(COMPLETED)(FAILED))
MC_REFLECT(test::task_interface,
           ((m_id, "Id"))((m_name, "Name"))((m_startTime, "StartTime"))((m_endTime, "EndTime"))(
               (m_progress, "Progress"))((m_state, "State"))((m_result, "Result"))(
               (start, "Start"))((stop, "Stop"))((pause, "Pause"))((resume, "Resume"))(
               (get_progress, "GetProgress"))((get_state, "GetState")))

#endif // GEN_TASK_INTERFACE_H
