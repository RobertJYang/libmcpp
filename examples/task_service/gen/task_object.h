/*
 * Copyright (c) 2023, OpenUBMC Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 */

#ifndef TASK_OBJECT_H
#define TASK_OBJECT_H

#include "task_interface.h"
#include <mc/engine/object.h>

namespace gen {
class task_object : public mc::engine::object<task_object> {
public:
    MC_OBJECT("/bmc/kepler/TaskService/Tasks/${Id}", (task_interface))

    task_interface m_task;
};

} // namespace gen

MC_REFLECT(test::task_object,
           ((m_task, "Task"))((start, "Start"))((stop, "Stop"))((pause, "Pause"))(
               (resume, "Resume"))((get_progress, "GetProgress"))((get_state, "GetState")))

#endif // TASK_OBJECT_H