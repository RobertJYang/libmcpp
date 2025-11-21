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

#include <gtest/gtest.h>
#include <mc/dbus/shm/local_msg.h>
#include <mc/dbus/shm/serialize.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <limits>
#include <vector>

using namespace mc;
using namespace mc::dbus;
using namespace mc::dbus::serialize;

TEST(SerializeTest, SerializeArrays) {
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
    auto packed = dbus::serialize::pack(msg_arr);

    auto unpacked = dbus::serialize::unpack(packed);
    ASSERT_EQ(unpacked.size(), 3);
    auto msg1_unpacked = unpacked[0].as_array();
    auto msg2_unpacked = unpacked[1].as_array();
    ASSERT_EQ(msg1_unpacked.size(), 4);
    ASSERT_EQ(msg2_unpacked.size(), 2);
    ASSERT_EQ(msg1_unpacked[0].as_uint32(), 1);
    ASSERT_EQ(msg1_unpacked[1].as_string(), "str1");
    ASSERT_EQ(msg1_unpacked[2].as_string(), "str2");
    ASSERT_EQ(msg1_unpacked[3].as_double(), 3.3);
    ASSERT_EQ(msg2_unpacked[0].as_uint32(), 2);
    auto msg2_array = msg2_unpacked[1].as_array();
    ASSERT_EQ(msg2_array.size(), 3);
    ASSERT_EQ(msg2_array[0].as_string(), "str3");
    ASSERT_EQ(msg2_array[1].as_string(), "str4");
    ASSERT_EQ(msg2_array[2].as_double(), 4.4);
    auto msg3_unpacked = unpacked[2].as_array();
    ASSERT_EQ(msg3_unpacked.size(), 1);
    ASSERT_EQ(msg3_unpacked[0].as_dict()["a"].as_int32(), 1);
    ASSERT_EQ(msg3_unpacked[0].as_dict()["b"].as_int32(), 2);
}

TEST(SerializeTest, SerializeMsg) {
    variants msg_arr;
    auto     msg1 = new dbus::local_msg("bmc.kepler.test1", "/bmc/kepler/test/obj1",
                                        "bmc.kepler.test.intf1", "TestMethod1");
    msg1->append("a{ss}", dict({{"a", "str1"}, {"b", "str2"}}));
    msg1->set_local_call(true);
    msg1->set_sender(":123");
    msg1->set_serial(123);
    auto data     = msg1->pack();
    auto unpacked = dbus::serialize::unpack(data);
    ASSERT_EQ(unpacked[0].as_uint32(), DBUS_MESSAGE_TYPE_METHOD_CALL);
    ASSERT_EQ(unpacked[1].as_string(), "bmc.kepler.test1");
    ASSERT_EQ(unpacked[2].as_string(), "/bmc/kepler/test/obj1");
    ASSERT_EQ(unpacked[3].as_string(), "bmc.kepler.test.intf1");
    ASSERT_EQ(unpacked[4].as_string(), "TestMethod1");
    ASSERT_EQ(unpacked[5].as_string(), "NA");
    ASSERT_EQ(unpacked[6].as_string(), "a{ss}");
    auto msg1_args = unpacked[7].as_array();
    ASSERT_TRUE(msg1_args[0].is_dict());
    ASSERT_EQ(msg1_args[0].as_dict()["a"].as_string(), "str1");
    ASSERT_EQ(msg1_args[0].as_dict()["b"].as_string(), "str2");
    ASSERT_EQ(unpacked[8].as_string(), ":123");
    ASSERT_EQ(unpacked[9].as_uint32(), 123);
    ASSERT_EQ(unpacked[10].as_bool(), true);
    ASSERT_EQ(unpacked[11].as_uint32(), 0);

    auto msg2 = new dbus::local_msg("bmc.kepler.test2", "/bmc/kepler/test/obj2",
                                    "bmc.kepler.test.intf2", "TestMethod2");
    msg2->set_local_call(false);
    msg2->set_sender(":456");
    msg2->set_serial(789);
    msg2->error("TestErrorName2", "test error msg2");
    msg2->set_serial(456);
    data     = msg2->pack();
    unpacked = dbus::serialize::unpack(data);
    ASSERT_EQ(unpacked[0].as_uint32(), DBUS_MESSAGE_TYPE_ERROR);
    ASSERT_EQ(unpacked[1].as_string(), ":456");
    ASSERT_EQ(unpacked[2].as_string(), "/bmc/kepler/test/obj2");
    ASSERT_EQ(unpacked[3].as_string(), "bmc.kepler.test.intf2");
    ASSERT_EQ(unpacked[4].as_string(), "TestMethod2");
    ASSERT_EQ(unpacked[5].as_string(), "TestErrorName2");
    ASSERT_EQ(unpacked[6].as_string(), "s");
    auto msg2_args = unpacked[7].as_array();
    ASSERT_EQ(msg2_args[0].as_string(), "test error msg2");
    ASSERT_EQ(unpacked[8].as_string(), ":456");
    ASSERT_EQ(unpacked[9].as_uint32(), 456);
    ASSERT_EQ(unpacked[10].as_bool(), false);
    ASSERT_EQ(unpacked[11].as_uint32(), 789);
}

