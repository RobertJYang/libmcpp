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

#ifndef MC_VALIDATE_VALIDATOR_H
#define MC_VALIDATE_VALIDATOR_H

#include <mc/common.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace mc {
namespace validate {

// 验证异常基类
class MC_API validation_exception : public std::runtime_error {
public:
    explicit validation_exception(const std::string& message)
        : std::runtime_error(message) {
    }
};

// 属性值类型错误异常
class MC_API property_value_type_error : public validation_exception {
public:
    explicit property_value_type_error(const std::string& message)
        : validation_exception(message) {
    }
};

// 属性值超出范围异常
class MC_API property_value_out_of_range : public validation_exception {
public:
    explicit property_value_out_of_range(const std::string& message)
        : validation_exception(message) {
    }
};

// 字符串长度错误异常
class MC_API string_length_error : public validation_exception {
public:
    explicit string_length_error(const std::string& message)
        : validation_exception(message) {
    }
};

// 格式错误异常
class MC_API format_error : public validation_exception {
public:
    explicit format_error(const std::string& message)
        : validation_exception(message) {
    }
};

// JSON 格式错误异常
class MC_API json_error : public validation_exception {
public:
    explicit json_error(const std::string& message)
        : validation_exception(message) {
    }
};

// 验证器：提供 C++ 校验能力（Lua 绑定基于该类做薄封装）
class MC_API validator {
public:
    // 格式化属性名和值（用于错误消息）
    static std::pair<std::string, std::string> format_name_and_value(std::string_view name, std::string_view val_str,
                                                                     bool need_convert);

    // 检查整数值（必须为整数 + 范围检查）
    static void check_integer(std::string_view name, double val, double min, double max, bool need_convert);

    // 数值范围校验
    static void ranges(std::string_view name, double val, double min, double max, bool need_convert,
                       bool allow_nil = false);

    // 字符串长度校验
    static void lens(std::string_view name, std::string_view val, int min, int max, bool need_convert,
                     bool allow_nil = false);

    // 正则表达式校验
    static void regex(std::string_view name, std::string_view val, std::string_view pattern, bool need_convert,
                      bool allow_nil = false);

    // JSON 格式校验
    static void json(std::string_view val);

private:
    static bool is_integer(double val);
};

} // namespace validate
} // namespace mc

#endif // MC_VALIDATE_VALIDATOR_H
