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

#include <mc/future.h>

static_assert(mc::futures::detail::is_executor_v<mc::runtime::any_executor>,
              "any_executor 应继续被 futures 识别为 executor");
static_assert(mc::futures::detail::is_executor_v<mc::runtime::thread_pool::executor_type>,
              "thread_pool::executor_type 应继续被 futures 识别为 executor");
static_assert(!mc::futures::detail::is_executor_v<mc::runtime::thread_pool>,
              "thread_pool 本身应被识别为 execution_context，而不是 executor");
static_assert(!mc::futures::detail::is_executor_v<mc::futures::Promise<int>>, "Promise 不应被误判为 executor");
static_assert(mc::futures::detail::is_execution_context_v<mc::runtime::thread_pool>,
              "thread_pool 应继续被 futures 识别为 execution_context");

TEST(future_public_header_test, future_header_uses_any_executor_based_traits)
{
    auto promise = mc::make_promise<int>();

    EXPECT_TRUE(static_cast<bool>(promise));
}
