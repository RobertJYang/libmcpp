/*
 * Copyright (c) 2023, OpenUBMC Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 */

#ifndef TASK_INTERFACE_H
#define TASK_INTERFACE_H

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

    uint32_t    m_id;
    std::string m_name;
    std::string m_startTime;
    std::string m_endTime;
    uint32_t    m_progress{0};
    task_state  m_state{task_state::PENDING};
    mc::variant m_result;
};

} // namespace test

MC_REFLECT_ENUM(test::task_state, (PENDING)(RUNNING)(COMPLETED)(FAILED))
MC_REFLECT(test::task_interface,
           ((m_id, "Id"))((m_name, "Name"))((m_startTime, "StartTime"))((m_endTime, "EndTime"))(
               (m_progress, "Progress"))((m_state, "State"))((m_result, "Result")))

#endif // TASK_INTERFACE_H