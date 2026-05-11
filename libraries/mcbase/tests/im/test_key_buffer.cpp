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

#include <mc/im/key_buffer.h>

#include <memory>
#include <string>
#include <type_traits>

namespace {

static_assert(!std::is_same_v<mc::im::key_buffer<>, std::string>);
static_assert(std::is_convertible_v<const mc::im::key_buffer<>&, mc::im::key_view>);

} // namespace

TEST(key_buffer_test, supports_owned_key_type_and_view_conversion)
{
    mc::im::key_buffer<> buffer = mc::im::to_key_buffer("alpha");

    mc::im::key_view view = buffer;
    EXPECT_EQ(view, "alpha");

    std::string std_buffer = buffer;
    EXPECT_EQ(std_buffer, "alpha");
}

TEST(key_buffer_test, allocator_first_constructors_preserve_content)
{
    std::allocator<char> alloc;

    mc::im::key_buffer<> from_view(alloc, mc::string_view("beta"));
    EXPECT_EQ(from_view, "beta");

    mc::im::key_buffer<> moved(alloc, mc::im::key_buffer<>("gamma"));
    EXPECT_EQ(moved, "gamma");
}
