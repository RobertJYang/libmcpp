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

#include <mc/engine/expr.h>
#include <mc/engine/object.h>
#include <mc/expr.h>
#include <mc/variant.h>

namespace mc::engine {

// // 表达式引擎类
// class expr_engine {
// private:
//     // 单例实例
//     static expr_engine& instance() {
//         static expr_engine s_instance;
//         return s_instance;
//     }

//     // 私有构造函数
//     expr_engine() = default;

// public:
//     // 获取表达式引擎单例
//     static expr_engine& get_instance() {
//         return instance();
//     }

//     // 评估表达式
//     mc::variant evaluate(const std::string& expr, abstract_object& obj) {
//         try {
//             // 创建表达式引擎和上下文
//             mc::expr::engine expr_engine;
//             auto             ctx = expr_engine.create_context();

//             // 获取引擎和对象
//             auto& engine   = engine::instance();
//             auto& path_mgr = engine.get_path_manager();

//             // 设置变量和函数
//             if (obj) {
//                 // 添加对象属性和方法
//                 for (const auto& prop : obj->get_properties()) {
//                     ctx.set_variable(prop.first, prop.second);
//                 }

//                 // 添加父对象属性
//                 std::shared_ptr<mc::engine::object> parent = obj->get_parent();
//                 while (parent) {
//                     for (const auto& prop : parent->get_properties()) {
//                         if (!ctx.has_variable(prop.first)) {
//                             ctx.set_variable(prop.first, prop.second);
//                         }
//                     }
//                     parent = parent->get_parent();
//                 }

//                 // 添加对象方法
//                 for (const auto& method : obj->get_methods()) {
//                     ctx.set_function(make_simple_function(
//                         method.first,
//                         [obj, method,
//                          &engine](const mc::variants& args) -> mc::variant {
//                             mc::dict params;
//                             for (size_t i = 0; i < args.size(); ++i) {
//                                 params["arg" + std::to_string(i + 1)] = args[i];
//                             }
//                             return obj->call_method(method.first, params);
//                         },
//                         -1));
//                 }

//                 // 添加父对象方法
//                 parent = obj->get_parent();
//                 while (parent) {
//                     for (const auto& method : parent->get_methods()) {
//                         if (!ctx.has_function(method.first)) {
//                             ctx.set_function(make_simple_function(
//                                 method.first,
//                                 [parent, method,
//                                  &engine](const mc::variants& args) -> mc::variant {
//                                     mc::dict params;
//                                     for (size_t i = 0; i < args.size(); ++i) {
//                                         params["arg" + std::to_string(i + 1)] = args[i];
//                                     }
//                                     return parent->call_method(method.first, params);
//                                 },
//                                 -1));
//                         }
//                     }
//                     parent = parent->get_parent();
//                 }
//             }

//             // 添加全局系统服务
//             for (const auto& service : engine.get_services()) {
//                 ctx.set_variable(service.first, service.second->get_name());

//                 // 添加服务方法
//                 for (const auto& method : service.second->get_methods()) {
//                     ctx.set_function(make_simple_function(
//                         service.first + "." + method.first,
//                         [service, method,
//                          &engine](const mc::variants& args) -> mc::variant {
//                             mc::dict params;
//                             for (size_t i = 0; i < args.size(); ++i) {
//                                 params["arg" + std::to_string(i + 1)] = args[i];
//                             }
//                             return service.second->call_method(method.first, params);
//                         },
//                         -1));
//                 }
//             }

//             // 计算表达式结果
//             return expr_engine.evaluate(expr, ctx);
//         } catch (const mc::expr::error& e) {
//             // 捕获表达式异常
//             MC_THROW(mc::engine::error, "表达式求值错误: ${message}", ("message", e.what()));
//         } catch (const std::exception& e) {
//             // 捕获其他异常
//             MC_THROW(mc::engine::error, "表达式求值错误: ${message}", ("message", e.what()));
//         }
//     }
// };

// 获取表达式引擎
mc::expr::engine& get_expr_engine() {
    static mc::expr::engine s_engine;
    return s_engine;
}

// 评估表达式
mc::variant evaluate_expr(const std::string& expr, abstract_object& obj) {
    // return expr_engine::get_instance().evaluate(expr, obj);
    return {};
}

// 解析路径模式
std::string resolve_path_pattern(const std::string& pattern, abstract_object& obj) {
    if (pattern.empty()) {
        return pattern;
    }

    // 如果不包含表达式占位符，直接返回原始路径
    if (pattern.find("${") == std::string::npos) {
        return pattern;
    }

    std::string result = pattern;
    size_t      start  = 0;

    // 解析所有表达式占位符
    while ((start = result.find("${", start)) != std::string::npos) {
        size_t end = result.find("}", start);
        if (end == std::string::npos) {
            break;
        }

        // 提取表达式
        std::string expr = result.substr(start + 2, end - start - 2);

        // 计算表达式值
        mc::variant value = evaluate_expr(expr, obj);

        // 替换表达式占位符
        result.replace(start, end - start + 1, mc::to_string(value));

        // 继续查找下一个占位符
        start += mc::to_string(value).length();
    }

    return result;
}

} // namespace mc::engine