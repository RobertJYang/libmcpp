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

#include "my_task_interface.h"
#include <mc/engine.h>
#include <mc/log.h>

namespace test {
namespace errors {
REGISTER_CONST_ERROR(task_state_error, "bmc.kepler.error.TaskStateError",
                     "expect task state is {}, actual state is {}");
} // namespace errors

my_task_interface::my_task_interface()
{}

my_task_interface::~my_task_interface()
{}

void my_task_interface::start()
{
    if (*m_state != task_state::PENDING) {
        elog("task {} is not pending", *m_name);
        MC_REPLY_ERROR(test::errors::task_state_error, task_state::PENDING, *m_state);
        return;
    }

    set_state(task_state::RUNNING);
    start_timer();
}

void my_task_interface::stop()
{
    if (*m_state == task_state::COMPLETED || *m_state == task_state::FAILED) {
        elog("task {} is already completed or failed", *m_name);
        return;
    }

    set_state(task_state::FAILED);
    stop_timer();
}

void my_task_interface::pause()
{
    if (*m_state != task_state::RUNNING) {
        elog("task {} is not running", *m_name);
        MC_REPLY_ERROR(test::errors::task_state_error, task_state::RUNNING, *m_state);
        return;
    }

    set_state(task_state::PAUSED);
    stop_timer();
}

void my_task_interface::resume()
{
    if (*m_state != task_state::PAUSED) {
        elog("task {} is not paused", *m_name);
        MC_REPLY_ERROR(test::errors::task_state_error, ("expect", task_state::PAUSED)("current", *m_state));
        return;
    }

    set_state(task_state::RUNNING);
    start_timer();
}

void my_task_interface::start_timer()
{
    if (!m_timer) {
        create_timer();
    } else {
        m_timer->stop();
    }

    m_timer->start(mc::milliseconds(1000));
    ilog("task {} start, current time: {}, progress: {}", *m_name, mc::time_point::now(), *m_progress);
}

void my_task_interface::create_timer()
{
    m_timer = new mc::timer(this);
    this->connect(m_timer->timeout, [this]() {
        if (*m_state != task_state::RUNNING) {
            return;
        }

        ilog("task {} progress: {}", *m_name, *m_progress);

        m_progress.set_value(*m_progress + 1);
        if (*m_progress >= 100) {
            set_state(task_state::COMPLETED);
            m_timer->stop();
        }
    });
    m_startTime.set_value(std::string(mc::time_point::now()));
    m_endTime.set_value(std::string(mc::time_point::now() + m_timeout));
}

void my_task_interface::stop_timer()
{
    if (m_timer) {
        m_timer->stop();
        ilog("task {} stop, current time: {}, progress: {}", *m_name, mc::time_point::now(), *m_progress);
    }
}

uint32_t my_task_interface::get_progress()
{
    return *m_progress;
}

task_state my_task_interface::get_state()
{
    return *m_state;
}

void my_task_interface::set_state(task_state state)
{
    ilog("task {} state from {} to {}", *m_name, *m_state, state);
    m_state.set_value(state);
}

} // namespace test

// 自动生成代码的反射单独放到一个模块的命名空间中
MC_MODULE_REFLECT_ENUM(mc_task_service_gen, test::task_state, (PENDING)(RUNNING)(PAUSED)(COMPLETED)(FAILED))
MC_MODULE_REFLECT(mc_task_service_gen, test::task_interface, (m_id, "Id")(m_name, "Name")(m_startTime, "StartTime"),
                  (m_endTime, "EndTime")(m_progress, "Progress"),
                  (m_state, "State")(m_result, "Result")(start, "Start"),
                  (stop, "Stop")(pause, "Pause")(resume, "Resume"),
                  (get_progress, "GetProgress")(get_state, "GetState"))
MC_MODULE_REFLECT(mc_task_service_gen, test::tasks_interface, (create_task, "CreateTask")(get_tasks, "GetTasks"))

// 继承的接口的放在全局命名空间（这里是测试，实际使用时应该放在对应模块中，尽量不放到全局命名空间）
MC_REFLECT(test::my_task_interface, MC_BASE_CLASS(test::task_interface))
