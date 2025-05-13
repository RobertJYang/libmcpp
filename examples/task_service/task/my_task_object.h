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

#ifndef TASK_OBJECT_H
#define TASK_OBJECT_H

#include "my_task_interface.h"
#include <mc/engine/object.h>

namespace test {

class my_task_object : public mc::engine::object<my_task_object> {
public:
    MC_OBJECT(my_task_object, "/bmc/kepler/TaskService/Tasks/${Id}", (my_task_interface))

    my_task_object(mc::core::object* parent = nullptr);

    static my_task_object* create_task(mc::engine::service* service, mc::milliseconds timeout);

private:
    my_task_interface m_task;
    static uint32_t   m_next_task_id;
};
} // namespace test

MC_REFLECT(test::my_task_object, ((m_task, "task")))

#endif // TASK_OBJECT_H