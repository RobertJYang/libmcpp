/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <glib-2.0/glib.h>
#include <gtest/gtest.h>
#include <mc/dbus/shm/gvariant_convert.h>
#include <mc/dict.h>
#include <mc/variant.h>

using namespace mc;

TEST(GvariantConvertTest, ArrayOfVariantsToGVariant) {
    variants msg_arr;
    msg_arr.push_back(1);
    GVariant* gvariant = dbus::gvariant_convert::to_gvariant(msg_arr, "av");
    ASSERT_NE(gvariant, nullptr);
    ASSERT_EQ(std::string(g_variant_get_type_string(gvariant)), "av");
    auto n = g_variant_n_children(gvariant);
    ASSERT_EQ(n, 1);
    auto child = g_variant_get_child_value(gvariant, 0);
    ASSERT_NE(child, nullptr);
    ASSERT_EQ(std::string(g_variant_get_type_string(child)), "v");
    auto inner_gvariant = g_variant_get_child_value(child, 0);
    ASSERT_NE(inner_gvariant, nullptr);
    ASSERT_EQ(std::string(g_variant_get_type_string(inner_gvariant)), "i");
    ASSERT_EQ(g_variant_get_int32(inner_gvariant), 1);

    auto value = dbus::gvariant_convert::to_mc_variant(gvariant);
    ASSERT_EQ(value.is_array(), true);
    auto array = value.as_array();
    ASSERT_EQ(array.size(), 1);
    ASSERT_TRUE(array[0].is_integer());
    ASSERT_EQ(array[0].as_uint32(), 1);
}

TEST(GvariantConvertTest, ArrayToGVariant) {
    variants msg_arr;
    auto     msg1 = variants({1, "str1", "str2", 3.3});
    auto     msg2 = variants();
    msg2.push_back(2);
    msg2.push_back(variants({"str3", "str4", 4.4}));
    auto msg3 = variants();
    dict d({{"a", 1}, {"b", 2}});
    msg3.push_back(d);
    msg_arr.push_back(msg1);
    msg_arr.push_back(msg2);
    msg_arr.push_back(msg3);
    GVariant* gvariant = dbus::gvariant_convert::to_gvariant(msg_arr, "((issd)(i(ssd))(a{si}))");
    ASSERT_NE(gvariant, nullptr);
    auto value = dbus::gvariant_convert::to_mc_variant(gvariant);
    g_variant_unref(gvariant);
    ASSERT_EQ(value.is_array(), true);
    auto array = value.as_array();
    ASSERT_EQ(array.size(), 3);
    ASSERT_EQ(array[0].is_array(), true);
    auto msg1_unpacked = array[0].as_array();
    ASSERT_EQ(msg1_unpacked.size(), 4);
    ASSERT_EQ(array[1].is_array(), true);
    auto msg2_unpacked = array[1].as_array();
    ASSERT_EQ(msg2_unpacked.size(), 2);
    ASSERT_EQ(msg1_unpacked[0].as_uint32(), 1);
    ASSERT_EQ(msg1_unpacked[1].as_string(), "str1");
    ASSERT_EQ(msg1_unpacked[2].as_string(), "str2");
    ASSERT_EQ(msg1_unpacked[3].as_double(), 3.3);
    ASSERT_EQ(msg2_unpacked[0].as_uint32(), 2);
    ASSERT_EQ(msg2_unpacked[1].is_array(), true);
    auto msg2_array = msg2_unpacked[1].as_array();
    ASSERT_EQ(msg2_array.size(), 3);
    ASSERT_EQ(msg2_array[0].as_string(), "str3");
    ASSERT_EQ(msg2_array[1].as_string(), "str4");
    ASSERT_EQ(msg2_array[2].as_double(), 4.4);
    ASSERT_EQ(array[2].is_array(), true);
    auto msg3_unpacked = array[2].as_array();
    ASSERT_EQ(msg3_unpacked.size(), 1);
    auto msg3_dict = msg3_unpacked[0].as_dict();
    ASSERT_EQ(msg3_dict["a"].as_int32(), 1);
    ASSERT_EQ(msg3_dict["b"].as_int32(), 2);
}

