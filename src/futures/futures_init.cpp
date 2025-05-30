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

// 静态初始化器，自动注册 Future 相关的异常类型
struct futures_exception_registrar {
    futures_exception_registrar() {
        MC_REGISTER_EXCEPTION(timeout_error);
        MC_REGISTER_EXCEPTION(operation_cancelled_error);
        MC_REGISTER_EXCEPTION(future_already_retrieved);
        MC_REGISTER_EXCEPTION(promise_already_satisfied);
        MC_REGISTER_EXCEPTION(future_exception);
    }
};

// 静态实例，确保在程序启动时注册异常
static futures_exception_registrar registrar;

} // namespace mc::futures