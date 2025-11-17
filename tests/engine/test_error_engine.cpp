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
#include <mc/error_engine.h>
#include <mc/exception.h>
#include <mc/variant.h>

#include <string>

using namespace mc;
using namespace mc::engine;

class ErrorEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
        error_engine::reset_for_test();
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
    auto retrieved_format = engine.get_error_info(name).format;
    EXPECT_EQ(retrieved_format, format);

    // 测试未注册的错误
    EXPECT_FALSE(engine.is_registered("not.registered.error"));
    EXPECT_TRUE(engine.get_error_info("not.registered.error").format.empty());
}

TEST_F(ErrorEngineTest, RegisterConstErrorWithErrorInfo) {
    auto& engine = error_engine::get_instance();

    error_info info("test.error.info", "信息错误：${value}");

    auto stored = engine.register_const_error(info);
    EXPECT_EQ(stored.name, info.name);
    EXPECT_EQ(stored.format, info.format);

    auto fetched = engine.get_error_info(info.name);
    EXPECT_EQ(fetched.name, info.name);
    EXPECT_EQ(fetched.format, info.format);
}

TEST_F(ErrorEngineTest, RegisterConstErrorDuplicate) {
    auto& engine = error_engine::get_instance();

    error_info info("test.error.duplicate", "重复注册：${value}");

    auto first  = engine.register_const_error(info);
    auto second = engine.register_const_error(info);

    EXPECT_EQ(first.name, info.name);
    EXPECT_EQ(first.format, info.format);

    EXPECT_TRUE(second.name.empty());
    EXPECT_TRUE(second.format.empty());

    auto stored = engine.get_error_info(info.name);
    EXPECT_EQ(stored.name, info.name);
    EXPECT_EQ(stored.format, info.format);
}

TEST_F(ErrorEngineTest, RegisterErrorSuccess) {
    auto& engine = error_engine::get_instance();

    auto dynamic = engine.register_error("test.error.dynamic.success", "动态错误：${value}");
    EXPECT_EQ(dynamic.name, "test.error.dynamic.success");
    EXPECT_EQ(dynamic.format, "动态错误：${value}");

    EXPECT_TRUE(engine.is_registered(dynamic.name));
    auto retrieved = engine.get_error_info(dynamic.name);
    EXPECT_EQ(retrieved.name, dynamic.name);
    EXPECT_EQ(retrieved.format, dynamic.format);
}

TEST_F(ErrorEngineTest, RegisterErrorDuplicate) {
    auto& engine = error_engine::get_instance();

    auto first  = engine.register_error("test.error.dynamic", "动态错误：${value}");
    auto second = engine.register_error("test.error.dynamic", "动态错误重复：${value}");

    EXPECT_EQ(first.name, "test.error.dynamic");
    EXPECT_EQ(first.format, "动态错误：${value}");

    EXPECT_TRUE(second.name.empty());
    EXPECT_TRUE(second.format.empty());
}

// 测试格式参数提取
TEST_F(ErrorEngineTest, GetFormatArgs) {
    // 准备测试数据
    std::string_view format = "错误信息：${code}，详细描述：${message}";
    mc::dict         args;

    // 提取参数
    bool result = mc::engine::get_error_format_args(format, args);

    // 验证结果
    EXPECT_TRUE(result);
    EXPECT_TRUE(args.contains("code"));
    EXPECT_TRUE(args.contains("message"));
    EXPECT_EQ(args.size(), 2);

    // 测试无参数的格式
    mc::dict args2;
    result = mc::engine::get_error_format_args("没有参数的格式", args2);
    EXPECT_TRUE(result);
    EXPECT_TRUE(args2.empty());

    // 测试格式错误的情况
    mc::dict args3;
    result = mc::engine::get_error_format_args("错误的格式：${name", args3);
    EXPECT_FALSE(result);
}

// 测试创建错误
TEST_F(ErrorEngineTest, MakeError) {
    // 创建错误信息
    std::string_view name   = "test.error.error2";
    std::string_view format = "错误代码：${code}，详细信息：${message}";

    // 创建错误
    auto error = mc::engine::make_error(name, format);
    error->append_arg("code", 404);
    error->append_arg("message", "资源不存在");

    // 验证结果
    EXPECT_EQ(error->get_name(), name);
    EXPECT_EQ(error->get_format(), format);
    EXPECT_EQ(error->get_args().size(), 2);
    EXPECT_EQ(error->get_message(), "错误代码：404，详细信息：资源不存在");
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
    EXPECT_EQ(engine.get_error_info(info.name), info);
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

    auto error = mc::engine::make_error(name, format);
    error->set_args(md);

    std::string expected = "用户admin在2024-05-20 15:30:45尝试访问/api/sensitive-data，但权限不足。"
                           "所需权限：admin:write，实际权限：user:read";
    EXPECT_EQ(error->get_message(), expected);
}

