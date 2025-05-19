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
        built_in_functions["abs"] = make_simple_function(
            "abs",
            [](const mc::variants& args) -> mc::variant {
                if (args.size() != 1) {
                    MC_THROW(invalid_arg_exception, "表达式求值错误: abs 函数需要一个参数");
                }

                const mc::variant& arg = args[0];
                if (arg.is_double()) {
                    return std::fabs(arg.as_double());
                } else if (arg.is_integer()) {
                    return std::abs(arg.as_int64());
                }

                MC_THROW(invalid_op_exception, "表达式求值错误: abs 函数参数必须是数值类型");
            },
            1);

        built_in_functions["min"] = make_simple_function(
            "min",
            [](const mc::variants& args) -> mc::variant {
                if (args.size() < 2) {
                    MC_THROW(invalid_arg_exception, "表达式求值错误: min 函数至少需要两个参数");
                }

                mc::variant result = args[0];
                for (size_t i = 1; i < args.size(); ++i) {
                    if (args[i].is_numeric() && result.is_numeric()) {
                        if (args[i].as_double() < result.as_double()) {
                            result = args[i];
                        }
                    } else {
                        MC_THROW(invalid_op_exception,
                                 "表达式求值错误: min 函数参数必须是数值类型");
                    }
                }

                return result;
            },
            -1);

        built_in_functions["max"] = make_simple_function(
            "max",
            [](const mc::variants& args) -> mc::variant {
                if (args.size() < 2) {
                    MC_THROW(invalid_arg_exception, "表达式求值错误: max 函数至少需要两个参数");
                }

                mc::variant result = args[0];
                for (size_t i = 1; i < args.size(); ++i) {
                    if (args[i].is_numeric() && result.is_numeric()) {
                        if (args[i].as_double() > result.as_double()) {
                            result = args[i];
                        }
                    } else {
                        MC_THROW(invalid_op_exception,
                                 "表达式求值错误: max 函数参数必须是数值类型");
                    }
                }

                return result;
            },
            -1);

        // 字符串函数
        built_in_functions["length"] = make_simple_function(
            "length",
            [](const mc::variants& args) -> mc::variant {
                if (args.size() != 1) {
                    MC_THROW(invalid_arg_exception, "表达式求值错误: length 函数需要一个参数");
                }

                const mc::variant& arg = args[0];
                if (arg.is_string()) {
                    return static_cast<int64_t>(arg.get_string().size());
                } else if (arg.is_array()) {
                    return static_cast<int64_t>(arg.get_array().size());
                }

                MC_THROW(invalid_op_exception,
                         "表达式求值错误: length 函数参数必须是字符串或数组类型");
            },
            1);

        built_in_functions["concat"] = make_simple_function(
            "concat",
            [](const mc::variants& args) -> mc::variant {
                if (args.empty()) {
                    return std::string();
                }

                std::string result;
                for (const auto& arg : args) {
                    if (arg.is_string()) {
                        result += arg.get_string();
                    } else {
                        result += mc::to_string(arg);
                    }
                }

                return result;
            },
            -1);

        // 类型转换函数
        built_in_functions["toNumber"] = make_simple_function(
            "toNumber",
            [](const mc::variants& args) -> mc::variant {
                if (args.size() != 1) {
                    MC_THROW(invalid_arg_exception, "表达式求值错误: toNumber 函数需要一个参数");
                }

                const mc::variant& arg = args[0];
                if (arg.is_numeric()) {
                    return arg;
                } else if (arg.is_string()) {
                    try {
                        return std::stod(arg.get_string());
                    } catch (const std::exception&) {
                        MC_THROW(invalid_op_exception, "表达式求值错误: 无法将字符串转换为数值");
                    }
                } else if (arg.is_bool()) {
                    return arg.as_bool() ? 1.0 : 0.0;
                }

                MC_THROW(invalid_op_exception, "表达式求值错误: 无法将参数转换为数值");
            },
            1);

        built_in_functions["toString"] = make_simple_function(
            "toString",
            [](const mc::variants& args) -> mc::variant {
                if (args.size() != 1) {
                    MC_THROW(invalid_arg_exception, "表达式求值错误: toString 函数需要一个参数");
                }

                return mc::to_string(args[0]);
            },
            1);

        built_in_functions["toBoolean"] = make_simple_function(
            "toBoolean",
            [](const mc::variants& args) -> mc::variant {
                if (args.size() != 1) {
                    MC_THROW(invalid_arg_exception, "表达式求值错误: toBoolean 函数需要一个参数");
                }

                const mc::variant& arg = args[0];
                if (arg.is_bool()) {
                    return arg;
                } else if (arg.is_numeric()) {
                    return arg.as_double() != 0;
                } else if (arg.is_string()) {
                    return !arg.get_string().empty();
                }

                return false;
            },
            1);
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