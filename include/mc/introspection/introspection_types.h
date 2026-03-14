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
#ifndef MC_INTROSPECTION_TYPES_H
#define MC_INTROSPECTION_TYPES_H

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct argument_info {
    std::string                name;
    std::string                type;
    std::string                direction;
    std::optional<std::string> struct_type;
};

struct method_info {
    std::vector<argument_info> args;
};

struct property_info {
    std::string                                  type;
    std::string                                  access;
    std::unordered_map<std::string, std::string> options;

    bool is_volatile() const
    {
        auto it = options.find("volatile");
        return it != options.end() && (it->second == "true" || it->second == "1");
    }
};

struct interface_info {
    std::unordered_map<std::string, method_info>   methods;
    std::unordered_map<std::string, property_info> properties;
};

struct node_info {
    std::unordered_map<std::string, interface_info> ifaces;
};

#endif // MC_INTROSPECTION_TYPES_H
