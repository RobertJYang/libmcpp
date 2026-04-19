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
#include <mutex>

namespace mc::expr {

namespace detail {

class reflected_builtin_function : public function {
public:
    reflected_builtin_function(mc::string name, mc::reflect::method_info_ptr method)
        : m_name(std::move(name)), m_method(std::move(method))
    {}

    mc::variant call(const mc::variants& args) const override
    {
        try {
            return m_method->invoke_static_erased(args);
        } catch (const mc::bad_function_call_exception& e) {
            MC_THROW(mc::invalid_arg_exception, "${message}", ("message", e.what()));
        }
    }

    const mc::string& get_name() const override
    {
        return m_name;
    }

    int get_arg_count() const override
    {
        return static_cast<int>(m_method->arg_count());
    }

private:
    mc::string                   m_name;
    mc::reflect::method_info_ptr m_method;
};

function_ptr make_reflected_builtin_function(const mc::reflect::method_registration_info& method)
{
    auto runtime_method = method.create_runtime_method_ptr();
    MC_ASSERT_THROW(runtime_method, mc::runtime_exception, "builtin 方法 ${name} 运行时元数据创建失败",
                    ("name", method.name));
    MC_ASSERT_THROW(runtime_method->is_static(), mc::invalid_arg_exception, "builtin 方法 ${name} 必须是静态函数",
                    ("name", method.name));
    return mc::make_shared<reflected_builtin_function>(mc::string(method.name), std::move(runtime_method));
}

} // namespace detail

struct builtin::impl {
    context builtin_context;
};

builtin& builtin::get_instance()
{
    // 不使用 mc::singleton 管理，在程序启动时各个模块会注册自己的内建函数，单例销毁后无法重建
    static builtin instance;
    return instance;
}

builtin::builtin() : m_impl(std::make_unique<impl>())
{}

builtin::~builtin() = default;

int builtin::register_symbol(function_ptr func)
{
    return m_impl->builtin_context.register_function(func);
}

int builtin::register_symbol(mc::string name, mc::variant value)
{
    return m_impl->builtin_context.register_variable(std::move(name), value);
}

context& builtin::get_context()
{
    return m_impl->builtin_context;
}

} // namespace mc::expr