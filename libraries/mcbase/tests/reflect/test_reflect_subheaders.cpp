/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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

#include <mc/reflect/method.h>
#include <mc/reflect/property.h>

#include "reflect_subheader_fixture.h"

namespace {

TEST(ReflectSubheaderTest, MethodAndPropertyHeadersWorkWithoutReflectFacade)
{
    test_reflect_subheaders::demo_type obj;

    EXPECT_EQ(mc::reflect::get_property(obj, "m_value"), 7);
    EXPECT_EQ(mc::reflect::invoke(obj, "get_value"), 7);
    EXPECT_EQ(mc::reflect::invoke(obj, "add", {5}), 12);
}

} // namespace
