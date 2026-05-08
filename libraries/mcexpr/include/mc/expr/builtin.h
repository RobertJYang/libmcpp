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

/**
 * @file builtin.h
 * @brief 定义内建注册表，负责管理表达式内建函数和内建常量s
 */
#ifndef MC_EXPR_BUILTIN_H
#define MC_EXPR_BUILTIN_H

#include <mc/expr/function.h>
#include <mc/reflect.h>
#include <mc/string.h>

namespace mc::expr {

class context;
class function;

namespace detail {
function_ptr make_reflected_builtin_function(const mc::reflect::method_registration_info& method);
}

/**
 * @brief 内建注册表类，管理所有内建函数和内建常量
 */
class MC_API builtin {
public:
    /**
     * @brief 获取单例实例
     */
    static builtin& get_instance();

    /**
     * @brief 析构函数
     */
    ~builtin();

    /**
     * @brief 注册内建函数
     * @param func 函数对象
     */
    int register_symbol(function_ptr func);

    template <typename F, std::enable_if_t<std::is_function_v<std::remove_pointer_t<std::decay_t<F>>>, int> = 0>
    int register_symbol(mc::string name, F&& func)
    {
        return register_symbol(make_simple_function(std::move(name), std::forward<F>(func)));
    }

    /**
     * @brief 注册变量
     * @param name 变量名称
     * @param value 变量值
     */
    int register_symbol(mc::string name, mc::variant value);

    /**
     * @brief 注册模块的所有方法
     * @tparam T 模块类型
     * @note
     * 模块类型必须使用MC_REFLECT宏注册，暂时只支持注册静态成员函数，且调用不需要传入模块名，如果后续
     * 存在模块间重名，再支持通过模块名限定
     */
    template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
    int register_module()
    {
        auto methods = mc::reflect::get_static_methods<T>();
        mc::traits::tuple_for_each(methods, [this](auto* method) {
            this->register_symbol(detail::make_reflected_builtin_function(*method));
        });
        return 0;
    }

    /**
     * @brief 获取内建上下文
     * @return 内建上下文
     */
    context& get_context();

private:
    builtin();

    struct impl;
    std::unique_ptr<impl> m_impl;
};

/**
 * @brief 注册所有 mcexpr 内建模块（conversion / math / string 等）
 */
MC_API void register_builtin_modules();

/**
 * @brief 注册内建函数或内建变量的宏
 *
 * 在cpp文件的命名空间外部使用此宏自动注册内建函数或内建变量
 */
#define MC_REGISTER_BUILTIN_SYMBOL(name, symbol)                                                                       \
    inline auto name##_symbol_id = mc::expr::builtin::get_instance().register_symbol(#name, symbol);

} // namespace mc::expr

#endif // MC_EXPR_BUILTIN_H