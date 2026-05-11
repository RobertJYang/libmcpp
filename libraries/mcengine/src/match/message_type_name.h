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

#ifndef MC_ENGINE_MATCH_INTERNAL_MESSAGE_TYPE_NAME_H
#define MC_ENGINE_MATCH_INTERNAL_MESSAGE_TYPE_NAME_H

#include <mc/engine/payload.h>
#include <mc/string_view.h>

namespace mc::engine::match {

inline mc::string_view message_type_to_name(message_type type) noexcept
{
    switch (type) {
        case message_type::method_call:
            return "method_call";
        case message_type::method_return:
            return "method_return";
        case message_type::error:
            return "error";
        case message_type::signal:
            return "signal";
        case message_type::invalid:
        default:
            return "invalid";
    }
}

} // namespace mc::engine::match

#endif // MC_ENGINE_MATCH_INTERNAL_MESSAGE_TYPE_NAME_H
