/*
 * Copyright (c) 2023, OpenUBMC Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TEST_SERVICE_H
#define TEST_SERVICE_H

#include "task/task_object.h"
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

    std::vector<task_object_ptr> m_tasks;
};

} // namespace test

#endif // TEST_SERVICE_H
