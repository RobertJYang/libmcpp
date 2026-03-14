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

#ifndef MC_FMT_FORMAT_SPEC_H
#define MC_FMT_FORMAT_SPEC_H

#include <mc/fmt/format_arg.h>

#include <string_view>
#include "securec.h"

namespace mc {
class dict;
}

namespace mc::fmt::detail {
[[noreturn]] MC_API void throw_format_error(std::string_view fmt_str, const mc::dict& args);

// 格式说明符
struct format_spec {
    enum class align {
        none,   // 默认对齐
        left,   // 左对齐 '<'
        right,  // 右对齐 '>'
        center, // 居中对齐 '^'
    };

    enum class sign {
        none,  // 默认符号处理
        plus,  // 总是显示符号 '+'
        minus, // 仅负数显示符号 '-'
        space, // 正数显示空格，负数显示符号 ' '
    };

    align            alignment       = align::none;   // 对齐方式
    sign             sign_mode       = sign::none;    // 符号处理方式
    char             fill            = ' ';           // 填充字符
    int              width           = 0;             // 字段宽度
    int              precision       = -1;            // 精度
    size_t           width_index     = INVALID_INDEX; // 动态宽度参数索引，INVALID_INDEX 表示未使用
    size_t           precision_index = INVALID_INDEX; // 动态精度参数索引，INVALID_INDEX 表示未使用
    std::string_view width_name;                      // 动态宽度参数名称（命名参数）
    std::string_view precision_name;                  // 动态精度参数名称（命名参数）
    char             type               = '\0';       // 类型标识符
    bool             zero_pad           = false;      // 是否使用零填充
    bool             alternate_form     = false;      // 支持 # 标志
    bool             custom_spec_in_use = false;      // 自定义格式是否被使用
    char             custom_spec[128]   = {0};        // 自定义格式

    constexpr format_spec()
    {
    }

    template <typename CustomSpec>
    CustomSpec& get_custom_spec()
    {
        static_assert(sizeof(CustomSpec) <= sizeof(custom_spec), "CustomSpec is too large");
        if (!custom_spec_in_use) {
            custom_spec_in_use = true;
            new (custom_spec) CustomSpec();
        }
        return *reinterpret_cast<CustomSpec*>(custom_spec);
    }

    template <typename CustomSpec>
    const CustomSpec& get_custom_spec() const
    {
        auto* self = const_cast<format_spec*>(this);
        return self->template get_custom_spec<CustomSpec>();
    }

    template <typename CustomSpec>
    void set_custom_spec(const CustomSpec& spec)
    {
        static_assert(sizeof(CustomSpec) <= sizeof(custom_spec), "CustomSpec is too large");
        static_assert(std::is_trivially_copyable_v<CustomSpec>,
                      "CustomSpec must be trivially copyable");

        custom_spec_in_use = true;
        new (custom_spec) CustomSpec();
        (void)memcpy_s(custom_spec, sizeof(custom_spec), &spec, sizeof(CustomSpec));
    }

    constexpr bool parse_alignment_and_fill(const char*& ptr, const char* end)
    {
        if (ptr + 1 < end && (ptr[1] == '<' || ptr[1] == '>' || ptr[1] == '^')) {
            fill      = *ptr;
            alignment = ptr[1] == '<'   ? align::left
                        : ptr[1] == '>' ? align::right
                                        : align::center;
            ptr += 2;
        } else if (ptr < end && (*ptr == '<' || *ptr == '>' || *ptr == '^')) {
            alignment = *ptr == '<'   ? align::left
                        : *ptr == '>' ? align::right
                                      : align::center;
            ++ptr;
        }

        return ptr < end;
    }

    constexpr bool parse_sign(const char*& ptr, const char* end)
    {
        if (ptr < end) {
            switch (*ptr) {
                case '+':
                    sign_mode = sign::plus;
                    ++ptr;
                    break;
                case '-':
                    sign_mode = sign::minus;
                    ++ptr;
                    break;
                case ' ':
                    sign_mode = sign::space;
                    ++ptr;
                    break;
                default:
                    break;
            }
        }

        return ptr < end;
    }

    constexpr bool parse_alternate(const char*& ptr, const char* end)
    {
        if (ptr < end && *ptr == '#') {
            alternate_form = true;
            ++ptr;
        }

        return ptr < end;
    }

    constexpr bool parse_zero_pad(const char*& ptr, const char* end)
    {
        if (ptr < end && *ptr == '0') {
            zero_pad = true;
            if (alignment == align::none) {
                fill = '0';
            }
            ++ptr;
        }

        return ptr < end;
    }

    constexpr bool parse_width(const char*& ptr, const char* end)
    {
        if (ptr < end && *ptr == '{') {
            // 动态宽度参数
            ++ptr;
            const char* name_start = ptr;

            // 查找名称结束位置
            while (ptr < end && *ptr != '}') {
                ++ptr;
            }

            if (ptr >= end) {
                return false;
            }

            std::string_view name(name_start, ptr - name_start);
            ++ptr; // 跳过 '}'

            if (!name.empty() && !mc::fmt::detail::to_integer(name, width_index)) {
                width_name = name;
            }
        } else if (ptr < end && isdigit(*ptr)) {
            width = 0;
            while (ptr < end && isdigit(*ptr)) {
                width = width * 10 + (*ptr - '0');
                ++ptr;
            }
        }
        return ptr < end;
    }

    constexpr bool parse_precision(const char*& ptr, const char* end)
    {
        if (ptr < end && *ptr == '.') {
            ++ptr;
            if (ptr < end && *ptr == '{') {
                // 动态精度参数
                ++ptr;
                const char* name_start = ptr;

                // 查找名称结束位置
                while (ptr < end && *ptr != '}') {
                    ++ptr;
                }

                if (ptr >= end) {
                    return false;
                }

                std::string_view name(name_start, ptr - name_start);
                ++ptr; // 跳过 '}'

                if (!name.empty() && !mc::fmt::detail::to_integer(name, precision_index)) {
                    precision_name = name;
                }
            } else if (ptr < end && isdigit(*ptr)) {
                precision = 0;
                while (ptr < end && isdigit(*ptr)) {
                    precision = precision * 10 + (*ptr - '0');
                    ++ptr;
                }
            } else {
                // 精度点后没有数字也没有{N}
                return false;
            }
        }
        return ptr < end;
    }

    constexpr bool parse_type(const char*& ptr, const char* end)
    {
        if (ptr < end && *ptr != '}') {
            type = *ptr++;
        }

        return ptr < end;
    }

    template <typename Arg>
    constexpr const char* parse(const Arg* arg, const char* ptr, const char* end)
    {
        if (ptr >= end) {
            return nullptr;
        }

        if (arg && arg->parser(ptr, end, *this)) {
            return ptr;
        }

        if (!parse_alignment_and_fill(ptr, end) ||
            !parse_sign(ptr, end) ||
            !parse_alternate(ptr, end) ||
            !parse_zero_pad(ptr, end) ||
            !parse_width(ptr, end) ||
            !parse_precision(ptr, end) ||
            !parse_type(ptr, end)) {
            return nullptr;
        }

        return ptr;
    }
};

} // namespace mc::fmt::detail

#endif // MC_FMT_FORMAT_SPEC_H