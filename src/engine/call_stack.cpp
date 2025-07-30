/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/engine/call_stack.h>
#include <mc/engine/context.h>
#include <mc/engine/object.h>
#include <mc/engine/service.h>

// 实例化常用调用栈类型
MC_ENGINE_CALL_STACK_IMPL(mc::engine::service, mc::engine::abstract_object)
MC_ENGINE_CALL_STACK_IMPL(mc::engine::service, mc::engine::context)
