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

#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/engine/error.h>
#include <mc/engine/error_engine.h>
#include <mc/exception.h>
#include <mc/variant.h>

using namespace mc;
using namespace mc::engine;

class ErrorEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
        mc::singleton<error_engine>::reset_for_test();
    }
};

// 测试错误注册和获取
TEST_F(ErrorEngineTest, RegisterAndGetError) {
    auto& engine = error_engine::get_instance();

    std::string name   = "test.error.1";
    std::string format = "测试错误：${message}";

    // 注册错误
    engine.register_error(name, format);

    // 检查是否注册成功
    EXPECT_TRUE(engine.is_registered(name));

    // 获取错误格式
    auto retrieved_format = engine.get_error_format(name);
    EXPECT_EQ(retrieved_format, format);

    // 测试未注册的错误
    EXPECT_FALSE(engine.is_registered("not.registered.error"));
    EXPECT_TRUE(engine.get_error_format("not.registered.error").empty());
}

// 测试格式参数提取
TEST_F(ErrorEngineTest, GetFormatArgs) {
    // 准备测试数据
    std::string_view  format = "错误信息：${code}，详细描述：${message}";
    mc::dict          args;
    mc::engine::error err;

    // 提取参数
    bool result = error_engine::get_format_args(format, args, err);

    // 验证结果
    EXPECT_TRUE(result);
    EXPECT_TRUE(args.contains("code"));
    EXPECT_TRUE(args.contains("message"));
    EXPECT_EQ(args.size(), 2);

    // 测试无参数的格式
    mc::dict args2;
    result = error_engine::get_format_args("没有参数的格式", args2, err);
    EXPECT_TRUE(result);
    EXPECT_TRUE(args2.empty());

    // 测试格式错误的情况
    mc::dict args3;
    result = error_engine::get_format_args("错误的格式：${name", args3, err);
    EXPECT_FALSE(result);
}

// 测试创建错误
TEST_F(ErrorEngineTest, MakeError) {
    // 创建错误信息
    std::string_view name   = "test.error.error2";
    std::string_view format = "错误代码：${code}，详细信息：${message}";

    // 创建错误
    auto error = error_engine::make_error(name, format)
                     .append_arg("code", 404)
                     .append_arg("message", "资源不存在");

    // 验证结果
    EXPECT_EQ(error.get_name(), name);
    EXPECT_EQ(error.get_format(), format);
    EXPECT_EQ(error.get_args().size(), 2);
    EXPECT_EQ(error.get_message(), "错误代码：404，详细信息：资源不存在");
}

// 测试错误信息结构体
TEST_F(ErrorEngineTest, ErrorInfo) {
    // 创建错误信息
    error_info info("test.error.3", "测试错误：${reason}");

    // 验证信息
    EXPECT_EQ(info.name, "test.error.3");
    EXPECT_EQ(info.format, "测试错误：${reason}");

    // 测试register_const_error方法
    auto& engine = error_engine::get_instance();
    engine.register_const_error(info);

    // 验证注册结果
    EXPECT_TRUE(engine.is_registered(info.name));
    EXPECT_EQ(engine.get_error_format(info.name), info.format);
}

// 测试复杂格式和参数
TEST_F(ErrorEngineTest, ComplexFormat) {
    std::string name   = "test.error.complex";
    std::string format = "用户${user}在${time}尝试访问${resource}，但权限不足。"
                         "所需权限：${required_permission}，实际权限：${actual_permission}";

    auto& engine = error_engine::get_instance();
    engine.register_error(name, format);

    mc::dict md = {
        {"user", "admin"},
        {"time", "2024-05-20 15:30:45"},
        {"resource", "/api/sensitive-data"},
        {"required_permission", "admin:write"},
        {"actual_permission", "user:read"},
    };

    auto error = error_engine::make_error(name, format).set_args(md);

    std::string expected = "用户admin在2024-05-20 15:30:45尝试访问/api/sensitive-data，但权限不足。"
                           "所需权限：admin:write，实际权限：user:read";
    EXPECT_EQ(error.get_message(), expected);
}

// 测试报告错误功能
TEST_F(ErrorEngineTest, ReportError) {
    auto& engine = error_engine::get_instance();

    // 注册一个错误
    std::string name   = "test.report.error";
    std::string format = "报告错误：${code}，消息：${message}";
    engine.register_error(name, format);

    // 使用名称报告错误
    error& err1 = engine.report_error(name);
    err1.append_arg("code", 500).append_arg("message", "服务器内部错误");

    // 验证错误内容
    EXPECT_EQ(err1.get_name(), name);
    EXPECT_EQ(err1.get_format(), format);
    EXPECT_EQ(err1.get_message(), "报告错误：500，消息：服务器内部错误");

    // 使用info结构体报告错误
    error_info info("test.report.info", "信息错误：${info}");
    engine.register_const_error(info);

    error& err2 = engine.report_error(info);
    err2.append_arg("info", "测试信息");

    // 验证错误内容
    EXPECT_EQ(err2.get_name(), info.name);
    EXPECT_EQ(err2.get_format(), info.format);
    EXPECT_EQ(err2.get_message(), "信息错误：测试信息");

    EXPECT_THROW(engine.report_error("not.registered.error"), mc::assert_exception);
}

// 测试last_error和error设置/重置功能
TEST_F(ErrorEngineTest, LastError) {
    auto& engine = error_engine::get_instance();

    // 重置错误
    engine.reset_error();

    // 验证重置后的错误状态
    EXPECT_FALSE(engine.last_error().is_set());

    // 创建并设置一个错误
    std::string name   = "test.last.error";
    std::string format = "最后错误：${info}";
    engine.register_error(name, format);

    error err = error_engine::make_error(name, format).append_arg("info", "这是最后的错误");

    // 设置为最后错误
    engine.set_last_error(err);

    // 验证最后错误
    auto& last = engine.last_error();
    EXPECT_TRUE(last.is_set());
    EXPECT_EQ(last.get_name(), name);
    EXPECT_EQ(last.get_message(), "最后错误：这是最后的错误");

    // 再次重置错误
    engine.reset_error();
    EXPECT_FALSE(engine.last_error().is_set());
}

// 测试错误名称验证功能
TEST_F(ErrorEngineTest, ValidateErrorName) {
    // 测试有效的错误名称
    EXPECT_TRUE(error_engine::is_valid_error_name("org.valid.error"));
    EXPECT_TRUE(error_engine::is_valid_error_name("com.example.error"));
    EXPECT_TRUE(error_engine::is_valid_error_name("x.y.z"));

    // 测试无效的错误名称
    EXPECT_FALSE(error_engine::is_valid_error_name(""));
    EXPECT_FALSE(error_engine::is_valid_error_name("invalid"));
    EXPECT_FALSE(error_engine::is_valid_error_name("invalid."));
    EXPECT_FALSE(error_engine::is_valid_error_name(".invalid"));
    EXPECT_FALSE(error_engine::is_valid_error_name("invalid..name"));
}