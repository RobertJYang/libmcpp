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
    MC_REFLECTABLE("mc.expr.builtin.conversion");

    static int64_t to_integer(const mc::variant& arg) {
        try {
            return arg.as_int64();
        } catch (const std::exception&) {
            MC_THROW(mc::invalid_arg_exception, "Expression evaluation error: unable to convert value to numeric type");
        }
    }

    static double to_double(const mc::variant& arg) {
        try {
            return arg.as_double();
        } catch (const std::exception&) {
            MC_THROW(mc::invalid_arg_exception, "Expression evaluation error: unable to convert value to floating point type");
        }
    }

    static std::string to_string(const mc::variant& arg) {
        return arg.as_string();
    }

    static bool to_bool(const mc::variant& arg) {
        try {
            return arg.as_bool(true);
        } catch (const std::exception&) {
            MC_THROW(mc::invalid_arg_exception, "Expression evaluation error: unable to convert value to boolean type");
        }
    }
};

} // namespace mc::expr

MC_REFLECT(mc::expr::conversion_funcs, (to_integer)(to_double)(to_string)(to_bool));

MC_REGISTER_BUILTIN_MODULE(conversion, mc::expr::conversion_funcs);
