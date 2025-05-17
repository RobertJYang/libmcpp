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
 * @file error.h
 * @brief 定义了表达式库的错误类型
 */
#ifndef MC_EXPR_ERROR_H
#define MC_EXPR_ERROR_H

#include <mc/exception.h>

namespace mc::expr {

/**
 * @brief 表达式模块异常编码
 */
enum expr_exception_code {
    expr_error_code       = 1000, // 表达式通用错误
    expr_parse_error_code = 1001, // 表达式解析错误
    expr_eval_error_code  = 1002, // 表达式求值错误
    expr_type_error_code  = 1003, // 表达式类型错误
};

/**
 * @brief 表达式错误基类
 */
MC_DEFINE_EXCEPTION_CLASS(error, expr_error_code, "表达式错误", "expr_error")

/**
 * @brief 表达式解析错误
 */
MC_DEFINE_EXCEPTION_CLASS(parse_error, expr_parse_error_code, "表达式解析错误", "expr_parse_error")

/**
 * @brief 表达式求值错误
 */
MC_DEFINE_EXCEPTION_CLASS(eval_error, expr_eval_error_code, "表达式求值错误", "expr_eval_error")

/**
 * @brief 表达式类型错误
 */
MC_DEFINE_EXCEPTION_CLASS(type_error, expr_type_error_code, "表达式类型错误", "expr_type_error")

} // namespace mc::expr

#endif // MC_EXPR_ERROR_H