TEST(SerializeTest, SerializeBasicTypes) {
    variants msg_arr;
    // 测试布尔值
    msg_arr.push_back(true);
    msg_arr.push_back(false);
    // 测试数字
    msg_arr.push_back(0);
    msg_arr.push_back(1);
    msg_arr.push_back(-1);
    msg_arr.push_back(int64_t(0x7F));
    msg_arr.push_back(int64_t(0x80));
    msg_arr.push_back(int64_t(0x7FFF));
    msg_arr.push_back(int64_t(0x8000));
    msg_arr.push_back(int64_t(0x7FFFFFFF));
    msg_arr.push_back(int64_t(0x80000000));
    msg_arr.push_back(int64_t(0x7FFFFFFFFFFFFFFF));
    msg_arr.push_back(int64_t(0x8000000000000000));
    msg_arr.push_back(3.14);
    // 测试字符串
    msg_arr.push_back("");
    msg_arr.push_back("short");
    msg_arr.push_back(std::string(MAX_COOKIE - 1, 'a'));
    msg_arr.push_back(std::string(MAX_COOKIE, 'b'));
    msg_arr.push_back(std::string(0xFFFF, 'c'));
    msg_arr.push_back(std::string(0x10000, 'd'));

    auto packed   = dbus::serialize::pack(msg_arr);
    auto unpacked = dbus::serialize::unpack(packed);

    ASSERT_EQ(unpacked.size(), msg_arr.size());
    ASSERT_EQ(unpacked[0].as_bool(), true);
    ASSERT_EQ(unpacked[1].as_bool(), false);
    ASSERT_EQ(unpacked[2].as_int32(), 0);
    ASSERT_EQ(unpacked[3].as_int32(), 1);
    ASSERT_EQ(unpacked[4].as_int32(), -1);
    ASSERT_EQ(unpacked[5].as_int32(), 0x7F);
    ASSERT_EQ(unpacked[6].as_int32(), 0x80);
    ASSERT_EQ(unpacked[7].as_int32(), 0x7FFF);
    ASSERT_EQ(unpacked[8].as_int32(), 0x8000);
    ASSERT_EQ(unpacked[9].as_int32(), 0x7FFFFFFF);
    ASSERT_EQ(unpacked[10].as_int32(), 0x80000000);
    ASSERT_EQ(unpacked[11].as_int64(), 0x7FFFFFFFFFFFFFFF);
    ASSERT_EQ(unpacked[12].as_int64(), 0x8000000000000000);
    ASSERT_DOUBLE_EQ(unpacked[13].as_double(), 3.14);
    ASSERT_EQ(unpacked[14].as_string(), "");
    ASSERT_EQ(unpacked[15].as_string(), "short");
    ASSERT_EQ(unpacked[16].as_string(), std::string(MAX_COOKIE - 1, 'a'));
    ASSERT_EQ(unpacked[17].as_string(), std::string(MAX_COOKIE, 'b'));
    ASSERT_EQ(unpacked[18].as_string(), std::string(0xFFFF, 'c'));
    ASSERT_EQ(unpacked[19].as_string(), std::string(0x10000, 'd'));
}

