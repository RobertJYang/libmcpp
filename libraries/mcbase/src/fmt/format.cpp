/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
 * @file format.cpp
 * @brief 实现字符串格式化功能
 */

#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/format.h>
#include <mc/variant.h>

namespace mc::fmt {

// 格式化字符串并追加到结果字符串中
static void format_dict_base(std::string& result, std::string_view format_str, const mc::dict& args, bool icase) {
    if (format_str.empty()) {
        return;
    }

    result.reserve(result.size() + format_str.size() * 2);

    detail::runtime_arg_store store;
    store.icase = icase;
    store.set_dict_args(&args);
    format_context ctx(result, store);
    detail::format_parser::parse(format_str, ctx);
}

void format_dict(std::string& result, std::string_view format_str, const dict& args) {
    format_dict_base(result, format_str, args, false);
}

std::string format_dict(std::string_view format_str, const dict& args) {
    std::string result;
    format_dict_base(result, format_str, args, false);
    return result;
}

void format_dict_icase(std::string& result, std::string_view format_str, const dict& args) {
    format_dict_base(result, format_str, args, true);
}

std::string format_dict_icase(std::string_view format_str, const dict& args) {
    std::string result;
    format_dict_base(result, format_str, args, true);
    return result;
}

std::string format_v(const char* format, ...) {
    va_list args;
    va_start(args, format);
    auto result = format_vv(format, args);
    va_end(args);
    return result;
}

std::string format_vv(const char* format, va_list args) {
    std::string result;
    format_vv(result, format, args);
    return result;
}

bool format_v(std::string& result, const char* format, ...) {
    va_list args;
    va_start(args, format);
    bool ret = format_vv(result, format, args);
    va_end(args);
    return ret;
}

bool format_vv(std::string& result, const char* format, va_list args) {
    va_list args_tmp;
    va_copy(args_tmp, args);
    int size = std::vsnprintf(nullptr, 0, format, args_tmp);
    va_end(args_tmp);
    if (size <= 0) {
        result.clear();
        return false;
    }

    result.resize(size + 1);
    int ret = std::vsnprintf(const_cast<char*>(result.data()), size + 1, format, args);
    if (ret <= 0) {
        result.clear();
        return false;
    }

    result.resize(ret);
    return true;
}

class get_format_args_context {
public:
    using arg_type = detail::format_arg;

    get_format_args_context() {
    }

    bool get_arg(size_t, detail::format_arg&) const {
        return true;
    }

    bool get_named_arg(std::string_view name, detail::format_arg&, size_t&) const {
        arg_names[name] = true;
        return true;
    }

    void format_arg(const detail::format_arg& arg, detail::format_spec& spec) {
        MC_UNUSED(arg);
        MC_UNUSED(spec);
    }

    void append(char) {
    }

    void set_used(size_t index) {
        MC_UNUSED(index);
    }

    bool resolve_dynamic_param(size_t index, std::string_view name, int& out) {
        MC_UNUSED(index);
        MC_UNUSED(name);
        MC_UNUSED(out);
        return true;
    }

    bool process_result(const detail::parser_result& result) {
        return !result.has_error();
    }

    void raise_error(const detail::parser_result& result) {
        if (!result.has_error()) {
            return;
        }

        is_valid = false;
    }

    mutable mc::dict arg_names;
    bool             is_valid{true};
};

bool get_format_args(std::string_view format, mc::dict& arg_names) {
    if (format.empty()) {
        return true;
    }

    get_format_args_context ctx;
    detail::parse_format_string(format, ctx);
    arg_names = ctx.arg_names;
    return ctx.is_valid;
}

} // namespace mc::fmt