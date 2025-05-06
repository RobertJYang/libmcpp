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
#include <boost/asio/steady_timer.hpp>
#include <mc/engine/object.h>
#include <mc/engine/service.h>
#include <mc/time.h>

namespace test {
class task_object;
using task_object_ptr = mc::im::ref_ptr<task_object>;

class task_object : public mc::engine::object<task_object> {
public:
    MC_OBJECT("/bmc/kepler/TaskService/Tasks/${Id}", (task_interface))

    static task_object_ptr create_task(mc::engine::service* service, mc::milliseconds timeout);

    task_interface m_task;

private:
    using timer_ptr = std::unique_ptr<boost::asio::steady_timer>;
    mc::engine::service* m_service;

    static uint32_t m_next_task_id;
};

} // namespace test

MC_REFLECT(test::task_object, ((m_task, "Task")))

#endif // TASK_OBJECT_H