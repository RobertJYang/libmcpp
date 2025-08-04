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

#ifndef TEST_SERVICE_H
#define TEST_SERVICE_H

#include "task/my_task_object.h"
#include <mc/engine.h>

namespace test {

struct test_service : public mc::engine::service {
public:
    test_service(std::string_view name);
    ~test_service();

    bool init(mc::dict args) override;
    bool start() override;
    bool stop() override;

private:
    void create_task();

    tasks_object_ptr m_tasks;
};

} // namespace test

#endif // TEST_SERVICE_H
