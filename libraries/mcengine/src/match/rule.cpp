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

#include <mc/engine/match/path_pattern.h>
#include <mc/engine/match/rule.h>

#include "message_type_name.h"

namespace mc::engine::match {

namespace {

bool field_matches(const mc::string& expected, const mc::string& actual) noexcept
{
    return expected.empty() || expected == actual;
}

bool path_field_matches(const mc::string& rule_path, const mc::string& msg_path) noexcept
{
    if (rule_path.empty()) {
        return true;
    }
    return path_pattern(rule_path).matches(msg_path);
}

} // namespace

bool matches(const match_rule& rule, const message_header& header) noexcept
{
    if (!rule.type.empty() && rule.type != message_type_to_name(header.type)) {
        return false;
    }
    return field_matches(rule.sender, header.sender)
           && field_matches(rule.destination, header.destination)
           && path_field_matches(rule.path, header.path)
           && field_matches(rule.interface_name, header.interface_name)
           && field_matches(rule.member_name, header.member_name);
}

} // namespace mc::engine::match
