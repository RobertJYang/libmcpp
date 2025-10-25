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
struct string_module {
    MC_REFLECTABLE("mc.expr.builtin.string");

    // 由于 mc::variant
    // 所有类型都可以自动转换成字符串类型，为了避免这种自动转换，我们需要显示判断类型，
    // 当前虽然是字符串库，但是为了方便还是尽量兼容其他类型吧
    static std::size_t length(const mc::variant& arg) {
        MC_ASSERT_THROW(arg.is_string() || arg.is_blob() || arg.is_array() || arg.is_object(),
                        invalid_arg_exception, "表达式求值错误: 无法计算类型 ${type} 的长度",
                        ("type", arg.get_type_name()));
        return arg.size();
    }

    // 连接多个字符串，不判断类型，什么都能加起来
    static mc::variant concat(const mc::variants& args) {
        mc::variant result(mc::type_id::string_type);
        for (const auto& arg : args) {
            result += variant(arg);
        }
        return result;
    }

    static std::string substring(const mc::variant& arg, int start, std::size_t length) {
        MC_ASSERT_THROW(arg.is_string(), invalid_arg_exception,
                        "表达式求值错误: 无法将类型 ${type} 转换为大写",
                        ("type", arg.get_type_name()));
        return std::string(mc::string::substring(arg.get_string(), start, length));
    }

    static std::string to_upper(const mc::variant& arg) {
        MC_ASSERT_THROW(arg.is_string(), invalid_arg_exception,
                        "表达式求值错误: 无法将类型 ${type} 转换为大写",
                        ("type", arg.get_type_name()));
        return mc::string::to_upper(arg.get_string());
    }

    static std::string to_lower(const mc::variant& arg) {
        MC_ASSERT_THROW(arg.is_string(), invalid_arg_exception,
                        "表达式求值错误: 无法将类型 ${type} 转换为小写",
                        ("type", arg.get_type_name()));
        return mc::string::to_lower(arg.get_string());
    }

    static std::string trim(const mc::variant& arg) {
        MC_ASSERT_THROW(arg.is_string(), invalid_arg_exception,
                        "表达式求值错误: 无法将类型 ${type} 转换为小写",
                        ("type", arg.get_type_name()));
        return mc::string::trim(arg.get_string());
    }
};
} // namespace mc::expr

MC_REFLECT(mc::expr::string_module, (length)(concat)(to_upper)(to_lower)(trim)(substring));
MC_REGISTER_BUILTIN_MODULE(string, mc::expr::string_module);