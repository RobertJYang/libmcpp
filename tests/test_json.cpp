/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/**
 * @file test_json.cpp
 * @brief JSON编解码功能的单元测试
 */
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/json.h>
#include <mc/variant.h>
#include <string>
#include <vector>

using namespace mc;
using namespace mc::json;

namespace mc {
namespace json {
namespace test {

// 基本类型编码测试
TEST(JsonEncodeTest, BasicTypes) {
    // null值测试
    EXPECT_EQ(json_encode(variant()), "null");
    EXPECT_EQ(json_encode(variant(nullptr)), "null");

    // 布尔值测试
    EXPECT_EQ(json_encode(variant(true)), "true");
    EXPECT_EQ(json_encode(variant(false)), "false");

    // 字符串测试
    EXPECT_EQ(json_encode(variant(std::string("hello"))), "\"hello\"");
    EXPECT_EQ(json_encode(variant(std::string(""))), "\"\"");
    EXPECT_EQ(json_encode(variant(std::string("hello\nworld"))), "\"hello\\nworld\"");
    EXPECT_EQ(json_encode(variant(std::string("hello\"world"))), "\"hello\\\"world\"");
}

// 数字类型编码测试
TEST(JsonEncodeTest, NumberTypes) {
    // int8_t测试
    EXPECT_EQ(json_encode(variant(int8_t{0})), "0");
    EXPECT_EQ(json_encode(variant(int8_t{127})), "127");
    EXPECT_EQ(json_encode(variant(int8_t{-128})), "-128");

    // uint8_t测试
    EXPECT_EQ(json_encode(variant(uint8_t{0})), "0");
    EXPECT_EQ(json_encode(variant(uint8_t{255})), "255");

    // int16_t测试
    EXPECT_EQ(json_encode(variant(int16_t{-32768})), "-32768");
    EXPECT_EQ(json_encode(variant(int16_t{32767})), "32767");

    // uint16_t测试
    EXPECT_EQ(json_encode(variant(uint16_t{65535})), "65535");

    // int32_t测试
    EXPECT_EQ(json_encode(variant(int32_t{-2147483648})), "-2147483648");
    EXPECT_EQ(json_encode(variant(int32_t{2147483647})), "2147483647");

    // uint32_t测试
    EXPECT_EQ(json_encode(variant(uint32_t{4294967295})), "4294967295");

    // int64_t测试
    EXPECT_EQ(json_encode(variant(int64_t{-9223372036854775807LL})), "-9223372036854775807");
    EXPECT_EQ(json_encode(variant(int64_t{9223372036854775807LL})), "9223372036854775807");

    // uint64_t测试
    EXPECT_EQ(json_encode(variant(uint64_t{18446744073709551615ULL})), "18446744073709551615");

    // double测试
    EXPECT_EQ(json_encode(variant(1.23)), "1.23");
    EXPECT_EQ(json_encode(variant(-1.23)), "-1.23");
    EXPECT_EQ(json_encode(variant(0.0)), "0");
}

// 复合类型编码测试
TEST(JsonEncodeTest, CompoundTypes) {
    // 数组测试
    variants arr{variant(1), variant("test"), variant(true)};
    EXPECT_EQ(json_encode(variant(arr)), "[1,\"test\",true]");

    // 对象测试 - 使用初始化列表构造
    dict obj{{"name", "张三"}, {"age", 30}, {"married", false}};
    EXPECT_EQ(json_encode(variant(obj)), "{\"name\":\"张三\",\"age\":30,\"married\":false}");

    // 嵌套对象测试 - 使用初始化列表构造
    dict        nested_obj{{"info", dict{{"name", "张三"}, {"age", 30}, {"married", false}}},
                           {"scores", variants{variant(1), variant("test"), variant(true)}}};
    std::string result = json_encode(variant(nested_obj));
    EXPECT_TRUE(result.find("\"info\":{") != std::string::npos);
    EXPECT_TRUE(result.find("\"scores\":[") != std::string::npos);
}

// 编码选项测试
TEST(JsonEncodeOptionsTest, PrettyPrint) {
    // 创建测试数据
    dict obj{
        {"name", "张三"}, {"age", 30}, {"scores", variants{variant(85), variant(92), variant(78)}}};

    // 默认选项（紧凑输出）
    std::string compact = json_encode(variant(obj));
    EXPECT_EQ(compact, "{\"name\":\"张三\",\"age\":30,\"scores\":[85,92,78]}");

    // 格式化输出
    json_encode_options pretty_options;
    pretty_options.pretty_print = true;
    std::string formatted       = json_encode(variant(obj), pretty_options);

    // 验证格式化输出包含换行和缩进
    EXPECT_NE(formatted.find("\n"), std::string::npos);
    EXPECT_NE(formatted.find("    "), std::string::npos);
}

TEST(JsonEncodeOptionsTest, EscapeNonAscii) {
    // 创建包含中文的测试数据
    dict obj{{"message", "你好，世界"}};

    // 默认选项（不转义非ASCII字符）
    std::string normal = json_encode(variant(obj));
    EXPECT_EQ(normal, "{\"message\":\"你好，世界\"}");

    // 转义非ASCII字符
    json_encode_options escape_options;
    escape_options.escape_non_ascii = true;
    std::string escaped             = json_encode(variant(obj), escape_options);
    EXPECT_NE(escaped.find("\\u"), std::string::npos);
}

TEST(JsonEncodeOptionsTest, SortKeys) {
    // 创建测试数据
    dict obj{{"c", 3}, {"a", 1}, {"b", 2}};

    // 默认选项（保持原有顺序）
    std::string unsorted = json_encode(variant(obj));
    EXPECT_EQ(unsorted, "{\"c\":3,\"a\":1,\"b\":2}");

    // 按键排序
    json_encode_options sort_options;
    sort_options.sort_keys = true;
    std::string sorted     = json_encode(variant(obj), sort_options);
    EXPECT_EQ(sorted, "{\"a\":1,\"b\":2,\"c\":3}");
}

TEST(JsonEncodeOptionsTest, FloatPrecision) {
    // 创建测试数据
    variant value(3.14159265359);
    // 默认精度测试（-1，使用最大精度）
    std::string default_precision = json_encode(value);

    // 指定不同精度测试
    json_encode_options options;

    // 精度为0
    options.float_precision = 0;
    EXPECT_EQ(json_encode(value, options), "3");

    // 精度为2
    options.float_precision = 2;
    EXPECT_EQ(json_encode(value, options), "3.14");

    // 精度为6
    options.float_precision = 6;
    EXPECT_EQ(json_encode(value, options), "3.141593");

    // 超出最大精度17的情况
    options.float_precision = 20;
    options.normalize();
    EXPECT_EQ(options.float_precision, 17);

    // 小于最小精度-1的情况
    options.float_precision = -2;
    options.normalize();
    EXPECT_EQ(options.float_precision, -1);

    // 特殊数值测试
    variant special_values[] = {variant(0.0),
                                variant(-0.0),
                                variant(1e-10),
                                variant(1e10),
                                variant(std::numeric_limits<double>::min()),
                                variant(std::numeric_limits<double>::max())};

    for (const auto& val : special_values) {
        // 确保可以正确编码和解码
        std::string encoded = json_encode(val);
        variant     decoded = json_decode(encoded);
        EXPECT_DOUBLE_EQ(decoded.as<double>(), val.as<double>());
    }
}

TEST(JsonEncodeOptionsTest, MaxDepth) {
    // 创建深度嵌套的测试数据
    variant nested = variant(dict{
        {"level1", dict{{"level2", dict{{"level3", dict{{"level4", dict{{"level5", 1}}}}}}}}}});

    // 默认深度限制（32）应该可以正常编码
    EXPECT_NO_THROW(json_encode(nested));

    // 设置不同的深度限制
    json_encode_options options;

    // 深度限制为1（只允许简单值或空对象/数组）
    options.max_depth = 1;
    EXPECT_THROW(json_encode(nested, options), parse_error_exception);

    // 深度限制为3（允许3层嵌套）
    options.max_depth = 3;
    EXPECT_THROW(json_encode(nested, options), parse_error_exception);

    // 深度限制为6（可以完整编码）
    options.max_depth = 6;
    EXPECT_NO_THROW(json_encode(nested, options));

    // 超出最大深度512的情况
    options.max_depth = 1000;
    options.normalize();
    EXPECT_EQ(options.max_depth, 512);

    // 小于最小深度1的情况
    options.max_depth = 0;
    options.normalize();
    EXPECT_EQ(options.max_depth, 1);

    // 测试数组嵌套
    variant nested_array =
        variant(variants{variant(variants{variant(variants{variant(variants{variant(1)})})})});

    // 默认深度限制应该可以正常编码
    EXPECT_NO_THROW(json_encode(nested_array));

    // 设置较小的深度限制
    options.max_depth = 2;
    EXPECT_THROW(json_encode(nested_array, options), parse_error_exception);
}

TEST(JsonEncodeOptionsTest, IndentSize) {
    // 创建测试数据
    dict obj{{"name", "张三"}, {"info", dict{{"age", 30}, {"city", "北京"}}}};

    json_encode_options options;
    options.pretty_print = true;

    // 测试不同的缩进大小
    for (int size : {0, 2, 4, 8}) {
        options.indent_size = size;
        std::string result  = json_encode(variant(obj), options);

        // 验证缩进
        if (size > 0) {
            std::string expected_indent(size, ' ');
            EXPECT_NE(result.find("\n" + expected_indent), std::string::npos);
        }
    }

    // 测试规范化
    // 超出最大缩进8的情况
    options.indent_size = 10;
    options.normalize();
    EXPECT_EQ(options.indent_size, 8);

    // 小于最小缩进0的情况
    options.indent_size = -1;
    options.normalize();
    EXPECT_EQ(options.indent_size, 0);
}

TEST(JsonEncodeOptionsTest, CombinedOptions) {
    // 创建测试数据
    dict obj{{"name", "张三"},
             {"age", 30},
             {"score", 98.7654321},
             {"details", dict{{"city", "北京"}, {"address", "朝阳区"}}},
             {"scores", variants{85, 92, 78}}};

    // 组合多个选项
    json_encode_options options;
    options.pretty_print     = true;
    options.indent_size      = 2;
    options.escape_non_ascii = true;
    options.sort_keys        = true;
    options.float_precision  = 2;
    options.max_depth        = 3; // 修改为3，因为测试数据最多有3层嵌套

    std::string result = json_encode(variant(obj), options);

    // 验证结果
    EXPECT_NE(result.find("\n"), std::string::npos);               // 格式化输出
    EXPECT_NE(result.find("  "), std::string::npos);               // 2空格缩进
    EXPECT_NE(result.find("\\u"), std::string::npos);              // 非ASCII转义
    EXPECT_TRUE(result.find("\"age\"") < result.find("\"name\"")); // 键排序
    EXPECT_NE(result.find("98.77"), std::string::npos);            // 浮点数精度

    // 创建超出深度限制的数据
    dict deep_obj{{"level1", dict{{"level2", dict{{"level3", dict{{"level4", 1}}}}}}}};
    EXPECT_THROW(json_encode(variant(deep_obj), options), parse_error_exception);
}

// 基本类型解码测试
TEST(JsonDecodeTest, BasicTypes) {
    // null值测试
    EXPECT_TRUE(json_decode("null").is_null());

    // 布尔值测试
    EXPECT_TRUE(json_decode("true").as<bool>());
    EXPECT_FALSE(json_decode("false").as<bool>());

    // 字符串测试
    EXPECT_EQ(json_decode("\"hello\"").as<std::string>(), "hello");
    EXPECT_EQ(json_decode("\"\"").as<std::string>(), "");
    EXPECT_EQ(json_decode("\"hello\\nworld\"").as<std::string>(), "hello\nworld");
    EXPECT_EQ(json_decode("\"hello\\\"world\"").as<std::string>(), "hello\"world");
}

// 数字类型解码测试
TEST(JsonDecodeTest, NumberTypes) {
    // 整数类型自动选择测试
    EXPECT_TRUE((std::is_same_v<decltype(json_decode("0").as<int8_t>()), int8_t>));
    EXPECT_TRUE((std::is_same_v<decltype(json_decode("128").as<uint8_t>()), uint8_t>));
    EXPECT_TRUE((std::is_same_v<decltype(json_decode("-129").as<int16_t>()), int16_t>));
    EXPECT_TRUE((std::is_same_v<decltype(json_decode("32768").as<uint16_t>()), uint16_t>));

    // int8_t范围测试
    EXPECT_EQ(json_decode("-128").as<int8_t>(), INT8_MIN);
    EXPECT_EQ(json_decode("127").as<int8_t>(), INT8_MAX);

    // uint8_t范围测试
    EXPECT_EQ(json_decode("0").as<uint8_t>(), 0);
    EXPECT_EQ(json_decode("255").as<uint8_t>(), UINT8_MAX);

    // int16_t范围测试
    EXPECT_EQ(json_decode("-32768").as<int16_t>(), INT16_MIN);
    EXPECT_EQ(json_decode("32767").as<int16_t>(), INT16_MAX);

    // uint16_t范围测试
    EXPECT_EQ(json_decode("65535").as<uint16_t>(), UINT16_MAX);

    // int32_t范围测试
    EXPECT_EQ(json_decode("-2147483648").as<int32_t>(), INT32_MIN);
    EXPECT_EQ(json_decode("2147483647").as<int32_t>(), INT32_MAX);

    // uint32_t范围测试
    EXPECT_EQ(json_decode("4294967295").as<uint32_t>(), UINT32_MAX);

    // 浮点数测试
    EXPECT_DOUBLE_EQ(json_decode("1.23").as<double>(), 1.23);
    EXPECT_DOUBLE_EQ(json_decode("-1.23").as<double>(), -1.23);
    EXPECT_DOUBLE_EQ(json_decode("0.0").as<double>(), 0.0);
    EXPECT_DOUBLE_EQ(json_decode("1e-10").as<double>(), 1e-10);
    EXPECT_DOUBLE_EQ(json_decode("1.23e+10").as<double>(), 1.23e+10);
}

// 复合类型解码测试
TEST(JsonDecodeTest, CompoundTypes) {
    // 数组测试
    variant arr = json_decode("[1,\"test\",true]");
    EXPECT_EQ(arr.get_array().size(), 3);
    EXPECT_EQ(arr.get_array()[0].as<int8_t>(), 1);
    EXPECT_EQ(arr.get_array()[1].as<std::string>(), "test");
    EXPECT_EQ(arr.get_array()[2].as<bool>(), true);

    // 对象测试
    variant obj = json_decode("{\"name\":\"张三\",\"age\":30,\"married\":false}");
    EXPECT_EQ(obj.get_object()["name"].as<std::string>(), "张三");
    EXPECT_EQ(obj.get_object()["age"].as<int8_t>(), 30);
    EXPECT_EQ(obj.get_object()["married"].as<bool>(), false);

    // 嵌套对象测试
    variant nested = json_decode(R"(
        {
            "info": {
                "name": "张三",
                "age": 30,
                "married": false
            },
            "scores": [1, "test", true]
        }
    )");
    EXPECT_EQ(nested.get_object()["info"].get_object()["name"].as<std::string>(), "张三");
    EXPECT_EQ(nested.get_object()["scores"].get_array().size(), 3);
}

// 解码选项测试
TEST(JsonDecodeOptionsTest, MaxDepth) {
    // 创建深度嵌套的JSON字符串
    std::string nested_json = R"(
        {
            "level1": {
                "level2": {
                    "level3": {
                        "level4": {
                            "value": 1
                        }
                    }
                }
            }
        }
    )";