// 测试报告错误功能
TEST_F(ErrorEngineTest, ReportError) {
    auto& engine = error_engine::get_instance();

    // 注册一个错误
    std::string name   = "test.report.error";
    std::string format = "报告错误：${code}，消息：${message}";
    engine.register_error(name, format);

    // 使用名称报告错误
    auto err1 = engine.report_error(name, {{"code", 500}, {"message", "服务器内部错误"}});

    // 验证错误内容
    EXPECT_EQ(err1->get_name(), name);
    EXPECT_EQ(err1->get_format(), format);
    EXPECT_EQ(err1->get_message(), "报告错误：500，消息：服务器内部错误");

    // 使用info结构体报告错误
    error_info info("test.report.info", "信息错误：${info}");
    engine.register_const_error(info);

    auto err2 = engine.report_error(info, {{"info", "测试信息"}});

    // 验证错误内容
    EXPECT_EQ(err2->get_name(), info.name);
    EXPECT_EQ(err2->get_format(), info.format);
    EXPECT_EQ(err2->get_message(), "信息错误：测试信息");

    EXPECT_THROW(engine.report_error("not.registered.error"), mc::assert_exception);
}

TEST_F(ErrorEngineTest, ReportErrorReusesUnsetLastError) {
    auto& engine = error_engine::get_instance();
    engine.reset_error();

    auto empty_last = mc::make_shared<mc::error>();
    engine.set_last_error(empty_last);

    error_info info("test.reuse.error", "重用错误：${value}");
    engine.register_const_error(info);

    auto reused = engine.report_error(info, {{"value", "first"}});
    EXPECT_EQ(reused, empty_last);
    EXPECT_TRUE(reused->is_set());
    EXPECT_EQ(reused->get_message(), "重用错误：first");

    auto chained = engine.report_error(info, {{"value", "second"}});
    EXPECT_NE(chained, empty_last);
    EXPECT_TRUE(chained->has_error("test.reuse.error"));
    EXPECT_EQ(chained->get_message(), "重用错误：second");
}

TEST_F(ErrorEngineTest, ReportErrorByName) {
    auto& engine = error_engine::get_instance();
    engine.register_error("test.error.by_name", "名称报告：${value}", mc::error_level::warn);

    auto reported = engine.report_error("test.error.by_name", {{"value", "content"}});
    ASSERT_TRUE(reported);
    EXPECT_EQ(reported->get_message(), "名称报告：content");
    EXPECT_EQ(reported->get_level(), mc::error_level::warn);
}

TEST_F(ErrorEngineTest, SetLastErrorReturnsPrevious) {
    auto& engine = error_engine::get_instance();

    auto first  = mc::make_error("test.error.first", "first");
    auto second = mc::make_error("test.error.second", "second");

    auto prev = engine.set_last_error(first);
    EXPECT_FALSE(prev);
    EXPECT_EQ(engine.last_error(), first);

    prev = engine.set_last_error(second);
    EXPECT_EQ(prev, first);
    EXPECT_EQ(engine.last_error(), second);
}

// 测试last_error和error设置/重置功能
TEST_F(ErrorEngineTest, LastError) {
    auto& engine = error_engine::get_instance();

    // 重置错误
    engine.reset_error();

    // 验证重置后的错误状态
    EXPECT_TRUE(!engine.last_error());

    // 创建并设置一个错误
    std::string name   = "test.last.error";
    std::string format = "最后错误：${info}";
    engine.register_error(name, format);

    auto err = mc::engine::make_error(name, format);
    err->append_arg("info", "这是最后的错误");

    // 设置为最后错误
    engine.set_last_error(err);

    // 验证最后错误
    auto last = engine.last_error();
    EXPECT_TRUE(last && last->is_set());
    EXPECT_EQ(last->get_name(), name);
    EXPECT_EQ(last->get_message(), "最后错误：这是最后的错误");

    // 再次重置错误
    engine.reset_error();
    EXPECT_TRUE(!engine.last_error());
}

// 测试错误名称验证功能
TEST_F(ErrorEngineTest, ValidateErrorName) {
    // 测试有效的错误名称
    EXPECT_TRUE(mc::engine::is_valid_error_name("org.valid.error"));
    EXPECT_TRUE(mc::engine::is_valid_error_name("com.example.error"));
    EXPECT_TRUE(mc::engine::is_valid_error_name("x.y.z"));

    // 测试无效的错误名称
    EXPECT_FALSE(mc::engine::is_valid_error_name(""));
    EXPECT_FALSE(mc::engine::is_valid_error_name("invalid"));
    EXPECT_FALSE(mc::engine::is_valid_error_name("invalid."));
    EXPECT_FALSE(mc::engine::is_valid_error_name(".invalid"));
    EXPECT_FALSE(mc::engine::is_valid_error_name("invalid..name"));
}

