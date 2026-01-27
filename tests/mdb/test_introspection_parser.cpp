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

#include <gtest/gtest.h>
#include <test_utilities/test_base.h>

#include "../../../src/introspection/introspection_parser.h"
#include "../../../src/introspection/introspection_types.h"

using namespace mc;

class introspection_parser_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        mc::test::TestBase::SetUp();
    }
};

// 测试解析简单的接口，包含方法和属性
TEST_F(introspection_parser_test, parse_simple_interface) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <method name="TestMethod">
      <arg type="s" name="input" direction="in"/>
      <arg type="s" name="output" direction="out"/>
    </method>
    <property name="TestProperty" type="i" access="readwrite"/>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    // 验证接口存在
    ASSERT_EQ(node.ifaces.size(), 1);
    ASSERT_NE(node.ifaces.find("org.example.Test"), node.ifaces.end());

    const interface_info& iface = node.ifaces.at("org.example.Test");

    // 验证方法
    ASSERT_EQ(iface.methods.size(), 1);
    ASSERT_NE(iface.methods.find("TestMethod"), iface.methods.end());

    const method_info& method = iface.methods.at("TestMethod");
    ASSERT_EQ(method.args.size(), 2);
    EXPECT_EQ(method.args[0].name, "input");
    EXPECT_EQ(method.args[0].type, "s");
    EXPECT_EQ(method.args[0].direction, "in");
    EXPECT_EQ(method.args[1].name, "output");
    EXPECT_EQ(method.args[1].type, "s");
    EXPECT_EQ(method.args[1].direction, "out");

    // 验证属性
    ASSERT_EQ(iface.properties.size(), 1);
    ASSERT_NE(iface.properties.find("TestProperty"), iface.properties.end());

    const property_info& prop = iface.properties.at("TestProperty");
    EXPECT_EQ(prop.type, "i");
    EXPECT_EQ(prop.access, "readwrite");
}

// 测试解析多个接口
TEST_F(introspection_parser_test, parse_multiple_interfaces) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Interface1">
    <method name="Method1">
      <arg type="i" name="arg1" direction="in"/>
    </method>
  </interface>
  <interface name="org.example.Interface2">
    <property name="Property1" type="s" access="read"/>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    ASSERT_EQ(node.ifaces.size(), 2);
    ASSERT_NE(node.ifaces.find("org.example.Interface1"), node.ifaces.end());
    ASSERT_NE(node.ifaces.find("org.example.Interface2"), node.ifaces.end());

    const interface_info& iface1 = node.ifaces.at("org.example.Interface1");
    ASSERT_EQ(iface1.methods.size(), 1);
    ASSERT_EQ(iface1.properties.size(), 0);

    const interface_info& iface2 = node.ifaces.at("org.example.Interface2");
    ASSERT_EQ(iface2.methods.size(), 0);
    ASSERT_EQ(iface2.properties.size(), 1);
}

// 测试解析方法参数，包含 struct-type
TEST_F(introspection_parser_test, parse_method_with_struct_type) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <method name="ComplexMethod">
      <arg type="a{sv}" name="input" direction="in" struct-type="VariantMap"/>
      <arg type="i" name="result" direction="out"/>
    </method>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const method_info&    method = iface.methods.at("ComplexMethod");

    ASSERT_EQ(method.args.size(), 2);
    EXPECT_EQ(method.args[0].name, "input");
    EXPECT_EQ(method.args[0].type, "a{sv}");
    EXPECT_EQ(method.args[0].direction, "in");
    ASSERT_TRUE(method.args[0].struct_type.has_value());
    EXPECT_EQ(method.args[0].struct_type.value(), "VariantMap");

    EXPECT_EQ(method.args[1].name, "result");
    EXPECT_EQ(method.args[1].type, "i");
    EXPECT_EQ(method.args[1].direction, "out");
    EXPECT_FALSE(method.args[1].struct_type.has_value());
}

// 测试解析属性，包含 annotation
TEST_F(introspection_parser_test, parse_property_with_annotation) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <property name="VolatileProperty" type="s" access="read">
      <annotation name="volatile" value="true"/>
      <annotation name="custom" value="test_value"/>
    </property>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const property_info&  prop   = iface.properties.at("VolatileProperty");

    EXPECT_EQ(prop.type, "s");
    EXPECT_EQ(prop.access, "read");
    ASSERT_EQ(prop.options.size(), 2);
    EXPECT_EQ(prop.options.at("volatile"), "true");
    EXPECT_EQ(prop.options.at("custom"), "test_value");
    EXPECT_TRUE(prop.is_volatile());
}