    // 默认深度限制（32）应该可以正常解码
    EXPECT_NO_THROW(json_decode(nested_json));

    // 设置较小的深度限制
    json_decode_options depth_options;
    depth_options.max_depth = 2;
    EXPECT_THROW(json_decode(nested_json, depth_options), parse_error_exception);
}

TEST(JsonDecodeOptionsTest, MaxInputLength) {
    // 创建一个长字符串
    std::string long_json = "[";
    for (size_t i = 0; i < 1000; ++i) {
        if (i > 0) {
            long_json += ",";
        }
        long_json += "\"test\"";
    }
    long_json += "]";

    // 默认长度限制（16MB）应该可以正常解码
    EXPECT_NO_THROW(json_decode(long_json));

    // 设置较小的长度限制
    json_decode_options length_options;
    length_options.max_input_length = 100;
    EXPECT_THROW(json_decode(long_json, length_options), parse_error_exception);
}

TEST(JsonDecodeOptionsTest, MaxStringLength) {
    // 创建一个包含长字符串的JSON
    std::string str_value(1000, 'a'); // 1000个'a'字符
    std::string json = "\"" + str_value + "\"";

    // 默认字符串长度限制（64KB）应该可以正常解码
    EXPECT_NO_THROW(json_decode(json));

    // 设置较小的字符串长度限制
    json_decode_options str_options;
    str_options.max_string_length = 100;
    EXPECT_THROW(json_decode(json, str_options), parse_error_exception);

    // 测试对象键名长度限制
    std::string long_key(1000, 'k'); // 1000个'k'字符
    json = "{\"" + long_key + "\":123}";
    EXPECT_THROW(json_decode(json, str_options), parse_error_exception);
}

TEST(JsonDecodeOptionsTest, MaxArraySize) {
    // 创建一个大数组的JSON
    std::string array_json = "[";
    for (size_t i = 0; i < 1000; ++i) {
        if (i > 0) {
            array_json += ",";
        }
        array_json += std::to_string(i);
    }
    array_json += "]";

    // 默认数组大小限制（64K）应该可以正常解码
    EXPECT_NO_THROW(json_decode(array_json));

    // 设置较小的数组大小限制
    json_decode_options array_options;
    array_options.max_array_size = 100;
    EXPECT_THROW(json_decode(array_json, array_options), parse_error_exception);
}

TEST(JsonDecodeOptionsTest, MaxObjectSize) {
    // 创建一个大对象的JSON
    std::string object_json = "{";
    for (size_t i = 0; i < 1000; ++i) {
        if (i > 0) {
            object_json += ",";
        }
        object_json += "\"key" + std::to_string(i) + "\":" + std::to_string(i);
    }
    object_json += "}";

    // 默认对象大小限制（64K）应该可以正常解码
    EXPECT_NO_THROW(json_decode(object_json));

    // 设置较小的对象大小限制
    json_decode_options object_options;
    object_options.max_object_size = 100;
    EXPECT_THROW(json_decode(object_json, object_options), parse_error_exception);
}

TEST(JsonDecodeOptionsTest, OptionNormalization) {
    json_decode_options options;

    // 测试最大嵌套深度规范化
    options.max_depth = 0; // 小于最小值1
    options.normalize();
    EXPECT_EQ(options.max_depth, 1);

    // 测试最大输入长度规范化
    options.max_input_length = 0; // 小于最小值1
    options.normalize();
    EXPECT_EQ(options.max_input_length, 1);

    // 测试最大字符串长度规范化
    options.max_string_length = 0; // 小于最小值1
    options.normalize();
    EXPECT_EQ(options.max_string_length, 1);

    // 测试最大数组大小规范化
    options.max_array_size = 0; // 小于最小值1
    options.normalize();
    EXPECT_EQ(options.max_array_size, 1);

    // 测试最大对象大小规范化
    options.max_object_size = 0; // 小于最小值1
    options.normalize();
    EXPECT_EQ(options.max_object_size, 1);
}

TEST(JsonDecodeOptionsTest, CombinedOptions) {
    // 创建一个复杂的JSON，包含多个可能超出限制的部分
    std::string complex_json = R"(
        {
            "nested": {
                "array": [1, 2, 3, 4, 5],
                "object": {
                    "key1": "value1",
                    "key2": "value2"
                }
            },
            "string": "test"
        }
    )";