// 复杂场景：错误链的完整生命周期测试
TEST_F(ErrorEngineTest, ComplexErrorChainLifecycle) {
    auto& engine = error_engine::get_instance();

    // 注册多个相关错误
    engine.register_error("test.chain.level1", "第一层错误: ${msg1}");
    engine.register_error("test.chain.level2", "第二层错误: ${msg2}");
    engine.register_error("test.chain.level3", "第三层错误: ${msg3}");
    engine.register_error("test.chain.level4", "第四层错误: ${msg4}");

    // 创建多层错误链
    auto err1 = engine.report_error("test.chain.level1", {{"msg1", "level1 message"}});
    EXPECT_TRUE(err1->is_set());
    EXPECT_EQ(err1->get_name(), "test.chain.level1");

    // 继续报告错误，形成错误链
    auto err2 = engine.report_error("test.chain.level2", {{"msg2", "level2 message"}});
    EXPECT_TRUE(err2->is_set());
    EXPECT_EQ(err2->get_name(), "test.chain.level2");
    EXPECT_TRUE(err2->has_error("test.chain.level1")); // 应该包含前一个错误

    auto err3 = engine.report_error("test.chain.level3", {{"msg3", "level3 message"}});
    EXPECT_TRUE(err3->is_set());
    EXPECT_TRUE(err3->has_error("test.chain.level2"));
    EXPECT_TRUE(err3->has_error("test.chain.level1"));

    auto err4 = engine.report_error("test.chain.level4", {{"msg4", "level4 message"}});
    EXPECT_TRUE(err4->is_set());
    EXPECT_TRUE(err4->has_error("test.chain.level3"));
    EXPECT_TRUE(err4->has_error("test.chain.level2"));
    EXPECT_TRUE(err4->has_error("test.chain.level1"));

    // 验证错误链的完整性
    auto last = engine.last_error();
    EXPECT_EQ(last, err4);
    EXPECT_EQ(last->get_message(), "第四层错误: level4 message");

    // 重置错误，验证错误链被清除
    engine.reset_error();
    EXPECT_FALSE(engine.last_error());

    // 重新报告错误，验证错误链重新开始
    auto new_err = engine.report_error("test.chain.level1", {{"msg1", "new level1"}});
    EXPECT_TRUE(new_err->is_set());
    EXPECT_FALSE(new_err->has_error("test.chain.level2")); // 新错误链不包含旧错误
}

// 复杂场景：错误报告与错误设置的交互
TEST_F(ErrorEngineTest, ComplexErrorReportAndSetInteraction) {
    auto& engine = error_engine::get_instance();

    engine.register_error("test.interaction.report", "报告错误: ${code}");
    engine.register_error("test.interaction.set", "设置错误: ${info}");

    // 先报告一个错误
    auto reported = engine.report_error("test.interaction.report", {{"code", 500}});
    EXPECT_EQ(engine.last_error(), reported);

    // 手动设置一个新错误
    auto manual = mc::make_error("test.interaction.set", "设置错误: ${info}");
    manual->append_arg("info", "manual error");
    auto prev = engine.set_last_error(manual);

    // 验证设置返回了之前的报告错误
    EXPECT_EQ(prev, reported);
    EXPECT_EQ(engine.last_error(), manual);

    // 再次报告错误，应该形成新的错误链
    auto reported2 = engine.report_error("test.interaction.report", {{"code", 404}});
    EXPECT_EQ(reported2->get_name(), "test.interaction.report");
    EXPECT_TRUE(reported2->has_error("test.interaction.set")); // 应该包含手动设置的错误
}

// 复杂场景：错误级别和格式化
TEST_F(ErrorEngineTest, ComplexErrorLevelAndFormatting) {
    auto& engine = error_engine::get_instance();

    // 注册不同级别的错误
    engine.register_error("test.level.debug", "调试: ${info}", mc::error_level::debug);
    engine.register_error("test.level.info", "信息: ${info}", mc::error_level::info);
    engine.register_error("test.level.warn", "警告: ${info}", mc::error_level::warn);
    engine.register_error("test.level.error", "错误: ${info}", mc::error_level::error);

    // 报告不同级别的错误
    auto debug_err = engine.report_error("test.level.debug", {{"info", "debug message"}});
    EXPECT_EQ(debug_err->get_level(), mc::error_level::debug);

    auto info_err = engine.report_error("test.level.info", {{"info", "info message"}});
    EXPECT_EQ(info_err->get_level(), mc::error_level::info);
    EXPECT_TRUE(info_err->has_error("test.level.debug"));

    auto warn_err = engine.report_error("test.level.warn", {{"info", "warn message"}});
    EXPECT_EQ(warn_err->get_level(), mc::error_level::warn);

    auto error_err = engine.report_error("test.level.error", {{"info", "error message"}});
    EXPECT_EQ(error_err->get_level(), mc::error_level::error);

    // 验证错误链包含所有级别的错误
    EXPECT_TRUE(error_err->has_error("test.level.warn"));
    EXPECT_TRUE(error_err->has_error("test.level.info"));
    EXPECT_TRUE(error_err->has_error("test.level.debug"));
}