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

#ifndef MC_RUNTIME_DETAIL_TASK_H
#define MC_RUNTIME_DETAIL_TASK_H

#include <mc/runtime/detail/inplace_function.h>

namespace mc::runtime::detail {

/// @brief 固定大小的 move-only 无参可调用对象，用于 executor 任务投递。
using task = inplace_function<void(), 48>;

} // namespace mc::runtime::detail

#endif // MC_RUNTIME_DETAIL_TASK_H
