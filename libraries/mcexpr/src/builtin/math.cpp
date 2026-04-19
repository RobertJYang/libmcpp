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

#include <cmath>
#include <mc/exception.h>
#include <mc/expr/builtin.h>
#include <mc/reflect.h>

namespace mc::expr {

namespace {
void ensure_numeric(std::string_view name, const mc::variant& value)
{
    MC_ASSERT_THROW(value.is_numeric(), mc::invalid_arg_exception, "${name} 参数必须是数值类型", ("name", name));
}
} // namespace

struct math_funcs {
    MC_REFLECTABLE("mc.expr.builtin.math");

    static mc::variant abs(const mc::variant& value)
    {
        ensure_numeric("abs", value);
        if (value.is_double()) {
            return std::fabs(value.as_double());
        } else {
            return std::abs(value.as_int64());
        }
    }

    static mc::variant min(const mc::variants& args)
    {
        if (args.empty()) {
            MC_THROW(mc::invalid_arg_exception, "min function requires at least one parameter");
        }

        mc::variant result = args[0];
        ensure_numeric("min", result);
        for (size_t i = 1; i < args.size(); ++i) {
            ensure_numeric("min", args[i]);
            if (args[i] < result) {
                result = args[i];
            }
        }
        return result;
    }

    static mc::variant max(const mc::variants& args)
    {
        if (args.empty()) {
            MC_THROW(mc::invalid_arg_exception, "max function requires at least one parameter");
        }

        mc::variant result = args[0];
        ensure_numeric("max", result);
        for (size_t i = 1; i < args.size(); ++i) {
            ensure_numeric("max", args[i]);
            if (args[i] > result) {
                result = args[i];
            }
        }
        return result;
    }

    static double round(double value)
    {
        return std::round(value);
    }

    static double floor(double value)
    {
        return std::floor(value);
    }

    static double ceil(double value)
    {
        return std::ceil(value);
    }

    static double sqrt(double value)
    {
        if (value < 0) {
            MC_THROW(mc::invalid_arg_exception, "sqrt function parameter must be greater than or equal to 0");
        }
        return std::sqrt(value);
    }

    static double pow(double base, double exponent)
    {
        if (base < 0) {
            MC_THROW(mc::invalid_arg_exception, "pow function base must be greater than or equal to 0");
        }
        return std::pow(base, exponent);
    }

    static double log(double value)
    {
        if (value <= 0) {
            MC_THROW(mc::invalid_arg_exception, "log function parameter must be greater than 0");
        }
        return std::log(value);
    }

    static double exp(double value)
    {
        return std::exp(value);
    }
};

} // namespace mc::expr

MC_REFLECT(mc::expr::math_funcs, (abs)(min)(max)(round)(floor)(ceil)(sqrt)(pow)(log)(exp));

MC_REGISTER_BUILTIN_MODULE(math, mc::expr::math_funcs);
