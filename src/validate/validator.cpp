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

#include <mc/validate/validator.h>

#include <glib.h>
#include <mc/json.h>
#include <mc/variant.h>

#include <cmath>
#include <sstream>

namespace mc {
namespace validate {

std::pair<std::string, std::string> validator::format_name_and_value(std::string_view name, std::string_view val_str,
                                                                     bool need_convert) {
    std::string final_name(name);
    std::string final_val(val_str);

    if (need_convert) {
        final_name = "%" + std::string(name);
        final_val  = "%" + std::string(name) + ":" + std::string(val_str);
    }

    return {final_name, final_val};
}

bool validator::is_integer(double val) {
    // 检查是否是 NaN
    if (val != val) {
        return false;
    }
    // 检查是否是整数
    double int_part;
    return std::modf(val, &int_part) == 0.0;
}

void validator::check_integer(std::string_view name, double val, double min, double max, bool need_convert) {
    // 检查是否为整数
    if (!is_integer(val)) {
        std::ostringstream oss;
        oss << val;
        auto [final_name, final_val] = format_name_and_value(name, oss.str(), need_convert);
        std::string msg              = "PropertyValueTypeError: The value " + final_val + " for the property " + final_name +
                          " is of a different type than the property can accept.";
        throw property_value_type_error(msg);
    }

    // 检查范围
    if (val < min || val > max) {
        std::ostringstream oss;
        oss << val;
        auto [final_name, final_val] = format_name_and_value(name, oss.str(), need_convert);
        std::string msg              = "PropertyValueOutOfRange: The value '" + final_val + "' for the property " + final_name +
                          " is not in the supported range of acceptable values.";
        throw property_value_out_of_range(msg);
    }
}

void validator::ranges(std::string_view name, double val, double min, double max, bool need_convert, bool allow_nil) {
    (void)allow_nil;

    if (val < min || val > max) {
        std::ostringstream oss;
        oss << val;
        auto [final_name, final_val] = format_name_and_value(name, oss.str(), need_convert);
        std::string msg              = "PropertyValueOutOfRange: The value '" + final_val + "' for the property " + final_name +
                          " is not in the supported range of acceptable values.";
        throw property_value_out_of_range(msg);
    }
}

void validator::lens(std::string_view name, std::string_view val, int min, int max, bool need_convert, bool allow_nil) {
    (void)allow_nil;

    size_t len = val.length();
    if (len < static_cast<size_t>(min)) {
        auto [final_name, final_val] = format_name_and_value(name, val, need_convert);
        (void)final_name;
        std::ostringstream oss;
        oss << min;
        std::string msg = "StringValueTooShort: The string '" + final_val + "' was under the minimum required length " +
                          oss.str() + ".";
        throw string_length_error(msg);
    }

    if (len > static_cast<size_t>(max)) {
        auto [final_name, final_val] = format_name_and_value(name, val, need_convert);
        (void)final_name;
        std::ostringstream oss;
        oss << max;
        std::string msg = "StringValueTooLong: The string '" + final_val + "' exceeds the length limit " + oss.str() + ".";
        throw string_length_error(msg);
    }
}

void validator::regex(std::string_view name, std::string_view val, std::string_view pattern, bool need_convert,
                      bool allow_nil) {
    (void)allow_nil;

    std::string pattern_str(pattern);
    std::string val_str(val);

    GError* error = nullptr;
    GRegex* regex = g_regex_new(pattern_str.c_str(), static_cast<GRegexCompileFlags>(0),
                                static_cast<GRegexMatchFlags>(0), &error);
    if (regex == nullptr) {
        if (error) {
            g_error_free(error);
        }
        auto [final_name, final_val] = format_name_and_value(name, val, need_convert);
        std::string msg              = "PropertyValueFormatError: The value " + final_val + " for the property " + final_name +
                          " is of a different format than the property can accept.";
        throw format_error(msg);
    }

    gboolean matched = g_regex_match(regex, val_str.c_str(), static_cast<GRegexMatchFlags>(0), nullptr);
    g_regex_unref(regex);

    if (!matched) {
        auto [final_name, final_val] = format_name_and_value(name, val, need_convert);
        std::string msg              = "PropertyValueFormatError: The value " + final_val + " for the property " + final_name +
                          " is of a different format than the property can accept.";
        throw format_error(msg);
    }
}

void validator::json(std::string_view val) {
    std::string json_str(val);
    try {
        mc::json::json_decode(json_str);
    } catch (...) {
        throw json_error("MalformedJSON: The request body submitted was malformed JSON and could not be parsed by the "
                         "receiving service.");
    }
}

} // namespace validate
} // namespace mc
