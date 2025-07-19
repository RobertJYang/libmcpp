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

#ifndef TASK_INTERFACE_H
#define TASK_INTERFACE_H

#include "gen/task_interface.h"
#include <mc/core/timer.h>

namespace test {

struct my_task_interface : public task_interface {
    my_task_interface();
    ~my_task_interface() override;

    void       start() override;
    void       stop() override;
    void       pause() override;
    void       resume() override;
    uint32_t   get_progress() override;
    task_state get_state() override;

    void start_timer();
    void stop_timer();
    void set_state(task_state state);
    void create_timer();

    mc::milliseconds m_timeout{1000};
    mc::core::timer* m_timer{nullptr};
};

} // namespace test

MC_REFLECTABLE(test::my_task_interface)

#endif // TASK_INTERFACE_H