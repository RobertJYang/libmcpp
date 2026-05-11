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

#ifndef MC_RUNTIME_DETAIL_COMPLETION_HANDLER_H
#define MC_RUNTIME_DETAIL_COMPLETION_HANDLER_H

#include <mc/small_function.h>

namespace mc::runtime::detail {

template <typename Signature>
using completion_handler = mc::small_function<Signature, 64>;

} // namespace mc::runtime::detail

#endif // MC_RUNTIME_DETAIL_COMPLETION_HANDLER_H
