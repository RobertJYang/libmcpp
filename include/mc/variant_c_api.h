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
 * @file variant_c_api.h
 * @brief C API 接口，用于动态加载时构造和操作 mc::variant
 */
#ifndef MC_VARIANT_C_API_H
#define MC_VARIANT_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// 前向声明
typedef struct mc_variant mc_variant_t;
typedef struct mc_context mc_context_t;
typedef struct mc_engine mc_engine_t;

/**
 * @brief 从 int64_t 构造 variant
 * @param value int64_t 值
 * @return variant 指针，失败返回 NULL
 */
mc_variant_t* mc_variant_from_int64(int64_t value);

/**
 * @brief 从 double 构造 variant
 * @param value double 值
 * @return variant 指针，失败返回 NULL
 */
mc_variant_t* mc_variant_from_double(double value);

/**
 * @brief 从 bool 构造 variant
 * @param value bool 值
 * @return variant 指针，失败返回 NULL
 */
mc_variant_t* mc_variant_from_bool(bool value);

/**
 * @brief 从字符串构造 variant
 * @param value 字符串值
 * @return variant 指针，失败返回 NULL
 */
mc_variant_t* mc_variant_from_string(const char* value);

/**
 * @brief 释放 variant
 * @param variant variant 指针
 */
void mc_variant_delete(mc_variant_t* variant);

/**
 * @brief 创建表达式引擎
 * @return 引擎指针，失败返回 NULL
 */
mc_engine_t* mc_engine_new(void);

/**
 * @brief 删除表达式引擎
 * @param engine 引擎指针
 */
void mc_engine_delete(mc_engine_t* engine);

/**
 * @brief 创建表达式上下文
 * @param engine 引擎指针
 * @return 上下文指针，失败返回 NULL
 */
mc_context_t* mc_context_new(mc_engine_t* engine);

/**
 * @brief 删除表达式上下文
 * @param context 上下文指针
 */
void mc_context_delete(mc_context_t* context);

/**
 * @brief 在上下文中注册变量
 * @param context 上下文指针
 * @param name 变量名
 * @param variant variant 指针
 * @return 0 成功，-1 失败
 */
int mc_context_register_variable(mc_context_t* context, const char* name, const mc_variant_t* variant);

/**
 * @brief 求值表达式并返回 GVariant
 * @param engine 引擎指针
 * @param expr 表达式字符串
 * @param context 上下文指针
 * @return GVariant 指针，失败返回 NULL
 */
void* mc_engine_evaluate_as_gvariant(mc_engine_t* engine, const char* expr, const mc_context_t* context);

#ifdef __cplusplus
}
#endif

#endif // MC_VARIANT_C_API_H 