TEST(SerializeTest, SerializeNestedArrays) {
    variants msg_arr;
    // 测试空数组
    msg_arr.push_back(variants());
    // 测试包含基本类型的数组
    msg_arr.push_back(variants({1, "str", 3.14}));
    // 测试嵌套数组
    variants nested1;
    nested1.push_back(variants({1, 2}));
    nested1.push_back(variants({3, 4}));
    msg_arr.push_back(nested1);
    // 测试深度嵌套
    variants     deep_nested;
    mc::variants v1 = {1};
    mc::variants v2 = {mc::variant(v1)};
    mc::variants v3 = {mc::variant(v2)};
    deep_nested.push_back(v3);
    msg_arr.push_back(deep_nested);
    // 测试混合类型的数组
    variants mixed;
    mixed.push_back(1);
    mixed.push_back("str");
    mixed.push_back(3.14);
    mixed.push_back(true);
    mixed.push_back(variants({1, 2}));
    msg_arr.push_back(mixed);

    auto packed   = dbus::serialize::pack(msg_arr);
    auto unpacked = dbus::serialize::unpack(packed);

    ASSERT_EQ(unpacked.size(), msg_arr.size());
    ASSERT_EQ(unpacked[0].as_array().size(), 0);
    auto arr1 = unpacked[1].as_array();
    ASSERT_EQ(arr1.size(), 3);
    ASSERT_EQ(arr1[0].as_int32(), 1);
    ASSERT_EQ(arr1[1].as_string(), "str");
    ASSERT_DOUBLE_EQ(arr1[2].as_double(), 3.14);
    auto arr2 = unpacked[2].as_array();
    ASSERT_EQ(arr2.size(), 2);
    ASSERT_EQ(arr2[0].as_array()[0].as_int32(), 1);
    ASSERT_EQ(arr2[0].as_array()[1].as_int32(), 2);
    ASSERT_EQ(arr2[1].as_array()[0].as_int32(), 3);
    ASSERT_EQ(arr2[1].as_array()[1].as_int32(), 4);
    auto arr3 = unpacked[3].as_array();
    ASSERT_EQ(arr3.size(), 1);
    ASSERT_TRUE(arr3[0].is_array());
    auto arr3_0 = arr3[0].as_array();
    ASSERT_EQ(arr3_0.size(), 1);
    ASSERT_TRUE(arr3_0[0].is_array());
    auto arr3_0_0 = arr3_0[0].as_array();
    ASSERT_EQ(arr3_0_0.size(), 1);
    ASSERT_TRUE(arr3_0_0[0].is_array());
    auto arr3_0_0_0 = arr3_0_0[0].as_array();
    ASSERT_EQ(arr3_0_0_0.size(), 1);
    ASSERT_EQ(arr3_0_0_0[0].get_type(), type_id::int64_type);
    ASSERT_EQ(arr3_0_0_0[0].as_int64(), 1);
    auto arr4 = unpacked[4].as_array();
    ASSERT_EQ(arr4.size(), 5);
    ASSERT_EQ(arr4[0].as_int32(), 1);
    ASSERT_EQ(arr4[1].as_string(), "str");
    ASSERT_DOUBLE_EQ(arr4[2].as_double(), 3.14);
    ASSERT_EQ(arr4[3].as_bool(), true);
    ASSERT_EQ(arr4[4].as_array()[0].as_int32(), 1);
    ASSERT_EQ(arr4[4].as_array()[1].as_int32(), 2);
}

