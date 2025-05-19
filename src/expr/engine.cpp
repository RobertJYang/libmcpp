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
#include <mc/expr/context.h>
#include <mc/expr/engine.h>
#include <mc/expr/function.h>
#include <mc/expr/lexer.h>
#include <mc/expr/node.h>
#include <mc/expr/parser.h>

#include <cmath>
#include <deque>
#include <iostream>
#include <unordered_map>

namespace mc::expr {

// 表达式引擎实现
struct engine::impl {
    // 内置函数注册表
    std::unordered_map<std::string, std::shared_ptr<function>> built_in_functions;

    // 注册标准内置函数
    void register_standard_functions() {
        // 数学函数
        built_in_functions["abs"] =
            make_simple_function("abs", [](const mc::variant& value) -> mc::variant {
                try {
                    if (value.is_double()) {
                        return std::fabs(value.as_double());
                    } else {
                        return std::abs(value.as_int64());
                    }
                } catch (const std::exception&) {
                    MC_THROW(invalid_op_exception, "表达式求值错误: abs 函数参数必须是数值类型");
                }
            });

        built_in_functions["min"] = make_simple_function("min", [](const mc::variants& args) {
            mc::variant result = args[0];
            for (size_t i = 1; i < args.size(); ++i) {
                if (args[i] < result) {
                    result = args[i];
                }
            }

            return result;
        });

        built_in_functions["max"] = make_simple_function("max", [](const mc::variants& args) {
            mc::variant result = args[0];
            for (size_t i = 1; i < args.size(); ++i) {
                if (args[i] > result) {
                    result = args[i];
                }
            }

            return result;
        });

        // 字符串函数
        built_in_functions["length"] = make_simple_function("length", [](const mc::variant& arg) {
            return arg.size();
        });

        built_in_functions["concat"] = make_simple_function("concat", [](const mc::variants& args) {
            mc::variant result(mc::type_id::string_type);
            for (const auto& arg : args) {
                result += arg;
            }
            return result;
        });

        // 类型转换函数
        built_in_functions["to_number"] =
            make_simple_function("to_number", [](const mc::variant& arg) {
                return arg.as_int64();
            });

        built_in_functions["to_string"] =
            make_simple_function("to_string", [](const mc::variant& arg) {
                return arg.as_string();
            });

        built_in_functions["to_bool"] = make_simple_function("to_bool", [](const mc::variant& arg) {
            return arg.as_bool();
        });
    }
};

engine::engine() : m_impl(std::make_unique<impl>()) {
    m_impl->register_standard_functions();
}

engine::~engine() = default;

std::shared_ptr<node> engine::compile(std::string_view expr) {
    lexer              lex(expr);
    std::vector<token> tokens = lex.scan_tokens();

    parser p(std::move(tokens));
    return p.parse();
}

mc::variant engine::evaluate(std::string_view expr, const context& ctx) {
    std::shared_ptr<node> ast = compile(expr);
    return ast->evaluate(ctx);
}

// 注册函数
void engine::register_function(std::shared_ptr<function> func) {
    if (!func) {
        MC_THROW(invalid_arg_exception, "表达式引擎错误: 函数指针不能为空");
    }

    m_impl->built_in_functions[func->get_name()] = std::move(func);
}

context engine::create_context() const {
    context ctx;

    for (const auto& [name, func] : m_impl->built_in_functions) {
        ctx.set_function(func);
    }

    return ctx;
}

} // namespace mc::expr