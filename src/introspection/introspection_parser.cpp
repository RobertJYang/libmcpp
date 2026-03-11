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
#include "mc/introspection/introspection_parser.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <sstream>
#include <unordered_map>

namespace bp = boost::property_tree;

void introspection_parser::parse_arg(const bp::ptree& pt, argument_info& arg)
{
    arg.type      = pt.get<std::string>("<xmlattr>.type");
    arg.name      = pt.get<std::string>("<xmlattr>.name", "");
    arg.direction = pt.get<std::string>("<xmlattr>.direction", "");

    auto struct_type = pt.get_optional<std::string>("<xmlattr>.struct-type");
    if (struct_type) {
        arg.struct_type = *struct_type;
    }
}

void introspection_parser::parse_method(const bp::ptree& pt, method_info& method)
{
    for (const auto& child : pt) {
        if (child.first == "arg") {
            argument_info arg;
            parse_arg(child.second, arg);
            method.args.emplace_back(std::move(arg));
        }
    }
}

void introspection_parser::parse_property(const bp::ptree& pt, property_info& prop)
{
    prop.type   = pt.get<std::string>("<xmlattr>.type");
    prop.access = pt.get<std::string>("<xmlattr>.access", "");

    // 读取 <annotation>
    for (const auto& child : pt) {
        if (child.first == "annotation") {
            auto key   = child.second.get<std::string>("<xmlattr>.name");
            auto value = child.second.get<std::string>("<xmlattr>.value");
            prop.options.emplace(key, value);
        }
    }
}

void introspection_parser::parse_interface(const bp::ptree& pt, interface_info& iface)
{
    for (const auto& child : pt) {
        if (child.first == "method") {
            method_info method;
            parse_method(child.second, method);

            auto name = child.second.get<std::string>("<xmlattr>.name");
            iface.methods.emplace(name, std::move(method));
        } else if (child.first == "property") {
            property_info prop;
            parse_property(child.second, prop);

            auto name = child.second.get<std::string>("<xmlattr>.name");
            iface.properties.emplace(name, std::move(prop));
        }
    }
}

/* ================================ 对外接口 ================================*/
node_info introspection_parser::parse(const std::string& xml)
{
    std::stringstream ss(xml);
    bp::ptree pt;
    bp::read_xml(ss, pt);

    node_info node;

    for (const auto& child : pt.get_child("node")) {
        if (child.first == "interface") {
            interface_info iface;
            parse_interface(child.second, iface);

            auto name = child.second.get<std::string>("<xmlattr>.name");
            node.ifaces.emplace(name, std::move(iface));
        }
    }
    return node;
}