TEST(GvariantConvertTest, ArrayToGVariantWithoutSignature) {
    variants msg_arr;
    auto     msg1 = variants({1, "str1", "str2", 3.3});
    auto     msg2 = variants();
    msg2.push_back(2);
    msg2.push_back(variants({"str3", "str4", 4.4}));
    auto msg3 = variants();
    dict d({{"a", 1}, {"b", 2}});
    msg3.push_back(d);
    auto msg4 = variants();
    msg_arr.push_back(msg1);
    msg_arr.push_back(msg2);
    msg_arr.push_back(msg3);
    msg_arr.push_back(msg4);
    GVariant* gvariant = dbus::gvariant_convert::to_gvariant(msg_arr);
    ASSERT_NE(gvariant, nullptr);
    auto value = dbus::gvariant_convert::to_mc_variant(gvariant);
    g_variant_unref(gvariant);
    ASSERT_EQ(value.is_array(), true);
    auto array = value.as_array();
    ASSERT_EQ(array.size(), 4);
    ASSERT_EQ(array[0].is_array(), true);
    auto msg1_unpacked = array[0].as_array();
    ASSERT_EQ(msg1_unpacked.size(), 4);
    ASSERT_EQ(array[1].is_array(), true);
    auto msg2_unpacked = array[1].as_array();
    ASSERT_EQ(msg2_unpacked.size(), 2);
    ASSERT_EQ(msg1_unpacked[0].as_uint32(), 1);
    ASSERT_EQ(msg1_unpacked[1].as_string(), "str1");
    ASSERT_EQ(msg1_unpacked[2].as_string(), "str2");
    ASSERT_EQ(msg1_unpacked[3].as_double(), 3.3);
    ASSERT_EQ(msg2_unpacked[0].as_uint32(), 2);
    ASSERT_EQ(msg2_unpacked[1].is_array(), true);
    auto msg2_array = msg2_unpacked[1].as_array();
    ASSERT_EQ(msg2_array.size(), 3);
    ASSERT_EQ(msg2_array[0].as_string(), "str3");
    ASSERT_EQ(msg2_array[1].as_string(), "str4");
    ASSERT_EQ(msg2_array[2].as_double(), 4.4);
    ASSERT_EQ(array[2].is_array(), true);
    auto msg3_unpacked = array[2].as_array();
    ASSERT_EQ(msg3_unpacked.size(), 1);
    auto msg3_dict = msg3_unpacked[0].as_dict();
    ASSERT_EQ(msg3_dict["a"].as_int32(), 1);
    ASSERT_EQ(msg3_dict["b"].as_int32(), 2);
    ASSERT_EQ(array[3].is_array(), true);
    auto msg4_unpacked = array[3].as_array();
    ASSERT_EQ(msg4_unpacked.size(), 0);
}

TEST(GvariantConvertTest, BasicTypesToGVariant) {
    // 测试布尔值
    GVariant* gvariant = dbus::gvariant_convert::to_gvariant(variant(true), "b");
    ASSERT_NE(gvariant, nullptr);
    std::string type_str = g_variant_get_type_string(gvariant);
    ASSERT_EQ(type_str, "b");
    auto value = dbus::gvariant_convert::to_mc_variant(gvariant);
    ASSERT_EQ(value.get_type(), type_id::bool_type);
    ASSERT_EQ(value.is_bool(), true);
    ASSERT_EQ(value.as_bool(), true);
    g_variant_unref(gvariant);

    gvariant = dbus::gvariant_convert::to_gvariant(variant(false), "b");
    ASSERT_NE(gvariant, nullptr);
    value = dbus::gvariant_convert::to_mc_variant(gvariant);
    ASSERT_EQ(value.is_bool(), true);
    ASSERT_EQ(value.as_bool(), false);
    g_variant_unref(gvariant);

    // 测试数字
    gvariant = dbus::gvariant_convert::to_gvariant(variant(0), "i");
    ASSERT_NE(gvariant, nullptr);
    value = dbus::gvariant_convert::to_mc_variant(gvariant);
    ASSERT_EQ(value.is_int32(), true);
    ASSERT_EQ(value.as_int32(), 0);
    g_variant_unref(gvariant);

    gvariant = dbus::gvariant_convert::to_gvariant(variant(INT64_MAX), "x");
    ASSERT_NE(gvariant, nullptr);
    value = dbus::gvariant_convert::to_mc_variant(gvariant);
    ASSERT_EQ(value.is_int64(), true);
    ASSERT_EQ(value.as_int64(), INT64_MAX);
    g_variant_unref(gvariant);

    gvariant = dbus::gvariant_convert::to_gvariant(variant(3.14), "d");
    ASSERT_NE(gvariant, nullptr);
    value = dbus::gvariant_convert::to_mc_variant(gvariant);
    ASSERT_EQ(value.is_double(), true);
    ASSERT_DOUBLE_EQ(value.as_double(), 3.14);
    g_variant_unref(gvariant);

    // 测试字符串
    gvariant = dbus::gvariant_convert::to_gvariant(variant(""), "s");
    ASSERT_NE(gvariant, nullptr);
    value = dbus::gvariant_convert::to_mc_variant(gvariant);
    ASSERT_EQ(value.is_string(), true);
    ASSERT_EQ(value.as_string(), "");
    g_variant_unref(gvariant);

    gvariant = dbus::gvariant_convert::to_gvariant(variant(std::string(32, 'a')), "s");
    ASSERT_NE(gvariant, nullptr);
    value = dbus::gvariant_convert::to_mc_variant(gvariant);
    ASSERT_EQ(value.is_string(), true);
    ASSERT_EQ(value.as_string(), std::string(32, 'a'));
    g_variant_unref(gvariant);
}

