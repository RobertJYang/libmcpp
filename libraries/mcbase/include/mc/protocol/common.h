/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_PROTOCOL_COMMON_H
#define MC_PROTOCOL_COMMON_H

#include <mc/string.h>
#include <mc/string_view.h>

#include <cstddef>
#include <cstdint>

namespace mc::protocol {

enum class flow_direction : uint8_t {
    push = 0,
    pop  = 1,
};

enum class execution_state : uint8_t {
    idle = 0,
    running,
    suspended,
    completed,
    failed,
};

enum class step_kind : uint8_t {
    push_next = 0,
    pop_next,
    jump,
    suspend,
    complete,
    fail,
};

struct protocol_error {
    mc::string name;
    mc::string message;

    void clear()
    {
        name.clear();
        message.clear();
    }
};

struct command {
    step_kind      kind{step_kind::push_next};
    std::size_t    target_index{0};
    flow_direction target_direction{flow_direction::push};
};

} // namespace mc::protocol

#endif // MC_PROTOCOL_COMMON_H
