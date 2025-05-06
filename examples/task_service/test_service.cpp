/*
 * Copyright (c) 2023, OpenUBMC Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "test_service.h"

namespace test {

test_service::test_service(std::string_view name) : mc::engine::service(name) {
}

test_service::~test_service() {
}

bool test_service::init(mc::dict args) {
    return mc::engine::service::init(args);
}

bool test_service::start() {
    if (!mc::engine::service::start()) {
        return false;
    }

    create_task();
    return true;
}

bool test_service::stop() {
    if (!mc::engine::service::stop()) {
        return false;
    }

    for (auto& task : m_tasks) {
        // task->stop();
    }
    return true;
}

void test_service::create_task() {
    auto task = task_object::create_task(this, mc::milliseconds(1000));
    m_tasks.push_back(task);
}

} // namespace test