TEST(GvariantConvertTest, DictToGVariant) {
    // 测试包含基本类型的字典
    mutable_dict basic_dict;
    basic_dict["int1"] = 1;
    basic_dict["int2"] = 2;
    basic_dict["int3"] = 3;
    auto gvariant      = dbus::gvariant_convert::to_gvariant(basic_dict, "a{si}");
    ASSERT_NE(gvariant, nullptr);
    auto value = dbus::gvariant_convert::to_mc_variant(gvariant);
    ASSERT_EQ(value.is_dict(), true);
    auto d = value.as_dict();
    ASSERT_EQ(d["int1"].get_type(), type_id::int32_type);
    ASSERT_EQ(d["int1"].as_int32(), 1);
    ASSERT_EQ(d["int2"].as_int32(), 2);
    ASSERT_EQ(d["int3"].as_int32(), 3);
    g_variant_unref(gvariant);

    // 测试包含数组的字典
    mutable_dict nested_dict;
    nested_dict["arr1"] = variants({1.1, 2.2, 3.3});
    nested_dict["arr2"] = variants({9.9, 8.8, 7.7, 6.6});
    gvariant            = dbus::gvariant_convert::to_gvariant(nested_dict, "a{sad}");
    ASSERT_NE(gvariant, nullptr);
    value = dbus::gvariant_convert::to_mc_variant(gvariant);
    ASSERT_EQ(value.is_dict(), true);
    auto nested_dict_unpacked = value.as_dict();
    auto arr1                 = nested_dict_unpacked["arr1"].as_array();
    ASSERT_EQ(arr1.size(), 3);
    ASSERT_EQ(arr1[0].as_double(), 1.1);
    ASSERT_EQ(arr1[1].as_double(), 2.2);
    ASSERT_EQ(arr1[2].as_double(), 3.3);
    auto arr2 = nested_dict_unpacked["arr2"].as_array();
    ASSERT_EQ(arr2.size(), 4);
    ASSERT_EQ(arr2[0].as_double(), 9.9);
    ASSERT_EQ(arr2[1].as_double(), 8.8);
    ASSERT_EQ(arr2[2].as_double(), 7.7);
    ASSERT_EQ(arr2[3].as_double(), 6.6);
    g_variant_unref(gvariant);

    // 测试包含嵌套字典的字典
    mutable_dict deep_dict;
    mutable_dict inner_dict;
    inner_dict["a"]    = 1;
    inner_dict["b"]    = 2;
    deep_dict["inner"] = inner_dict;
    gvariant           = dbus::gvariant_convert::to_gvariant(deep_dict, "a{sa{si}}");
    ASSERT_NE(gvariant, nullptr);
    value = dbus::gvariant_convert::to_mc_variant(gvariant);
    ASSERT_EQ(value.is_dict(), true);
    auto deep_dict_unpacked  = value.as_dict();
    auto inner_dict_unpacked = deep_dict_unpacked["inner"].as_dict();
    ASSERT_EQ(inner_dict_unpacked["a"].as_int32(), 1);
    ASSERT_EQ(inner_dict_unpacked["b"].as_int32(), 2);
    g_variant_unref(gvariant);
}