TEST(SerializeTest, SerializeDicts) {
    variants msg_arr;
    // 测试包含基本类型的字典
    dict basic_dict;
    basic_dict["int"]    = 1;
    basic_dict["str"]    = "string";
    basic_dict["double"] = 3.14;
    basic_dict["bool"]   = true;
    msg_arr.push_back(basic_dict);
    // 测试包含嵌套数组的字典
    dict nested_dict;
    nested_dict["arr"]        = variants({1, 2, 3});
    nested_dict["nested_arr"] = variants({variants({1, 2}), variants({3, 4})});
    msg_arr.push_back(nested_dict);
    // 测试包含嵌套字典的字典
    dict deep_dict;
    dict inner_dict;
    inner_dict["a"]    = 1;
    inner_dict["b"]    = 2;
    deep_dict["inner"] = inner_dict;
    msg_arr.push_back(deep_dict);

    auto packed   = dbus::serialize::pack(msg_arr);
    auto unpacked = dbus::serialize::unpack(packed);

    ASSERT_EQ(unpacked.size(), msg_arr.size());
    auto dict1 = unpacked[0].as_dict();
    ASSERT_EQ(dict1["int"].as_int32(), 1);
    ASSERT_EQ(dict1["str"].as_string(), "string");
    ASSERT_DOUBLE_EQ(dict1["double"].as_double(), 3.14);
    ASSERT_EQ(dict1["bool"].as_bool(), true);
    auto dict2 = unpacked[1].as_dict();
    ASSERT_EQ(dict2["arr"].as_array().size(), 3);
    ASSERT_EQ(dict2["arr"].as_array()[0].as_int32(), 1);
    ASSERT_EQ(dict2["nested_arr"].as_array().size(), 2);
    ASSERT_EQ(dict2["nested_arr"].as_array()[0].as_array()[0].as_int32(), 1);
    auto dict3 = unpacked[2].as_dict();
    ASSERT_EQ(dict3["inner"].as_dict()["a"].as_int32(), 1);
    ASSERT_EQ(dict3["inner"].as_dict()["b"].as_int32(), 2);
}

TEST(SerializeTest, SerializeMixedTypes) {
    variants msg_arr;
    // 测试混合类型的数组
    variants mixed_arr;
    mixed_arr.push_back(1);
    mixed_arr.push_back("str");
    mixed_arr.push_back(3.14);
    mixed_arr.push_back(true);
    mixed_arr.push_back(variants({1, 2}));
    mixed_arr.push_back(dict({{"a", 1}, {"b", 2}}));
    msg_arr.push_back(mixed_arr);
    // 测试混合类型的字典
    dict mixed_dict;
    mixed_dict["int"]  = 1;
    mixed_dict["str"]  = "string";
    mixed_dict["arr"]  = variants({1, 2, 3});
    mixed_dict["dict"] = dict({{"a", 1}, {"b", 2}});
    msg_arr.push_back(mixed_dict);

    auto packed   = dbus::serialize::pack(msg_arr);
    auto unpacked = dbus::serialize::unpack(packed);

    ASSERT_EQ(unpacked.size(), msg_arr.size());
    auto arr = unpacked[0].as_array();
    ASSERT_EQ(arr.size(), 6);
    ASSERT_EQ(arr[0].as_int32(), 1);
    ASSERT_EQ(arr[1].as_string(), "str");
    ASSERT_DOUBLE_EQ(arr[2].as_double(), 3.14);
    ASSERT_EQ(arr[3].as_bool(), true);
    ASSERT_EQ(arr[4].as_array()[0].as_int32(), 1);
    ASSERT_EQ(arr[4].as_array()[1].as_int32(), 2);
    ASSERT_EQ(arr[5].as_dict()["a"].as_int32(), 1);
    ASSERT_EQ(arr[5].as_dict()["b"].as_int32(), 2);
    auto dict = unpacked[1].as_dict();
    ASSERT_EQ(dict["int"].as_int32(), 1);
    ASSERT_EQ(dict["str"].as_string(), "string");
    ASSERT_EQ(dict["arr"].as_array().size(), 3);
    ASSERT_EQ(dict["dict"].as_dict()["a"].as_int32(), 1);
    ASSERT_EQ(dict["dict"].as_dict()["b"].as_int32(), 2);
}

