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

/**
 * @file expr.h
 * @brief 定义了engine模块中的表达式支持
 */
#ifndef MC_ENGINE_EXPR_H
#define MC_ENGINE_EXPR_H

#include <functional>
#include <mc/dict.h>
#include <mc/expr.h>
#include <mc/variant.h>
#include <memory>
#include <string>
#include <string_view>

namespace mc::engine {

/**
 * @brief 获取全局表达式引擎实例
 */
mc::expr::engine& get_expr_engine();

} // namespace mc::engine

#endif // MC_ENGINE_EXPR_H