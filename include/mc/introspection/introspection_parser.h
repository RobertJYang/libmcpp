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
#ifndef MC_INTROSPECTION_PARSER_H
#define MC_INTROSPECTION_PARSER_H

#include "introspection_types.h"
#include <string>

#include <boost/property_tree/ptree_fwd.hpp>

class introspection_parser {
public:
    // 解析 DBus introspection XML
    // 输入完整XML字符串
    // 输出：node_info (node / interface / method / property)
    static node_info parse(const std::string& xml);

private:
    static void parse_interface(const boost::property_tree::ptree& pt, interface_info& iface);

    static void parse_method(const boost::property_tree::ptree& pt, method_info& method);

    static void parse_property(const boost::property_tree::ptree& pt, property_info& prop);

    static void parse_arg(const boost::property_tree::ptree& pt, argument_info& arg);
};

#endif // MC_INTROSPECTION_PARSER_H