TEST(SerializeTest, WriteVariantElements) {
    dbus::signature_iterator      it = "a{ss}s";
    dbus::serialize::write_buffer wb;
    mc::variants                  args;
    args.push_back(dict({{"a", "str1"}, {"b", "str2"}}));
    args.push_back("test");
    wb.write_variant_elements(it, args);
    auto packed   = wb.to_string();
    auto unpacked = dbus::serialize::unpack(packed);
    ASSERT_EQ(unpacked.size(), 1);
    ASSERT_EQ(unpacked[0].as_array().size(), 2);
    ASSERT_EQ(unpacked[0].as_array()[0].as_dict()["a"].as_string(), "str1");
    ASSERT_EQ(unpacked[0].as_array()[0].as_dict()["b"].as_string(), "str2");
    ASSERT_EQ(unpacked[0].as_array()[1].as_string(), "test");
}

TEST(SerializeTest, WriteNumberVariousTypes) {
    write_buffer buf;
    buf.write_arg(static_cast<int64_t>(0), 0);
    buf.write_arg(static_cast<int64_t>(-123), 0);
    buf.write_arg(static_cast<int64_t>(123), 0);
    buf.write_arg(static_cast<int64_t>(0x1234), 0);
    buf.write_arg(static_cast<int64_t>(0x12345678), 0);
    buf.write_arg(static_cast<int64_t>(0x123456789ABCDEF0LL), 0);
    buf.write_arg(3.14, 0);
    buf.write_arg(static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1, 0);

    auto       packed = buf.to_string();
    read_buffer rb(packed);

    auto expect_int64 = [&](int64_t expected) {
        uint8_t type = 0;
        auto*   t    = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
        ASSERT_NE(t, nullptr);
        type = *t;
        auto value = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
        EXPECT_EQ(value.as_int64(), expected);
    };

    expect_int64(0);
    expect_int64(-123);
    expect_int64(123);
    expect_int64(0x1234);
    expect_int64(0x12345678);
    expect_int64(0x123456789ABCDEF0LL);

    {
        uint8_t type = 0;
        auto*   t    = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
        ASSERT_NE(t, nullptr);
        type = *t;
        auto value = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
        EXPECT_DOUBLE_EQ(value.as_double(), 3.14);
    }

    {
        uint8_t type = 0;
        auto*   t    = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
        ASSERT_NE(t, nullptr);
        type = *t;
        auto value = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
        EXPECT_DOUBLE_EQ(value.as_double(),
                         static_cast<double>(static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1));
    }
}

TEST(SerializeTest, WriteStringVariousLengths) {
    write_buffer buf1;
    buf1.write_arg(std::string("short"), 0);
    write_buffer buf2;
    buf2.write_arg(std::string(1000, 'a'), 0);
    write_buffer buf3;
    buf3.write_arg(std::string(0x20000, 'b'), 0);
    write_buffer buf4;
    buf4.write_arg(std::string(""), 0);

    auto verify_string = [](const write_buffer& buf, const std::string& expected) {
        auto       packed = buf.to_string();
        read_buffer rb(packed);
        uint8_t     type = 0;
        auto*       t    = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
        ASSERT_NE(t, nullptr);
        type = *t;
        auto value = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
        EXPECT_EQ(value.as_string(), expected);
    };

    verify_string(buf1, "short");
    verify_string(buf2, std::string(1000, 'a'));
    verify_string(buf3, std::string(0x20000, 'b'));
    verify_string(buf4, "");
}

