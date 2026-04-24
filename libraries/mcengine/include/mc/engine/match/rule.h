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

#ifndef MC_ENGINE_MATCH_RULE_H
#define MC_ENGINE_MATCH_RULE_H

#include <mc/common.h>
#include <mc/engine/message.h>
#include <mc/string.h>

namespace mc::engine::match {

// 6 字段匹配规则。空字符串视为通配。
// path 字段支持 path pattern 语法。
struct match_rule {
    mc::string type;
    mc::string sender;
    mc::string destination;
    mc::string path;
    mc::string interface_name;
    mc::string member_name;
};

MC_API bool matches(const match_rule& rule, const message_header& header) noexcept;

} // namespace mc::engine::match

#endif // MC_ENGINE_MATCH_RULE_H