// 测试解析属性，volatile 为 false
TEST_F(introspection_parser_test, parse_property_not_volatile) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <property name="NormalProperty" type="i" access="readwrite">
      <annotation name="volatile" value="false"/>
    </property>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const property_info&  prop   = iface.properties.at("NormalProperty");

    EXPECT_FALSE(prop.is_volatile());
}

// 测试解析属性，volatile 为 1
TEST_F(introspection_parser_test, parse_property_volatile_as_one) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <property name="VolatileProperty" type="s" access="read">
      <annotation name="volatile" value="1"/>
    </property>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const property_info&  prop   = iface.properties.at("VolatileProperty");

    EXPECT_TRUE(prop.is_volatile());
}

// 测试解析方法，无参数
TEST_F(introspection_parser_test, parse_method_no_args) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <method name="NoArgMethod"/>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const method_info&    method = iface.methods.at("NoArgMethod");

    EXPECT_EQ(method.args.size(), 0);
}

// 测试解析方法，参数无 name
TEST_F(introspection_parser_test, parse_method_arg_without_name) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <method name="MethodWithUnnamedArg">
      <arg type="i" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const method_info&    method = iface.methods.at("MethodWithUnnamedArg");

    ASSERT_EQ(method.args.size(), 2);
    EXPECT_EQ(method.args[0].name, "");
    EXPECT_EQ(method.args[0].type, "i");
    EXPECT_EQ(method.args[0].direction, "in");
    EXPECT_EQ(method.args[1].name, "");
    EXPECT_EQ(method.args[1].type, "s");
    EXPECT_EQ(method.args[1].direction, "out");
}

// 测试解析属性，无 access
TEST_F(introspection_parser_test, parse_property_without_access) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <property name="PropertyWithoutAccess" type="b"/>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const property_info&  prop   = iface.properties.at("PropertyWithoutAccess");

    EXPECT_EQ(prop.type, "b");
    EXPECT_EQ(prop.access, "");
}

// 测试解析复杂场景：多个方法、多个属性、多个 annotation
TEST_F(introspection_parser_test, parse_complex_interface) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Complex">
    <method name="Method1">
      <arg type="s" name="arg1" direction="in"/>
      <arg type="i" name="result" direction="out"/>
    </method>
    <method name="Method2">
      <arg type="a{sv}" name="input" direction="in" struct-type="VariantMap"/>
    </method>
    <property name="Property1" type="s" access="read">
      <annotation name="volatile" value="true"/>
    </property>
    <property name="Property2" type="i" access="readwrite">
      <annotation name="custom1" value="value1"/>
      <annotation name="custom2" value="value2"/>
    </property>
    <property name="Property3" type="b" access="write"/>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Complex");

    // 验证方法
    ASSERT_EQ(iface.methods.size(), 2);
    ASSERT_NE(iface.methods.find("Method1"), iface.methods.end());
    ASSERT_NE(iface.methods.find("Method2"), iface.methods.end());

    const method_info& method1 = iface.methods.at("Method1");
    ASSERT_EQ(method1.args.size(), 2);
    EXPECT_EQ(method1.args[0].name, "arg1");
    EXPECT_EQ(method1.args[0].type, "s");
    EXPECT_EQ(method1.args[1].name, "result");
    EXPECT_EQ(method1.args[1].type, "i");

    const method_info& method2 = iface.methods.at("Method2");
    ASSERT_EQ(method2.args.size(), 1);
    EXPECT_EQ(method2.args[0].name, "input");
    EXPECT_EQ(method2.args[0].type, "a{sv}");
    ASSERT_TRUE(method2.args[0].struct_type.has_value());
    EXPECT_EQ(method2.args[0].struct_type.value(), "VariantMap");

    // 验证属性
    ASSERT_EQ(iface.properties.size(), 3);
    ASSERT_NE(iface.properties.find("Property1"), iface.properties.end());
    ASSERT_NE(iface.properties.find("Property2"), iface.properties.end());
    ASSERT_NE(iface.properties.find("Property3"), iface.properties.end());

    const property_info& prop1 = iface.properties.at("Property1");
    EXPECT_EQ(prop1.type, "s");
    EXPECT_EQ(prop1.access, "read");
    EXPECT_TRUE(prop1.is_volatile());

    const property_info& prop2 = iface.properties.at("Property2");
    EXPECT_EQ(prop2.type, "i");
    EXPECT_EQ(prop2.access, "readwrite");
    ASSERT_EQ(prop2.options.size(), 2);
    EXPECT_EQ(prop2.options.at("custom1"), "value1");
    EXPECT_EQ(prop2.options.at("custom2"), "value2");
    EXPECT_FALSE(prop2.is_volatile());

    const property_info& prop3 = iface.properties.at("Property3");
    EXPECT_EQ(prop3.type, "b");
    EXPECT_EQ(prop3.access, "write");
    EXPECT_EQ(prop3.options.size(), 0);
}