TEST(SerializeTest, WriteLargeArray) {
    variants large_arr;
    for (int i = 0; i < 100; ++i) {
        large_arr.push_back(i);
    }
    write_buffer buf;
    buf.write_arg(variant(large_arr), 0);

    auto packed = buf.to_string();
    read_buffer rb(packed);
    uint8_t     type = 0;
    auto*       t    = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
    ASSERT_NE(t, nullptr);
    type = *t;
    auto value = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
    ASSERT_TRUE(value.is_array());
    ASSERT_EQ(value.as_array().size(), large_arr.size());
    for (std::size_t i = 0; i < large_arr.size(); ++i) {
        EXPECT_EQ(value.as_array()[i].as_int32(), static_cast<int32_t>(i));
    }
}

TEST(SerializeTest, WriteInnerCrossBlock) {
    write_buffer buf;
    buf.write_arg(std::string(5000, 'x'), 0);

    auto       packed = buf.to_string();
    read_buffer rb(packed);
    uint8_t     type = 0;
    auto*       t    = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
    ASSERT_NE(t, nullptr);
    type = *t;
    auto value = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
    EXPECT_EQ(value.as_string(), std::string(5000, 'x'));
}

TEST(SerializeTest, WriteArgWithSignatureVariousTypes) {
    write_buffer buf;
    buf.write_arg_with_signature(signature_iterator("(isd)"), variant(variants{1, "test", 3.14}), 0);

    auto       packed = buf.to_string();
    read_buffer rb(packed);
    uint8_t     type = 0;
    auto*       t    = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
    ASSERT_NE(t, nullptr);
    type = *t;
    auto value = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
    ASSERT_TRUE(value.is_array());
    auto tuple = value.as_array();
    ASSERT_EQ(tuple.size(), 3U);
    EXPECT_EQ(tuple[0].as_int32(), 1);
    EXPECT_EQ(tuple[1].as_string(), "test");
    EXPECT_DOUBLE_EQ(tuple[2].as_double(), 3.14);
}

TEST(SerializeTest, WriteArrayOrDictByteType) {
    write_buffer buf;
    buf.write_arg_with_signature(signature_iterator("ay"), variant(std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04}), 0);

    auto       packed = buf.to_string();
    read_buffer rb(packed);
    uint8_t     type = 0;
    auto*       t    = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
    ASSERT_NE(t, nullptr);
    type = *t;
    auto value = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
    ASSERT_TRUE(value.is_string());
    auto str = value.as_string();
    ASSERT_EQ(str.size(), 4U);
    EXPECT_EQ(static_cast<uint8_t>(str[0]), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(str[1]), 0x02);
    EXPECT_EQ(static_cast<uint8_t>(str[2]), 0x03);
    EXPECT_EQ(static_cast<uint8_t>(str[3]), 0x04);
}

TEST(SerializeTest, ReadValueInvalidType) {
    std::string invalid_data;
    invalid_data.push_back(static_cast<uint8_t>(255));
    invalid_data.push_back(0);
    read_buffer rb(invalid_data);
    EXPECT_THROW(rb.read_value(255, 0), mc::invalid_arg_exception);
}

TEST(SerializeTest, ReadLongStringInvalidCookie) {
    std::string invalid_data;
    uint8_t type_byte = static_cast<uint8_t>(data_type::long_string) | (3 << VALUE_SHIFT);
    invalid_data.push_back(type_byte);
    invalid_data.push_back(0);
    invalid_data.push_back(0);
    invalid_data.push_back(0);
    read_buffer rb(invalid_data);
    uint8_t type = static_cast<uint8_t>(data_type::long_string);
    EXPECT_THROW(rb.read_value(type, 3), mc::invalid_arg_exception);
}

