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

#include <mc/exception.h>
#include <mc/expr/builtin.h>
#include <mc/reflect.h>

namespace mc::expr {

struct conversion_funcs {
    static int64_t to_integer(const mc::variant& arg) {
        try {
            return arg.as_int64();
        } catch (const std::exception&) {
            MC_THROW(mc::invalid_arg_exception, "表达式求值错误: 无法将值转换为数值类型");
        }
    }

    static double to_double(const mc::variant& arg) {
        try {
            return arg.as_double();
        } catch (const std::exception&) {
            MC_THROW(mc::invalid_arg_exception, "表达式求值错误: 无法将值转换为浮点数类型");
        }
    }

    static std::string to_string(const mc::variant& arg) {
        return arg.as_string();
    }

    static bool to_bool(const mc::variant& arg) {
        try {
            return arg.as_bool();
        } catch (const std::exception&) {
            MC_THROW(mc::invalid_arg_exception, "表达式求值错误: 无法将值转换为布尔类型");
        }
    }
};

} // namespace mc::expr

MC_REFLECT(mc::expr::conversion_funcs, (to_integer)(to_double)(to_string)(to_bool));

MC_REGISTER_BUILTIN_MODULE(conversion, mc::expr::conversion_funcs);
