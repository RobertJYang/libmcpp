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

MC_MODULE(mc_task_service_gen)

namespace test {

enum class task_state {
    PENDING,
    RUNNING,
    PAUSED,
    COMPLETED,
    FAILED,
};

struct tasks_interface : public mc::engine::interface<tasks_interface> {
    MC_INTERFACE("bmc.kepler.TaskService.Tasks")

    virtual ~tasks_interface() = default;

    virtual std::string_view create_task(const std::string& name) {
        return {}; // 空函数，在对象中重载
    }
    virtual std::vector<std::string_view> get_tasks() {
        return {}; // 空函数，在对象中重载
    }
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

MC_REFLECTABLE("test.task_state", test::task_state)

#endif // GEN_TASK_INTERFACE_H
