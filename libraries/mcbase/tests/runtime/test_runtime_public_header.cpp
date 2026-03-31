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

#include <mc/runtime.h>

TEST(runtime_public_header_test, runtime_header_exposes_public_io_wrappers)
{
    auto executor = mc::runtime::make_io_strand();
    mc::runtime::steady_timer timer(executor);

    EXPECT_TRUE(executor.valid());
    EXPECT_GE(timer.expiry(), mc::runtime::steady_timer::clock_type::time_point::min());
}
