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

#include "test_service.h"

namespace test {

test_service::test_service(mc::string name) : mc::app::service(std::move(name))
{}

test_service::~test_service()
{}

bool test_service::on_start()
{
    setup_tasks_root();
    return true;
}

bool test_service::on_stop()
{
    m_tasks.reset();
    return true;
}

void test_service::setup_tasks_root()
{
    if (m_tasks) {
        return;
    }

    m_tasks = my_tasks_object::create();
    m_tasks->set_object_name("TasksObject");
    m_tasks->set_object_path("/bmc/kepler/TaskService/Tasks");
    register_object(m_tasks);
}

} // namespace test