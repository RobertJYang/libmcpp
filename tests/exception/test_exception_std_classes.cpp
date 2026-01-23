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

#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/reflect.h>

#include <string_view>

namespace {

template <typename ExceptionT>
void verify_exception(ExceptionT&& ex, int64_t expected_code, std::string_view expected_name,
                      std::string_view expected_msg) {
    EXPECT_EQ(ex.code(), expected_code);
    EXPECT_EQ(ex.name(), expected_name);
    EXPECT_NE(ex.to_string().find(expected_msg), std::string::npos);

    auto copy = ex.dynamic_copy_exception();
    ASSERT_TRUE(copy);

    EXPECT_THROW(copy->dynamic_rethrow_exception(), ExceptionT);
}

} // namespace

#define GEN_STD_EXCEPTION_TEST(TYPE, CODE, DEFAULT_MSG, DEFAULT_NAME)                        \
    TEST(ExceptionStdClassCoverage, TYPE##ConstructAndThrow) {                               \
        auto ex = MC_MAKE_EXCEPTION(mc::TYPE, DEFAULT_MSG);                                  \
        verify_exception(std::move(ex), mc::CODE, DEFAULT_NAME, DEFAULT_MSG);                \
    }

MC_STD_EXCEPTION_CLASS(GEN_STD_EXCEPTION_TEST)
#undef GEN_STD_EXCEPTION_TEST

TEST(ExceptionStdClassCoverage, ErrorInfoReflectionMetadata) {
    const auto* name_prop = mc::reflect::get_property_info<mc::error_info>("name");
    ASSERT_NE(name_prop, nullptr);

    const auto* format_prop = mc::reflect::get_property_info<mc::error_info>("format");
    ASSERT_NE(format_prop, nullptr);

    mc::error_info info("test.error.reflect", "测试反射");
    EXPECT_EQ(name_prop->get_value(info).as<std::string_view>(), "test.error.reflect");
    EXPECT_EQ(format_prop->get_value(info).as<std::string_view>(), "测试反射");
}

TEST(ExceptionStdClassCoverage, ErrorReflectionMetadata) {
    const auto* name_prop = mc::reflect::get_property_info<mc::error>("name");
    const auto* format_prop = mc::reflect::get_property_info<mc::error>("format");
    const auto* args_prop = mc::reflect::get_property_info<mc::error>("args");

    ASSERT_NE(name_prop, nullptr);
    ASSERT_NE(format_prop, nullptr);
    ASSERT_NE(args_prop, nullptr);

    auto error_ptr = mc::make_error("org.test.runtime", "Runtime failure: ${code}");
    error_ptr->append_arg("code", 500);

    EXPECT_EQ(name_prop->get_value(*error_ptr).as<std::string_view>(), "org.test.runtime");
    EXPECT_NE(format_prop->get_value(*error_ptr).as<std::string_view>().find("Runtime failure"),
              std::string::npos);
    EXPECT_TRUE(args_prop->get_value(*error_ptr).as<mc::dict>().contains("code"));
}

