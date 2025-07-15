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

#include <mc/fmt/format.h>
#include <mc/fmt/formatter_mc.h>

namespace mc::fmt::detail {

void format_variant(const mc::variant& v, format_context& ctx, const format_spec& spec) {
    auto& out = ctx.out();
    if (spec.type != '\0') {
        switch (spec.type) {
        case 'd':
        case 'i':
        case 'u':
        case 'x':
        case 'X':
        case 'o':
        case 'b':
            if (auto v_uint64 = v.try_as<uint64_t>()) {
                detail::format_to(ctx, spec, *v_uint64);
                return;
            }
            break;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
            if (auto v_double = v.try_as<double>()) {
                detail::format_to(ctx, spec, *v_double);
                return;
            }
            break;
        case 's':
            if (v.is_string()) {
                detail::format_to(ctx, spec, v.get_string());
                return;
            } else if (auto v_string = v.try_as<std::string>()) {
                detail::format_to(ctx, spec, *v_string);
                return;
            }
            break;
        case 'c':
            if (auto v_int8 = v.try_as<int8_t>()) {
                detail::format_to(ctx, spec, static_cast<char>(*v_int8));
                return;
            }
            break;
        }
    }

    // 默认行为：按内部类型格式化
    v.visit_with([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, nullptr_t>) {
            out.append("null");
        } else {
            // 对于字符串类型，清除不兼容的格式化参数
            if constexpr (std::is_same_v<T, std::string>) {
                format_spec clean_spec = spec;
                if (clean_spec.type != '\0' && clean_spec.type != 's') {
                    clean_spec.type = '\0'; // 清除不兼容的类型参数
                }
                detail::format_to(ctx, clean_spec, std::forward<decltype(arg)>(arg));
            } else {
                detail::format_to(ctx, spec, std::forward<decltype(arg)>(arg));
            }
        }
    });
}

void format_dict(const mc::dict& dict, format_context& ctx, const format_spec& spec) {
    ctx.append('{');
    bool first = true;
    for (const auto& entry : dict) {
        if (!first) {
            ctx.append(',');
        }
        first = false;
        // 格式化键
        if (entry.key.is_string()) {
            ctx.append('"');
            detail::format_to(ctx, format_spec{}, entry.key.get_string());
            ctx.append('"');
        } else {
            detail::format_to(ctx, format_spec{}, entry.key);
        }
        ctx.append(':');
        detail::format_to(ctx, format_spec{}, entry.value);
    }
    ctx.append('}');
}

void format_blob(const mc::blob& blob, format_context& ctx, const format_spec& spec) {
    auto& out = ctx.out();
    out.append("blob(");
    // TODO:: 增加 debug 格式化 {?} 后可以打印详细信息
    out.append(std::to_string(blob.data.size()));
    out.append(" bytes)");
}

void format_extension(const mc::variant_extension_base& ext, format_context& ctx, const format_spec& spec) {
    auto& out = ctx.out();
    out.append("extension(");
    out.append(ext.get_type_name());
    out.append(")");
}

} // namespace mc::fmt::detail
