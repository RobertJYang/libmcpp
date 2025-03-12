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

/**
 * @file test_format_benchmark.cpp
 * @brief 测试字符串格式化函数的性能
 */
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <mc/dict.h>
#include <mc/string.h>

namespace mc {
namespace test {

/**
 * @brief 用于测量性能的助手函数
 */
template <typename Func>
double measure_time(Func&& func, int iterations = 10000) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        func();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    return elapsed.count() / iterations;
}

/**
 * @brief 性能测试类
 */
class FormatBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化测试数据
        small_args = {{"host", "example.com"}, {"port", 8080}};

        medium_args = {{"host", "example.com"},     {"port", 8080},  {"protocol", "https"},
                       {"enabled", true},           {"ratio", 0.75}, {"username", "admin"},
                       {"password", "password123"}, {"timeout", 30}};

        // 创建一个大型参数字典
        large_args = medium_args;
        // 使用mutable_dict的operator()方法添加更多键值对
        for (int i = 0; i < 50; ++i) {
            large_args(std::string("key") + std::to_string(i),
                       std::string("value") + std::to_string(i));
        }

        // 简单格式字符串
        simple_format = "连接到 ${host}:${port}";

        // 中等复杂度的格式字符串
        medium_format = "协议: ${protocol}, 主机: ${host}, 端口: ${port}, 启用: ${enabled}, "
                        "比率: ${ratio}, 用户名: ${username}, 超时: ${timeout}";

        // 复杂格式字符串（含有多个占位符和不存在的键）
        complex_format =
            "协议: ${protocol}, 主机: ${host}, 端口: ${port}, 启用: ${enabled}, "
            "比率: ${ratio}, 用户名: ${username}, 密码: ${password}, 超时: ${timeout}, "
            "不存在的键: ${missing_key}, 另一个不存在的键: ${another_missing_key}";

        // 没有占位符的格式字符串
        no_placeholders = "这是一个没有占位符的字符串，不需要替换任何内容。";
    }

    void TearDown() override {
        // 清理资源
    }

    // 测试数据
    mutable_dict small_args;
    mutable_dict medium_args;
    mutable_dict large_args;

    std::string simple_format;
    std::string medium_format;
    std::string complex_format;
    std::string no_placeholders;
};

/**
 * @brief 测试format函数在不同情况下的性能
 */
TEST_F(FormatBenchmarkTest, FormatPerformance) {
    // 场景1：简单格式字符串 + 小型参数字典
    double time1 = measure_time([&]() {
        // 确保调用接受dict的format函数
        mc::format(simple_format, static_cast<const dict&>(small_args));
    });
    std::cout << "简单格式 + 小型参数字典: " << time1 << " ms/次" << std::endl;

    // 场景2：中等复杂度格式字符串 + 中型参数字典
    double time2 = measure_time([&]() {
        // 确保调用接受dict的format函数
        mc::format(medium_format, static_cast<const dict&>(medium_args));
    });
    std::cout << "中等格式 + 中型参数字典: " << time2 << " ms/次" << std::endl;

    // 场景3：复杂格式字符串 + 大型参数字典
    double time3 = measure_time([&]() {
        // 确保调用接受dict的format函数
        mc::format(complex_format, large_args);
    });
    std::cout << "复杂格式 + 大型参数字典: " << time3 << " ms/次" << std::endl;

    // 场景4：没有占位符的格式字符串
    double time4 = measure_time([&]() {
        // 确保调用接受dict的format函数
        mc::format(no_placeholders, large_args);
    });
    std::cout << "无占位符格式: " << time4 << " ms/次" << std::endl;

    // 场景5：空格式字符串
    double time5 = measure_time([&]() {
        // 确保调用接受dict的format函数
        mc::format("", large_args);
    });
    std::cout << "空格式字符串: " << time5 << " ms/次" << std::endl;

    // 确保测试通过
    ASSERT_TRUE(true);
}

} // namespace test
} // namespace mc