// 测试解析空节点
TEST_F(introspection_parser_test, parse_empty_node) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
</node>)";

    node_info node = introspection_parser::parse(xml);

    EXPECT_EQ(node.ifaces.size(), 0);
}

// 测试解析参数 direction 为空
TEST_F(introspection_parser_test, parse_arg_without_direction) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <method name="Method">
      <arg type="s" name="arg1"/>
    </method>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const method_info&    method = iface.methods.at("Method");

    ASSERT_EQ(method.args.size(), 1);
    EXPECT_EQ(method.args[0].name, "arg1");
    EXPECT_EQ(method.args[0].type, "s");
    EXPECT_EQ(method.args[0].direction, "");
}

// 测试解析方法，包含多个参数（超过2个）
TEST_F(introspection_parser_test, parse_method_with_multiple_args) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <method name="MultiArgMethod">
      <arg type="s" name="arg1" direction="in"/>
      <arg type="i" name="arg2" direction="in"/>
      <arg type="b" name="arg3" direction="in"/>
      <arg type="d" name="result" direction="out"/>
    </method>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const method_info&    method = iface.methods.at("MultiArgMethod");

    ASSERT_EQ(method.args.size(), 4);
    EXPECT_EQ(method.args[0].name, "arg1");
    EXPECT_EQ(method.args[0].type, "s");
    EXPECT_EQ(method.args[0].direction, "in");
    EXPECT_EQ(method.args[1].name, "arg2");
    EXPECT_EQ(method.args[1].type, "i");
    EXPECT_EQ(method.args[1].direction, "in");
    EXPECT_EQ(method.args[2].name, "arg3");
    EXPECT_EQ(method.args[2].type, "b");
    EXPECT_EQ(method.args[2].direction, "in");
    EXPECT_EQ(method.args[3].name, "result");
    EXPECT_EQ(method.args[3].type, "d");
    EXPECT_EQ(method.args[3].direction, "out");
}

// 测试解析属性，access 为 read
TEST_F(introspection_parser_test, parse_property_read_only) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <property name="ReadOnlyProperty" type="s" access="read"/>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const property_info&  prop   = iface.properties.at("ReadOnlyProperty");

    EXPECT_EQ(prop.type, "s");
    EXPECT_EQ(prop.access, "read");
    EXPECT_EQ(prop.options.size(), 0);
}

// 测试解析属性，access 为 write
TEST_F(introspection_parser_test, parse_property_write_only) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <property name="WriteOnlyProperty" type="i" access="write"/>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const property_info&  prop   = iface.properties.at("WriteOnlyProperty");

    EXPECT_EQ(prop.type, "i");
    EXPECT_EQ(prop.access, "write");
    EXPECT_EQ(prop.options.size(), 0);
}

// 测试解析属性，access 为 readwrite
TEST_F(introspection_parser_test, parse_property_readwrite) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <property name="ReadWriteProperty" type="b" access="readwrite"/>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const property_info&  prop   = iface.properties.at("ReadWriteProperty");

    EXPECT_EQ(prop.type, "b");
    EXPECT_EQ(prop.access, "readwrite");
    EXPECT_EQ(prop.options.size(), 0);
}

// 测试解析属性，包含多个 annotation
TEST_F(introspection_parser_test, parse_property_with_multiple_annotations) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <property name="AnnotatedProperty" type="s" access="read">
      <annotation name="volatile" value="true"/>
      <annotation name="custom1" value="value1"/>
      <annotation name="custom2" value="value2"/>
      <annotation name="custom3" value="value3"/>
    </property>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const property_info&  prop   = iface.properties.at("AnnotatedProperty");

    EXPECT_EQ(prop.type, "s");
    EXPECT_EQ(prop.access, "read");
    ASSERT_EQ(prop.options.size(), 4);
    EXPECT_EQ(prop.options.at("volatile"), "true");
    EXPECT_EQ(prop.options.at("custom1"), "value1");
    EXPECT_EQ(prop.options.at("custom2"), "value2");
    EXPECT_EQ(prop.options.at("custom3"), "value3");
    EXPECT_TRUE(prop.is_volatile());
}