    // 组合多个限制选项
    json_decode_options options;
    options.max_depth         = 2;
    options.max_array_size    = 3;
    options.max_object_size   = 1;
    options.max_string_length = 10;

    // 验证组合限制的效果
    EXPECT_THROW(json_decode(complex_json, options), parse_error_exception);

    // 放宽限制，确保可以正常解码
    options.max_depth         = 5;
    options.max_array_size    = 10;
    options.max_object_size   = 10;
    options.max_string_length = 100;
    EXPECT_NO_THROW(json_decode(complex_json, options));
}

// 错误处理测试
TEST(JsonErrorTest, InvalidInput) {
    // 无效的JSON格式
    EXPECT_THROW(json_decode(""), parse_error_exception);
    EXPECT_THROW(json_decode("{"), parse_error_exception);
    EXPECT_THROW(json_decode("["), parse_error_exception);
    EXPECT_THROW(json_decode("\"unclosed string"), parse_error_exception);

    // 无效的数字格式
    EXPECT_THROW(json_decode("12.34.56"), parse_error_exception);
    EXPECT_THROW(json_decode("1e"), parse_error_exception);
    EXPECT_THROW(json_decode("-"), parse_error_exception);

    // 无效的对象格式
    EXPECT_THROW(json_decode("{\"key\"}"), parse_error_exception);
    EXPECT_THROW(json_decode("{\"key\":}"), parse_error_exception);
    EXPECT_THROW(json_decode("{key:1}"), parse_error_exception);

    // 无效的数组格式
    EXPECT_THROW(json_decode("[1,]"), parse_error_exception);
    EXPECT_THROW(json_decode("[1,,2]"), parse_error_exception);
}

// 字符串转义测试
TEST(JsonStringTest, EscapeSequences) {
    // 基本转义序列
    EXPECT_EQ(json_decode("\"\\\"\"").as<std::string>(), "\"");
    EXPECT_EQ(json_decode("\"\\\\\"").as<std::string>(), "\\");
    EXPECT_EQ(json_decode("\"\\/\"").as<std::string>(), "/");
    EXPECT_EQ(json_decode("\"\\b\"").as<std::string>(), "\b");
    EXPECT_EQ(json_decode("\"\\f\"").as<std::string>(), "\f");
    EXPECT_EQ(json_decode("\"\\n\"").as<std::string>(), "\n");
    EXPECT_EQ(json_decode("\"\\r\"").as<std::string>(), "\r");
    EXPECT_EQ(json_decode("\"\\t\"").as<std::string>(), "\t");

    // Unicode转义序列
    EXPECT_EQ(json_decode("\"\\u0020\"").as<std::string>(), " ");
    EXPECT_EQ(json_decode("\"\\u0041\"").as<std::string>(), "A");

    // 无效的转义序列
    EXPECT_THROW(json_decode("\"\\x\""), parse_error_exception);
    EXPECT_THROW(json_decode("\"\\u\""), parse_error_exception);
    EXPECT_THROW(json_decode("\"\\u123\""), parse_error_exception);
    EXPECT_THROW(json_decode("\"\\u123x\""), parse_error_exception);
}

// 空白字符处理测试
TEST(JsonWhitespaceTest, WhitespaceHandling) {
    // 前导和尾随空白
    EXPECT_EQ(json_decode(" 123 ").as<int8_t>(), 123);
    EXPECT_EQ(json_decode("\n\"test\"\t").as<std::string>(), "test");
    EXPECT_EQ(json_decode("\r\n{\n\"key\"\r:\n123\n}\n").get_object()["key"].as<int8_t>(), 123);

    // 对象和数组中的空白
    EXPECT_NO_THROW(json_decode("[ 1 , 2 , 3 ]"));
    EXPECT_NO_THROW(json_decode("{ \"a\" : 1 , \"b\" : 2 }"));
}

// 中文字符串测试
TEST(JsonChineseTest, ChineseCharacters) {
    // 中文字符串编码
    EXPECT_EQ(json_encode(variant(std::string("你好，世界"))), "\"你好，世界\"");

    // 中文字符串解码
    EXPECT_EQ(json_decode("\"你好，世界\"").as<std::string>(), "你好，世界");

    // 包含中文的对象
    dict chinese_obj{{"姓名", "张三"}, {"年龄", 30}, {"城市", "北京"}};

    std::string encoded = json_encode(variant(chinese_obj));
    variant     decoded = json_decode(encoded);

    EXPECT_EQ(decoded.get_object()["姓名"].as<std::string>(), "张三");
    EXPECT_EQ(decoded.get_object()["年龄"].as<int8_t>(), 30);
    EXPECT_EQ(decoded.get_object()["城市"].as<std::string>(), "北京");
}

// 编解码循环测试
TEST(JsonRoundTripTest, EncodeDecode) {
    // 创建复杂的测试数据 - 使用初始化列表构造
    dict original{{"null_value", variant()},
                  {"bool_value", true},
                  {"int_value", int32_t{12345}},
                  {"float_value", 123.45},
                  {"string_value", "test string"},
                  {"array_value", variants{variant(1), variant("two"), variant(true)}},
                  {"object_value", dict{{"key", "value"}}}};

    // 编码然后解码
    std::string encoded = json_encode(variant(original));
    variant     decoded = json_decode(encoded);

    auto decoded_1 = json_decode(decoded.to_string());
    EXPECT_EQ(decoded_1, decoded);

    // 验证结果
    const dict& result = decoded.get_object();
    EXPECT_TRUE(result["null_value"].is_null());
    EXPECT_EQ(result["bool_value"].as<bool>(), true);
    EXPECT_EQ(result["int_value"].as<int32_t>(), 12345);
    EXPECT_DOUBLE_EQ(result["float_value"].as<double>(), 123.45);
    EXPECT_EQ(result["string_value"].as<std::string>(), "test string");
    EXPECT_EQ(result["array_value"].get_array().size(), 3);
    EXPECT_EQ(result["object_value"].get_object()["key"].as<std::string>(), "value");
}

} // namespace test
} // namespace json
} // namespace mc