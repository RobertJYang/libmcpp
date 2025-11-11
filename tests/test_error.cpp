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

#include <mc/error.h>
#include <mc/exception.h>

#include <string>

namespace {

TEST(ErrorClassTest, CopyAndEquality) {
    auto base_error = mc::make_error("test.error.base", "基础错误: ${info}");
    base_error->append_arg("info", "base");

    auto nested_error = mc::make_error("test.error.nested", "嵌套错误: ${detail}");
    nested_error->append_arg("detail", "detail");
    base_error->set_prev_error(nested_error);

    mc::error copy(*base_error);
    EXPECT_TRUE(copy.is_set());
    EXPECT_EQ(copy.get_name(), "test.error.base");
    EXPECT_TRUE(copy.has_error("test.error.nested"));

    // 深拷贝：prev_error 不共享同一指针
    EXPECT_EQ(copy, *base_error);

    // 修改原始错误的链条，确保不会影响拷贝
    base_error->set_prev_error(mc::make_error("test.error.replaced", "replaced"));
    EXPECT_TRUE(copy.has_error("test.error.nested"));
    EXPECT_FALSE(base_error->has_error("test.error.nested"));

    copy.reset();
    EXPECT_FALSE(copy.is_set());
    EXPECT_NE(copy, *base_error);
}

TEST(ErrorClassTest, ToExceptionAndFromException) {
    auto err = mc::make_error("test.error.to_exception", "异常信息: ${msg}");
    err->append_arg("msg", "to_exception");

    mc::exception ex(mc::exception_code::unknow_exception_code, "", "");
    err->to_exception(ex);
    EXPECT_EQ(ex.name(), "test.error.to_exception");
    EXPECT_NE(std::string(ex.to_string()).find("to_exception"), std::string::npos);

    try {
        MC_THROW(mc::invalid_arg_exception, "测试异常: ${value}", ("value", "from_exception"));
    } catch (const mc::exception& caught) {
        auto converted = mc::error::from_exception(caught);
        ASSERT_TRUE(converted);
        EXPECT_EQ(converted->get_name(), caught.name());
        EXPECT_TRUE(converted->is_set());
        EXPECT_NE(converted->get_message().find("from_exception"), std::string::npos);
    }
}

TEST(ErrorClassTest, HasErrorAndReset) {
    auto primary = mc::make_error("test.primary", "Primary");
    auto nested  = mc::make_error("test.nested", "Nested");
    primary->set_prev_error(nested);

    EXPECT_TRUE(primary->has_error("test.primary"));
    EXPECT_TRUE(primary->has_error("test.nested"));
    EXPECT_FALSE(primary->has_error("test.unknown"));

    primary->reset();
    EXPECT_FALSE(primary->is_set());
    EXPECT_FALSE(primary->has_error("test.primary"));
}

// 复杂场景：多层错误链的深拷贝和操作
TEST(ErrorClassTest, ComplexErrorChainDeepCopy) {
    // 创建多层错误链
    auto level1 = mc::make_error("test.chain.level1", "第一层: ${msg1}");
    level1->append_arg("msg1", "level1");

    auto level2 = mc::make_error("test.chain.level2", "第二层: ${msg2}");
    level2->append_arg("msg2", "level2");
    level2->set_prev_error(level1);

    auto level3 = mc::make_error("test.chain.level3", "第三层: ${msg3}");
    level3->append_arg("msg3", "level3");
    level3->set_prev_error(level2);

    // 深拷贝错误链
    mc::error copy(*level3);

    // 验证拷贝的完整性
    EXPECT_EQ(copy.get_name(), "test.chain.level3");
    EXPECT_TRUE(copy.has_error("test.chain.level2"));
    EXPECT_TRUE(copy.has_error("test.chain.level1"));
    EXPECT_EQ(copy.get_message(), "第三层: level3");

    // 修改原始错误链，验证不影响拷贝
    auto new_level = mc::make_error("test.chain.new", "新错误");
    level3->set_prev_error(new_level);

    // 拷贝应该仍然包含旧的错误链
    EXPECT_TRUE(copy.has_error("test.chain.level2"));
    EXPECT_TRUE(copy.has_error("test.chain.level1"));
    EXPECT_FALSE(copy.has_error("test.chain.new"));

    // 原始错误链应该包含新错误
    EXPECT_TRUE(level3->has_error("test.chain.new"));
    EXPECT_FALSE(level3->has_error("test.chain.level2")); // 被替换了
}

// 复杂场景：错误链的查询和操作
TEST(ErrorClassTest, ComplexErrorChainQueryAndOperation) {
    // 创建复杂的错误链
    auto err1 = mc::make_error("test.query.err1", "错误1");
    auto err2 = mc::make_error("test.query.err2", "错误2");
    auto err3 = mc::make_error("test.query.err3", "错误3");
    auto err4 = mc::make_error("test.query.err4", "错误4");

    err1->set_prev_error(err2);
    err2->set_prev_error(err3);
    err3->set_prev_error(err4);

    // 测试错误链查询
    EXPECT_TRUE(err1->has_error("test.query.err1"));
    EXPECT_TRUE(err1->has_error("test.query.err2"));
    EXPECT_TRUE(err1->has_error("test.query.err3"));
    EXPECT_TRUE(err1->has_error("test.query.err4"));
    EXPECT_FALSE(err1->has_error("test.query.err5"));

    // 测试 is_set
    EXPECT_TRUE(err1->is_set());
    EXPECT_TRUE(err2->is_set());
    EXPECT_TRUE(err3->is_set());
    EXPECT_TRUE(err4->is_set());

    // 重置中间错误
    err2->reset();
    EXPECT_FALSE(err2->is_set());
    EXPECT_FALSE(err2->has_error("test.query.err2"));

    // 但错误链的其他部分应该仍然有效
    EXPECT_TRUE(err1->is_set());
    EXPECT_TRUE(err3->is_set());
    EXPECT_TRUE(err4->is_set());
}

// 复杂场景：错误的赋值和移动
TEST(ErrorClassTest, ComplexErrorAssignmentAndMove) {
    auto err1 = mc::make_error("test.assign.err1", "错误1: ${msg}");
    err1->append_arg("msg", "message1");

    auto err2 = mc::make_error("test.assign.err2", "错误2: ${msg}");
    err2->append_arg("msg", "message2");
    err2->set_prev_error(err1);

    // 拷贝赋值
    auto err3 = mc::make_error("test.assign.err3", "错误3");
    *err3 = *err2;

    EXPECT_EQ(err3->get_name(), "test.assign.err2");
    EXPECT_TRUE(err3->has_error("test.assign.err1"));
    EXPECT_EQ(err3->get_message(), "错误2: message2");

    // 修改原始错误的名称，验证不影响拷贝（args 是共享的，但名称和格式是拷贝的）
    err2->set_name("test.assign.modified");
    EXPECT_EQ(err3->get_name(), "test.assign.err2"); // 名称不受影响
    EXPECT_EQ(err2->get_name(), "test.assign.modified");
}

} // namespace