TEST(SerializeTest, ReadTableAsDict) {
    write_buffer wb;
    dict d;
    d["key1"] = "value1";
    d["key2"] = 42;
    variant v(d);
    wb.write_arg(v, 0);
    auto packed = wb.to_string();

    read_buffer rb(packed);
    uint8_t type = 0;
    auto t = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
    ASSERT_NE(t, nullptr);
    type = *t;
    auto result = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
    EXPECT_TRUE(result.is_dict());
    EXPECT_EQ(result.as_dict()["key1"].as_string(), "value1");
    EXPECT_EQ(result.as_dict()["key2"].as_int32(), 42);
}

TEST(SerializeTest, ReadValueBooleanFalse) {
    write_buffer wb;
    variant v(false);
    wb.write_arg(v, 0);
    auto packed = wb.to_string();

    read_buffer rb(packed);
    uint8_t type = 0;
    auto t = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
    ASSERT_NE(t, nullptr);
    type = *t;
    auto result = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
    EXPECT_FALSE(result.as_bool());
}

TEST(SerializeTest, ReadValueBooleanTrue) {
    write_buffer wb;
    variant v(true);
    wb.write_arg(v, 0);
    auto packed = wb.to_string();

    read_buffer rb(packed);
    uint8_t type = 0;
    auto t = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
    ASSERT_NE(t, nullptr);
    type = *t;
    auto result = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
    EXPECT_TRUE(result.as_bool());
}

TEST(SerializeTest, ReadValueNumberReal) {
    write_buffer wb;
    variant v(3.14159);
    wb.write_arg(v, 0);
    auto packed = wb.to_string();

    read_buffer rb(packed);
    uint8_t type = 0;
    auto t = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
    ASSERT_NE(t, nullptr);
    type = *t;
    auto result = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
    EXPECT_DOUBLE_EQ(result.as_double(), 3.14159);
}

TEST(SerializeTest, ReadValueUserdata) {
    write_buffer wb;
    uint64_t ptr_value = 0x12345678;
    variant v(ptr_value);
    wb.write_arg(v, 0);
    auto packed = wb.to_string();

    read_buffer rb(packed);
    uint8_t type = 0;
    auto t = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
    ASSERT_NE(t, nullptr);
    type = *t;
    auto result = rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
    EXPECT_EQ(result.as_uint64(), ptr_value);
}

TEST(SerializeTest, DeserializeInvalidFormat) {
    std::string invalid_msg = "abc";
    EXPECT_THROW(deserialize(invalid_msg), mc::invalid_arg_exception);
}

TEST(SerializeTest, DeserializeEmptyMessage) {
    std::string empty_msg;
    empty_msg.resize(4, 0);
    auto result = deserialize(empty_msg);
    EXPECT_TRUE(result.empty());
}

TEST(SerializeTest, WriteArgDepthExceedsLimit) {
    write_buffer buf;
    variant v_int = 42;
    EXPECT_THROW(buf.write_arg(v_int, MAX_DEPTH + 1), mc::exception);
}

// 测试 deserialize_variant 读取 blob 时 buffer 不足
TEST(SerializeTest, BlobInsufficientBuffer) {
    // 构造一个包含 gvariant 类型的序列化数据，但 buffer 不足
    write_buffer wb;
    // 创建一个包含 blob 的 variant
    variant blob_variant(std::string(100, 'x'));
    wb.write_arg(blob_variant, 0);
    auto packed = wb.to_string();

    // 截断 buffer，使其不足以读取完整的 blob
    std::string truncated = packed.substr(0, packed.size() / 2);
    read_buffer rb(truncated);

    // 尝试读取，应该抛出异常或返回 nullptr
    uint8_t type = 0;
    auto t = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
    if (t != nullptr) {
        type = *t;
        // 如果类型是 gvariant，尝试读取应该失败
        if ((type & TYPE_MASK) == static_cast<uint8_t>(data_type::gvariant)) {
            EXPECT_THROW(rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT), mc::exception);
        }
    }
}