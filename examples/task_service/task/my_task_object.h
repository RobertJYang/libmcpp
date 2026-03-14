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

// 调用函数
// 绑定对象
//
class my_task_object : public mc::engine::object<my_task_object> {
public:
    MC_OBJECT(my_task_object, "TaskObject", "/bmc/kepler/TaskService/Tasks/${Id}", (my_task_interface))

    my_task_object(mc::core::object* parent = nullptr);

    static mc::shared_ptr<my_task_object> create_task(mc::core::object* parent, mc::milliseconds timeout);

    my_task_interface m_task;
    static uint32_t   m_next_task_id;
};
using task_object_ptr = mc::shared_ptr<my_task_object>;

class my_tasks_object : public mc::engine::object<my_tasks_object> {
    using tasks_type = std::vector<mc::shared_ptr<my_task_object>>;

public:
    MC_OBJECT(my_tasks_object, "TasksObject", "/bmc/kepler/TaskService/Tasks", (tasks_interface))

    my_tasks_object(mc::core::object* parent = nullptr);

    // 测试在对象中实现接口的方法
    std::string_view              create_task(const std::string& name);
    std::vector<std::string_view> get_tasks();

private:
    tasks_interface m_tasks_intf;
    tasks_type      m_tasks;
};
using tasks_object_ptr = mc::shared_ptr<my_tasks_object>;

} // namespace test

#endif // TASK_OBJECT_H