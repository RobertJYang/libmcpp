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

#include <mc/futures/exceptions.h>

namespace mc::futures {
MC_IMPLEMENT_EXCEPTION_CLASS(future_already_retrieved, future_already_retrieved_code, "Future 已被获取",
                             "future_already_retrieved")
MC_IMPLEMENT_EXCEPTION_CLASS(promise_already_satisfied, promise_already_set_code, "Promise 值已被设置",
                             "promise_already_satisfied")
MC_IMPLEMENT_EXCEPTION_CLASS(invalid_future_exception, invalid_future_code, "Future 无效", "invalid_future_exception")
} // namespace mc::futures
