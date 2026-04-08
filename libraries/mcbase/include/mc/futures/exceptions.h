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

#ifndef MC_FUTURES_EXCEPTIONS_H
#define MC_FUTURES_EXCEPTIONS_H

#include <mc/exception.h>

namespace mc::futures {

// 定义 Future 相关的异常代码
enum future_exception_codes {
    future_already_retrieved_code = 5001, // Future 已被获取异常
    promise_already_set_code      = 5002, // Promise 值已被设置异常
    invalid_future_code           = 5003, // 无效的 Future 异常
};

MC_DECLARE_EXCEPTION_CLASS(future_already_retrieved, future_already_retrieved_code, "Future 已被获取",
                           "future_already_retrieved")
MC_DECLARE_EXCEPTION_CLASS(promise_already_satisfied, promise_already_set_code, "Promise 值已被设置",
                           "promise_already_satisfied")
MC_DECLARE_EXCEPTION_CLASS(invalid_future_exception, invalid_future_code, "Future 无效", "invalid_future_exception")

} // namespace mc::futures

#endif // MC_FUTURES_EXCEPTIONS_H