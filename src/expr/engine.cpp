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
#include <mc/expr/context.h>
#include <mc/expr/engine.h>
#include <mc/expr/function.h>
#include <mc/expr/lexer.h>
#include <mc/expr/node.h>
#include <mc/expr/parser.h>
#include <mc/dbus/shm/gvariant_convert.h>

namespace mc::expr {

struct engine::impl {
    impl() : global_context(&builtin::get_instance().get_context()) {
    }

    ~impl() = default;

    context global_context;
};

engine::engine() : m_impl(std::make_unique<impl>()) {
}

engine::~engine() = default;

node_ptr engine::compile(std::string_view expr) {
    lexer              lex(expr);
    std::vector<token> tokens = lex.scan_tokens();

    parser p(std::move(tokens));
    return p.parse();
}

mc::variant engine::evaluate(std::string_view expr, const context_base& ctx) {
    node_ptr ast = compile(expr);
    return ast->evaluate(ctx);
}

GVariant* engine::evaluate_as_gvariant(std::string_view expr, const context_base& ctx) {
    mc::variant result = evaluate(expr, ctx);
    return mc::dbus::gvariant_convert::to_gvariant(result);
}

context& engine::get_global_context() const {
    return m_impl->global_context;
}

context engine::make_context(context_base* parent) const {
    return context(parent ? parent : &get_global_context());
}

context engine::make_context(const mc::dict& variables, context_base* parent) const {
    return context(variables, parent ? parent : &get_global_context());
}

object_context engine::make_context(mc::engine::abstract_object* object,
                                    context_base*                parent) const {
    return object_context(object, parent ? parent : &get_global_context());
}

} // namespace mc::expr