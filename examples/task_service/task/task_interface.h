/*
 * Copyright (c) 2023, OpenUBMC Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 */

#ifndef TASK_INTERFACE_H
#define TASK_INTERFACE_H

#include <mc/core/timer.h>
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

    task_interface();
    ~task_interface();

    void       start();
    void       stop();
    void       pause();
    void       resume();
    uint32_t   get_progress();
    task_state get_state();

    void start_timer();
    void stop_timer();
    void set_state(task_state state);
    void create_timer();

    uint32_t    m_id;
    std::string m_name;
    std::string m_startTime;
    std::string m_endTime;
    uint32_t    m_progress{0};
    task_state  m_state{task_state::PENDING};
    mc::variant m_result;

    mc::milliseconds m_timeout{1000};
    mc::core::timer* m_timer{nullptr};
};

} // namespace test

MC_REFLECT_ENUM(test::task_state, (PENDING)(RUNNING)(PAUSED)(COMPLETED)(FAILED))
MC_REFLECT(test::task_interface,
           ((m_id, "Id"))((m_name, "Name"))((m_startTime, "StartTime"))((m_endTime, "EndTime"))(
               (m_progress, "Progress"))((m_state, "State"))((m_result, "Result"))(
               (start, "Start"))((stop, "Stop"))((pause, "Pause"))((resume, "Resume"))(
               (get_progress, "GetProgress"))((get_state, "GetState")))

#endif // TASK_INTERFACE_H