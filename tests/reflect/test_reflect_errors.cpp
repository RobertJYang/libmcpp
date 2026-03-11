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

#include <mc/exception.h>
#include <mc/reflect/reflect.h>

namespace {

TEST(reflect_error_test, throw_bad_enum_cast_with_integer)
{
    EXPECT_THROW(mc::reflect::throw_bad_enum_cast(7, "MockEnum"), mc::bad_cast_exception);
}

TEST(reflect_error_test, throw_bad_enum_cast_with_string)
{
    EXPECT_THROW(mc::reflect::throw_bad_enum_cast("unknown", "MockEnum"), mc::bad_cast_exception);
}

TEST(reflect_error_test, throw_not_enum_type)
{
    EXPECT_THROW(mc::reflect::throw_not_enum_type("mc::Sample"), mc::bad_type_exception);
}

TEST(reflect_error_test, throw_enum_value_not_found_by_name)
{
    EXPECT_THROW(mc::reflect::throw_enum_value_not_found("MockEnum", "missing"), mc::bad_type_exception);
}

TEST(reflect_error_test, throw_enum_value_not_found_by_value)
{
    EXPECT_THROW(mc::reflect::throw_enum_value_not_found("MockEnum", 123U), mc::bad_type_exception);
}

TEST(reflect_error_test, throw_enum_not_support_create_object)
{
    EXPECT_THROW(mc::reflect::throw_enum_not_support_create_object("MockEnum"), mc::bad_type_exception);
}

} // namespace
