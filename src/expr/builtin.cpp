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

int builtin::register_symbol(std::shared_ptr<function> func)
{
    return m_impl->builtin_context.register_function(func);
}

int builtin::register_symbol(std::string name, mc::variant value)
{
    return m_impl->builtin_context.register_variable(std::move(name), value);
}

context& builtin::get_context()
{
    return m_impl->builtin_context;
}

} // namespace mc::expr