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

    m_tasks = mc::make_shared<my_tasks_object>(this);
    register_object(m_tasks);
    return true;
}

bool test_service::stop() {
    if (!mc::engine::service::stop()) {
        return false;
    }

    // for (auto& task : m_tasks) {
    //     // task->stop();
    // }
    return true;
}

} // namespace test