// 测试解析方法，参数包含复杂类型
TEST_F(introspection_parser_test, parse_method_with_complex_types) {
    std::string xml = R"delim(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <method name="ComplexTypeMethod">
      <arg type="a{sv}" name="dict_arg" direction="in"/>
      <arg type="as" name="array_arg" direction="in"/>
      <arg type="(si)" name="struct_arg" direction="in"/>
      <arg type="v" name="variant_arg" direction="out"/>
    </method>
  </interface>
</node>)delim";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const method_info&    method = iface.methods.at("ComplexTypeMethod");

    ASSERT_EQ(method.args.size(), 4);
    EXPECT_EQ(method.args[0].name, "dict_arg");
    EXPECT_EQ(method.args[0].type, "a{sv}");
    EXPECT_EQ(method.args[0].direction, "in");
    EXPECT_EQ(method.args[1].name, "array_arg");
    EXPECT_EQ(method.args[1].type, "as");
    EXPECT_EQ(method.args[1].direction, "in");
    EXPECT_EQ(method.args[2].name, "struct_arg");
    EXPECT_EQ(method.args[2].type, "(si)");
    EXPECT_EQ(method.args[2].direction, "in");
    EXPECT_EQ(method.args[3].name, "variant_arg");
    EXPECT_EQ(method.args[3].type, "v");
    EXPECT_EQ(method.args[3].direction, "out");
}

// 测试解析方法，参数包含 struct-type
TEST_F(introspection_parser_test, parse_method_with_struct_type_in_multiple_args) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <method name="StructTypeMethod">
      <arg type="a{sv}" name="arg1" direction="in" struct-type="VariantMap"/>
      <arg type="a{ss}" name="arg2" direction="in" struct-type="StringMap"/>
      <arg type="i" name="result" direction="out"/>
    </method>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const method_info&    method = iface.methods.at("StructTypeMethod");

    ASSERT_EQ(method.args.size(), 3);
    EXPECT_EQ(method.args[0].name, "arg1");
    EXPECT_EQ(method.args[0].type, "a{sv}");
    ASSERT_TRUE(method.args[0].struct_type.has_value());
    EXPECT_EQ(method.args[0].struct_type.value(), "VariantMap");
    EXPECT_EQ(method.args[1].name, "arg2");
    EXPECT_EQ(method.args[1].type, "a{ss}");
    ASSERT_TRUE(method.args[1].struct_type.has_value());
    EXPECT_EQ(method.args[1].struct_type.value(), "StringMap");
    EXPECT_EQ(method.args[2].name, "result");
    EXPECT_EQ(method.args[2].type, "i");
    EXPECT_FALSE(method.args[2].struct_type.has_value());
}

// 测试解析多个接口，每个接口包含多个方法和属性
TEST_F(introspection_parser_test, parse_multiple_interfaces_with_multiple_items) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Interface1">
    <method name="Method1">
      <arg type="s" name="arg1" direction="in"/>
    </method>
    <method name="Method2">
      <arg type="i" name="arg1" direction="in"/>
      <arg type="s" name="result" direction="out"/>
    </method>
    <property name="Property1" type="s" access="read"/>
    <property name="Property2" type="i" access="readwrite"/>
  </interface>
  <interface name="org.example.Interface2">
    <method name="Method3"/>
    <property name="Property3" type="b" access="read">
      <annotation name="volatile" value="true"/>
    </property>
  </interface>
  <interface name="org.example.Interface3">
    <property name="Property4" type="d" access="write"/>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    ASSERT_EQ(node.ifaces.size(), 3);

    // 验证 Interface1
    const interface_info& iface1 = node.ifaces.at("org.example.Interface1");
    ASSERT_EQ(iface1.methods.size(), 2);
    ASSERT_EQ(iface1.properties.size(), 2);
    ASSERT_NE(iface1.methods.find("Method1"), iface1.methods.end());
    ASSERT_NE(iface1.methods.find("Method2"), iface1.methods.end());
    ASSERT_NE(iface1.properties.find("Property1"), iface1.properties.end());
    ASSERT_NE(iface1.properties.find("Property2"), iface1.properties.end());

    // 验证 Interface2
    const interface_info& iface2 = node.ifaces.at("org.example.Interface2");
    ASSERT_EQ(iface2.methods.size(), 1);
    ASSERT_EQ(iface2.properties.size(), 1);
    ASSERT_NE(iface2.methods.find("Method3"), iface2.methods.end());
    ASSERT_NE(iface2.properties.find("Property3"), iface2.properties.end());
    EXPECT_TRUE(iface2.properties.at("Property3").is_volatile());

    // 验证 Interface3
    const interface_info& iface3 = node.ifaces.at("org.example.Interface3");
    ASSERT_EQ(iface3.methods.size(), 0);
    ASSERT_EQ(iface3.properties.size(), 1);
    ASSERT_NE(iface3.properties.find("Property4"), iface3.properties.end());
}

