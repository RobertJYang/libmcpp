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
 * @file test_yaml_compound.cpp
 * @brief YAML复合类型编解码功能的单元测试
 */
#include <gtest/gtest.h>
#include <mc/yaml.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <mc/exception.h>
#include <limits>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

using namespace mc;
using namespace mc::yaml;

namespace mc {
namespace yaml {
namespace test {

// 复合类型编码测试
TEST(YamlEncodeTest, CompoundTypes) {
    // 空数组测试
    variants empty_arr;
    EXPECT_EQ(yaml_encode(variant(empty_arr)), "[]");
    
    // 简单数组测试
    variants arr{variant(1), variant("test"), variant(true)};
    
    // 默认块式风格
    std::string block_style = yaml_encode(variant(arr));
    EXPECT_TRUE(block_style.find("- 1") != std::string::npos);
    EXPECT_TRUE(block_style.find("- test") != std::string::npos);
    EXPECT_TRUE(block_style.find("- true") != std::string::npos);
    
    // 流式风格
    yaml_encode_options flow_options;
    flow_options.flow_style = true;
    std::string flow_style = yaml_encode(variant(arr), flow_options);
    EXPECT_EQ(flow_style, "[1,test,true]");
    
    // 空对象测试
    dict empty_obj;
    EXPECT_EQ(yaml_encode(variant(empty_obj)), "{}");
    
    // 简单对象测试
    dict obj{
        {"name", "张三"},
        {"age", 30},
        {"married", false}
    };
    
    // 默认块式风格
    std::string obj_block_style = yaml_encode(variant(obj));
    EXPECT_TRUE(obj_block_style.find("name: 张三") != std::string::npos);
    EXPECT_TRUE(obj_block_style.find("age: 30") != std::string::npos);
    EXPECT_TRUE(obj_block_style.find("married: false") != std::string::npos);
    
    // 流式风格
    std::string obj_flow_style = yaml_encode(variant(obj), flow_options);
    EXPECT_TRUE(obj_flow_style.find("{") != std::string::npos);
    EXPECT_TRUE(obj_flow_style.find("}") != std::string::npos);
    EXPECT_TRUE(obj_flow_style.find("name:张三") != std::string::npos || 
                obj_flow_style.find("name: 张三") != std::string::npos);
}

// 嵌套复合类型测试
TEST(YamlEncodeTest, NestedCompoundTypes) {
    // 嵌套对象测试
    dict nested_obj{
        {"info", dict{
            {"name", "张三"},
            {"age", 30},
            {"married", false}
        }},
        {"scores", variants{variant(85), variant(92), variant(78)}}
    };
    
    std::string result = yaml_encode(variant(nested_obj));
    
    // 验证嵌套结构
    EXPECT_TRUE(result.find("info:") != std::string::npos);
    EXPECT_TRUE(result.find("name: 张三") != std::string::npos);
    EXPECT_TRUE(result.find("scores:") != std::string::npos);
    EXPECT_TRUE(result.find("- 85") != std::string::npos);
    
    // 流式风格测试
    yaml_encode_options flow_options;
    flow_options.flow_style = true;
    std::string flow_result = yaml_encode(variant(nested_obj), flow_options);
    
    EXPECT_TRUE(flow_result.find("{") != std::string::npos);
    EXPECT_TRUE(flow_result.find("}") != std::string::npos);
    EXPECT_TRUE(flow_result.find("[") != std::string::npos);
    EXPECT_TRUE(flow_result.find("]") != std::string::npos);
}

// 格式化选项测试
TEST(YamlEncodeOptionsTest, PrettyPrint) {
    // 创建测试数据
    dict obj{
        {"name", "张三"},
        {"age", 30},
        {"scores", variants{variant(85), variant(92), variant(78)}}
    };

    // 默认选项（格式化输出）
    std::string formatted = yaml_encode(variant(obj));
    
    // 验证格式化输出包含换行
    EXPECT_NE(formatted.find("\n"), std::string::npos);
    
    // 紧凑输出
    yaml_encode_options compact_options;
    compact_options.pretty_print = false;
    compact_options.flow_style = true;
    std::string compact = yaml_encode(variant(obj), compact_options);
    
    // 验证紧凑输出不包含换行和多余空格
    EXPECT_EQ(compact.find("\n"), std::string::npos);
}

// 键排序测试
TEST(YamlEncodeOptionsTest, SortKeys) {
    // 创建测试数据
    dict obj{
        {"c", 3},
        {"a", 1},
        {"b", 2}
    };

    // 默认选项（保持原有顺序）
    std::string unsorted = yaml_encode(variant(obj));
    
    // 按键排序
    yaml_encode_options sort_options;
    sort_options.sort_keys = true;
    std::string sorted = yaml_encode(variant(obj), sort_options);
    
    // 验证排序后的顺序
    size_t pos_a = sorted.find("a:");
    size_t pos_b = sorted.find("b:");
    size_t pos_c = sorted.find("c:");
    
    EXPECT_TRUE(pos_a < pos_b && pos_b < pos_c);
}

} // namespace test
} // namespace yaml
} // namespace mc