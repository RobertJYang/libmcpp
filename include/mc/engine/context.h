/**
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

#ifndef MC_ENGINE_CONTEXT_H
#define MC_ENGINE_CONTEXT_H

#include <mc/dbus/message.h>
#include <mc/dict.h>
#include <mc/engine/call_stack.h>
#include <mc/error_engine.h>
#include <mc/memory.h>
#include <mc/variant.h>

namespace mc::engine {
class abstract_object;
class service;
using method_type_info = mc::reflect::method_type_info;

namespace detail {

struct dbus_call {
    dbus_call() = default;
    dbus_call(mc::dbus::message request) : request(std::move(request)) {
    }

    mc::dbus::message request;
    mc::dbus::message response;
};

struct variants_call {
    variants_call(const mc::variants& a, std::string_view interface_name,
                  std::string_view method_name)
        : args(&a), interface_name(interface_name), method_name(method_name) {
    }

    const mc::variants* args;
    mc::variant         result;
    std::string_view    interface_name;
    std::string_view    method_name;
    std::string_view    sender;
    std::string_view    path;
};

using call_info = std::variant<dbus_call, variants_call>;

} // namespace detail

// 用于表示一次请求的处理状态，用于消息分层路由
enum class handler_status {
    accepted, // 已接受（默认行为，消息路由到处理者即已接受）
    ignored,  // 忽略（消息路由到处理者后，处理者主动忽略，消息路由到下一个处理者）
    error,    // 错误（消息路由到处理者后，主动调用 MC_REPLY_ERROR_AND_THROW 抛出错误，或用 MC_THROW 抛出错误）
};

class MC_API context {
public:
    context(service& s, abstract_object& object);
    ~context();

    void        set_arg(std::string_view key, mc::variant value);
    mc::variant get_arg(std::string_view key) const;

    const mc::dict& get_args() const;
    mc::dict&       get_args();
    void            set_args(mc::dict args);

    service&         get_service() const;
    abstract_object& get_object() const;

    detail::call_info& get_call_info();
    void               set_call_info(detail::call_info call_info);

    const method_type_info* get_method() const;
    void                    set_method(const method_type_info* method);

    error_ptr get_error();
    bool      is_error() const;
    void      report_error(std::string_view error_name, mc::dict args = {});
    void      report_error(const error_info& error, mc::dict args = {});

    std::string_view get_path() const;
    std::string_view get_method_name() const;
    std::string_view get_interface_name() const;
    std::string_view get_sender() const;

    [[noreturn]] static void throw_error(std::string_view error_name, mc::dict args = {});
    [[noreturn]] static void throw_error(const error_info& error, mc::dict args = {});

    static context& get_current_context();
    static context* get_current_context_ptr();

    void           ignore();
    void           accept();
    handler_status get_status() const;

private:
    service&                m_service;
    abstract_object&        m_object;
    error_ptr               m_error;
    mc::dict                m_args;
    detail::call_info       m_call_info;
    const method_type_info* m_method{nullptr};
    handler_status          m_status{handler_status::accepted};
};

using context_stack = detail::call_stack<service, context>;

// MC_REPLY_ERROR: 设置上下文调用错误
//
// @param error 错误名称或 error_info 结构表示的消息
// @param args  错误参数
// 注意：
// 1. 这里的错误参数必须是一个已经注册到错误引擎的错误名称，或者是一个 error_info 结构表示的消息
// 2. 我们不建议在调用上下文中返回任意错误消息，这不利于错误消息的统一管理
// 3. 这里仅仅将错误设置到上下文中，函数还是需要正常返回
#define MC_REPLY_ERROR_1(err) \
    mc::engine::context::get_current_context().report_error(err)
#define MC_REPLY_ERROR_2(err, ...)                                                              \
    mc::engine::context::get_current_context()                                                  \
        .report_error(err,                                                                      \
                      mc::log::detail::make_args(                                               \
                          BOOST_PP_SEQ_FOR_EACH(MC_FORMAT_CHECK_ARG, MC_FORMAT_APPLY_ARG_NAMED, \
                                                BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) std::monostate{}))
#define MC_REPLY_ERROR(...)                                                                  \
    BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), MC_REPLY_ERROR_2, \
                 MC_REPLY_ERROR_1)(__VA_ARGS__)

// MC_REPLY_ERROR_AND_THROW: 抛出错误并立即结束当前调用
//
// @param error 错误名称或 error_info 结构表示的消息
// @param args  错误参数
// 注意：
// 1. 这里的错误参数必须是一个已经注册到错误引擎的错误名称，或者是一个 error_info 结构表示的消息
// 2. 我们不建议在调用上下文中返回任意错误消息，这不利于错误消息的统一管理
// 3. 这里会抛出异常，相比 MC_REPLY_ERROR 开销更大，使用这个宏的唯一理由是函数正常返回时某些方法
// 返回值难以构造或构造成本过高，可以利用这个机制快速返回并路由到下一个处理者，否则应该使用 MC_REPLY_ERROR
// 并正常返回
#define MC_REPLY_ERROR_AND_THROW_1(err) \
    mc::engine::context::throw_error(err)
#define MC_REPLY_ERROR_AND_THROW_2(err, ...) \
    mc::engine::context::throw_error(err, mc::dict() __VA_ARGS__)
#define MC_REPLY_ERROR_AND_THROW(...)                                                                  \
    BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), MC_REPLY_ERROR_AND_THROW_2, \
                 MC_REPLY_ERROR_AND_THROW_1)(__VA_ARGS__)

// MC_IGNORE: 忽略当前请求，消息分发机制会继续路由到下一个处理者
//
// 这个宏仅仅标记调用上下文忽略当前请求，函数还是需要正常返回
#define MC_IGNORE() \
    mc::engine::context::get_current_context().ignore()

// MC_IGNORE_AND_THROW: 忽略当前请求并抛出异常，消息分发机制会继续路由到下一个处理者
//
// 与 MC_IGNORE 的差别是这里会抛出异常，消息仍然会路由给下一个处理者，
// 注意：抛出异常的开销远高于函数正常返回，使用这个宏的唯一理由是某些方法返回值难以构造
// 或构造成本过高，可以利用这个机制可以快速返回并路由到下一个处理者，否则应该使用 MC_IGNORE
// 并正常返回
#define MC_IGNORE_AND_THROW() \
    MC_IGNORE();              \
    MC_THROW(mc::method_call_exception, "ignore method call");

} // namespace mc::engine

#endif // MC_ENGINE_CONTEXT_H