// 测试解析方法，参数顺序验证
TEST_F(introspection_parser_test, parse_method_arg_order) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <method name="OrderedMethod">
      <arg type="s" name="first" direction="in"/>
      <arg type="i" name="second" direction="in"/>
      <arg type="b" name="third" direction="in"/>
      <arg type="d" name="fourth" direction="out"/>
    </method>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const method_info&    method = iface.methods.at("OrderedMethod");

    ASSERT_EQ(method.args.size(), 4);
    EXPECT_EQ(method.args[0].name, "first");
    EXPECT_EQ(method.args[1].name, "second");
    EXPECT_EQ(method.args[2].name, "third");
    EXPECT_EQ(method.args[3].name, "fourth");
}

// 测试解析属性，不同类型的 type
TEST_F(introspection_parser_test, parse_property_different_types) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <property name="StringProperty" type="s" access="read"/>
    <property name="IntProperty" type="i" access="read"/>
    <property name="BoolProperty" type="b" access="read"/>
    <property name="DoubleProperty" type="d" access="read"/>
    <property name="ArrayProperty" type="as" access="read"/>
    <property name="DictProperty" type="a{sv}" access="read"/>
    <property name="VariantProperty" type="v" access="read"/>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");

    ASSERT_EQ(iface.properties.size(), 7);
    EXPECT_EQ(iface.properties.at("StringProperty").type, "s");
    EXPECT_EQ(iface.properties.at("IntProperty").type, "i");
    EXPECT_EQ(iface.properties.at("BoolProperty").type, "b");
    EXPECT_EQ(iface.properties.at("DoubleProperty").type, "d");
    EXPECT_EQ(iface.properties.at("ArrayProperty").type, "as");
    EXPECT_EQ(iface.properties.at("DictProperty").type, "a{sv}");
    EXPECT_EQ(iface.properties.at("VariantProperty").type, "v");
}

// 测试解析方法，只有 in 参数
TEST_F(introspection_parser_test, parse_method_only_in_args) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <method name="OnlyInMethod">
      <arg type="s" name="arg1" direction="in"/>
      <arg type="i" name="arg2" direction="in"/>
      <arg type="b" name="arg3" direction="in"/>
    </method>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const method_info&    method = iface.methods.at("OnlyInMethod");

    ASSERT_EQ(method.args.size(), 3);
    EXPECT_EQ(method.args[0].direction, "in");
    EXPECT_EQ(method.args[1].direction, "in");
    EXPECT_EQ(method.args[2].direction, "in");
}

// 测试解析方法，只有 out 参数
TEST_F(introspection_parser_test, parse_method_only_out_args) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <method name="OnlyOutMethod">
      <arg type="s" name="result1" direction="out"/>
      <arg type="i" name="result2" direction="out"/>
    </method>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const method_info&    method = iface.methods.at("OnlyOutMethod");

    ASSERT_EQ(method.args.size(), 2);
    EXPECT_EQ(method.args[0].direction, "out");
    EXPECT_EQ(method.args[1].direction, "out");
}

// 测试解析属性，volatile 为 "yes"
TEST_F(introspection_parser_test, parse_property_volatile_as_yes) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <property name="VolatileProperty" type="s" access="read">
      <annotation name="volatile" value="yes"/>
    </property>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const property_info&  prop   = iface.properties.at("VolatileProperty");

    // volatile="yes" 应该不被识别为 volatile（只有 "true" 或 "1" 才被识别）
    EXPECT_FALSE(prop.is_volatile());
    EXPECT_EQ(prop.options.at("volatile"), "yes");
}

// 测试解析属性，volatile 为 "0"
TEST_F(introspection_parser_test, parse_property_volatile_as_zero) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.example.Test">
    <property name="NonVolatileProperty" type="s" access="read">
      <annotation name="volatile" value="0"/>
    </property>
  </interface>
</node>)";

    node_info node = introspection_parser::parse(xml);

    const interface_info& iface = node.ifaces.at("org.example.Test");
    const property_info&  prop   = iface.properties.at("NonVolatileProperty");

    // volatile="0" 应该不被识别为 volatile
    EXPECT_FALSE(prop.is_volatile());
    EXPECT_EQ(prop.options.at("volatile"), "0");
}
