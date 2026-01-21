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

#include <cstdarg>
#include <string>

#include <mc/fmt/format_v.h>

namespace {

// 封装 format_vv(bool) 版本，便于在测试中构造 va_list
bool call_format_vv(std::string& out, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    bool ret = mc::fmt::format_vv(out, fmt, args);
    va_end(args);
    return ret;
}

std::string call_format_vv_to_string(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::string result = mc::fmt::format_vv(fmt, args);
    va_end(args);
    return result;
}

} // namespace

// 测试 format_v 返回 std::string 的功能
TEST(format_v_test, c_style_variadic_string) {
    std::string result = mc::fmt::format_v("%d-%s-%.1f", 42, "cpu", 3.5);
    EXPECT_EQ(result, "42-cpu-3.5");
}

// 测试 format_v 将结果写入外部字符串
TEST(format_v_test, c_style_variadic_to_buffer) {
    std::string result;
    bool        ok = mc::fmt::format_v(result, "%04d %s", 7, "ready");
    ASSERT_TRUE(ok);
    EXPECT_EQ(result, "0007 ready");
}

// 测试 format_vv 在结果缓冲区上运行成功路径
TEST(format_v_test, format_vv_string_and_buffer) {
    std::string buffer;
    ASSERT_TRUE(call_format_vv(buffer, "%s %d", "task", 8));
    EXPECT_EQ(buffer, "task 8");

    std::string direct = call_format_vv_to_string("%08X", 0x3AF);
    EXPECT_EQ(direct, "000003AF");
}

// 测试 format_vv 在空格式字符串情况下返回 false
TEST(format_v_test, format_vv_empty_format_returns_false) {
    std::string buffer = "original";
    EXPECT_FALSE(call_format_vv(buffer, ""));
    EXPECT_TRUE(buffer.empty());
}

