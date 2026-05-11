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

#include <mc/engine/engine.h>
#include <mc/exception.h>
#include <mc/expr/builtin.h>
#include <mc/expr/context.h>
#include <mc/expr/engine.h>
#include <mc/expr/function.h>
#include <mc/expr/lexer.h>
#include <mc/expr/node.h>
#include <mc/expr/parser.h>
#include <mc/singleton.h>

namespace mc::expr {
using abstract_object = mc::engine::abstract_object;

static bool resolve_object_path(engine& expr_engine, mc::string_view path_pattern, const abstract_object& obj,
                                mc::string& path);

namespace {

class expr_path_template_backend : public mc::engine::path_template_backend {
public:
    mc::string_view name() const noexcept override
    {
        return "mc.expr.path_template";
    }

    bool try_resolve(mc::string_view path_pattern, const abstract_object& obj, mc::string& path) const override
    {
        return resolve_object_path(engine::get_instance(), path_pattern, obj, path);
    }
};

mc::shared_ptr<mc::engine::path_template_backend> make_expr_path_template_backend()
{
    return mc::make_shared<expr_path_template_backend>();
}

} // namespace

static bool resolve_object_path(engine& expr_engine, mc::string_view path_pattern, const abstract_object& obj,
                                mc::string& path)
{
    if (!lexer::is_template_string(path_pattern)) {
        return false;
    }

    // 是路径表达式，使用表达式引擎计算路径
    auto ctx = expr_engine.make_context(const_cast<abstract_object*>(&obj));

    mc::expr::lexer  lex(path_pattern);
    auto             tokens = lex.scan_template_string_tokens();
    mc::expr::parser p(std::move(tokens));
    auto             node     = p.parse();
    auto             path_val = node->evaluate(ctx);
    MC_ASSERT_THROW(path_val.is_string(), mc::invalid_arg_exception, "resolve object path ${path} failed",
                    ("path", path_pattern));
    path = path_val.get_string();
    return true;
}

struct engine::impl {
    impl() : global_context(&builtin::get_instance().get_context())
    {}

    ~impl() = default;

    context global_context;
};

engine::engine() : m_impl(std::make_unique<impl>())
{
    register_path_template_backend();
}

engine::~engine() = default;

engine& engine::get_instance()
{
    return mc::singleton<engine>::instance();
}

node_ptr engine::compile(mc::string_view expr)
{
    lexer            lex(expr);
    mc::array<token> tokens = lex.scan_tokens();

    parser p(std::move(tokens));
    return p.parse();
}

mc::variant engine::evaluate(mc::string_view expr, const context_base& ctx)
{
    node_ptr ast = compile(expr);
    return ast->evaluate(ctx);
}

context& engine::get_global_context() const
{
    return m_impl->global_context;
}

context engine::make_context(context_base* parent) const
{
    return context(parent ? parent : &get_global_context());
}

context engine::make_context(const mc::dict& variables, context_base* parent) const
{
    return context(variables, parent ? parent : &get_global_context());
}

object_context engine::make_context(mc::engine::abstract_object* object, context_base* parent) const
{
    return object_context(object, parent ? parent : &get_global_context());
}

mc::shared_ptr<mc::engine::path_template_backend> create_path_template_backend()
{
    return make_expr_path_template_backend();
}

void register_path_template_backend()
{
    mc::engine::engine::add_path_template_backend(create_path_template_backend());
}

} // namespace mc::expr