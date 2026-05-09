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

#include <functional>
#include <mc/dict.h>
#include <mc/engine/call_stack.h>
#include <mc/error_engine.h>
#include <mc/memory.h>
#include <mc/pp.h>
#include <mc/reflect/signature_helper.h>
#include <mc/string_view.h>
#include <mc/variant.h>
#include <optional>
#include <type_traits>
#include <utility>

namespace mc::engine {
class abstract_object;
class service;
using method_type_info = mc::reflect::method_type_info;

// 用于表示一次请求的处理状态，用于消息分层路由
enum class handler_status {
    accepted, // 已接受（默认行为，消息路由到处理者即已接受）
    ignored,  // 忽略（消息路由到处理者后，处理者主动忽略，消息路由到下一个处理者）
    error,    // 错误（消息路由到处理者后，主动调用 MC_REPLY_ERROR_AND_THROW 抛出错误，或用 MC_THROW 抛出错误）
};

enum class auth_state {
    no_auth       = 0,
    auth_required = 1,
};

// context 首参在「反射/introspection/兼容外部 busctl」中的签名描述，由 MC_CONTEXT_SIGNATURE 进程级一次性决定。
//   - "0"           ：none —— 反射仍可展示默认 a{ss} 占位（具体见 context_signature_string()）
//   - "1" / 未设置  ：a{ss}（默认，供外部文档与兼容入站）
//   - "2"           ：a{sv}
// 框架内部 method_call 首参 dict 内容与线型：`context::snapshot_for_internal_rpc()` +
// `internal_context_wire_signature_string()`（固定 a{sv}，条目为 variant 原生类型）。
enum class context_signature_mode {
    none = 0,
    a_ss = 1,
    a_sv = 2,
};

MC_API context_signature_mode current_context_signature_mode() noexcept;
MC_API mc::string_view context_signature_string() noexcept;

// 框架内部转发（mdb proxy_object、std_interface 递归取属性等）在 method_call 上使用的上下文 dict 签名，固定为 a{sv}。
// 反射与 dbus introspection 仍通过 context_signature_string() 给出 a{ss}/a{sv}/none（供外部工具与兼容入站）。
MC_API mc::string_view internal_context_wire_signature_string() noexcept;

namespace detail {

struct variants_call {
    variants_call() = default;

    variants_call(const mc::variants& a, mc::string_view interface_name, mc::string_view method_name)
        : args(&a), interface_name(interface_name), method_name(method_name)
    {}

    const mc::variants* args;
    mc::variant         result;
    mc::string_view     interface_name;
    mc::string_view     method_name;
    mc::string_view     sender;
    mc::string_view     path;
};

using call_info = variants_call;

struct context_state : public mc::enable_shared_from_this<context_state> {
    std::optional<auth_state> auth;
    std::optional<uint32_t>   privilege;
    std::optional<mc::string> username;
    std::optional<mc::string> client_addr;
    std::optional<mc::string> interface_name;
    // 与固定语义字段同属「可参与跨进程转发」的扩展上下文。
    mc::dict extensions;
    // 仅调用栈/进程内生效的控制项，默认不参与跨进程转发。
    mc::dict local;
};

struct context_frame {
    service*                service_ptr{nullptr};
    abstract_object*        object_ptr{nullptr};
    error_ptr               error;
    call_info               call_info;
    const method_type_info* method{nullptr};
    handler_status          status{handler_status::accepted};
};

} // namespace detail

class MC_API context {
public:
    context();
    context(service& s, abstract_object& object);
    ~context();

    context(const context& rhs);
    context(context&& rhs) noexcept;
    context& operator=(const context& rhs);
    context& operator=(context&& rhs) noexcept;

    static context& get_current_context();
    static context* get_current_context_ptr();
    static context  current();

    context clone() const;

    std::optional<auth_state> auth() const;
    void                      set_auth(std::optional<auth_state> value);

    std::optional<uint32_t> privilege() const;
    void                    set_privilege(std::optional<uint32_t> value);

    std::optional<mc::string> username() const;
    void                      set_username(std::optional<mc::string> value);

    std::optional<mc::string> client_addr() const;
    void                      set_client_addr(std::optional<mc::string> value);

    std::optional<mc::string> interface_name() const;
    void                      set_interface_name(std::optional<mc::string> value);

    // 可转发扩展键值：与固定语义字段一同参与序列化/跨进程传播（如 proxy_object）。
    mc::dict&       extensions();
    const mc::dict& extensions() const;

    // 调用栈内本地控制项：不参与跨进程转发。
    mc::dict&       local();
    const mc::dict& local() const;

    void        set_arg(mc::string_view key, mc::variant value);
    mc::variant get_arg(mc::string_view key) const;

    mc::dict get_args() const;
    void     set_args(const mc::dict& args);

    service&         get_service() const;
    abstract_object& get_object() const;

    service*         get_service_ptr() const noexcept;
    abstract_object* get_object_ptr() const noexcept;

    detail::call_info& get_call_info();
    void               set_call_info(detail::call_info call_info);

    const method_type_info* get_method() const;
    const method_type_info* get_method_ptr() const noexcept;
    void                    set_method(const method_type_info* method);

    error_ptr get_error();
    bool      is_error() const;
    void      report_error(mc::string_view error_name, mc::dict args = {});
    void      report_error(const error_info& error, mc::dict args = {});

    mc::string_view get_path() const;
    mc::string_view get_method_name() const;
    mc::string_view get_interface_name() const;
    mc::string_view get_sender() const;

    [[noreturn]] static void throw_error(mc::string_view error_name, mc::dict args = {});
    [[noreturn]] static void throw_error(const error_info& error, mc::dict args = {});

    void           ignore();
    void           accept();
    handler_status get_status() const;

    // 供框架内部拼接 method_call 首参：extensions() + 可转发语义字段，保持 variant 原生类型；不含 local()。
    mc::dict snapshot_for_internal_rpc() const;

private:
    explicit context(mc::shared_ptr<detail::context_state> state);

    mc::shared_ptr<detail::context_state>&       ensure_state();
    const mc::shared_ptr<detail::context_state>& ensure_state() const;
    detail::context_frame&                       ensure_frame();
    const detail::context_frame*                 frame_ptr() const noexcept;

    mc::variant export_auth_arg() const;
    mc::variant export_privilege_arg() const;

    mc::shared_ptr<detail::context_state> m_state;
    std::optional<detail::context_frame>  m_frame;
};

using context_stack = detail::call_stack<service, context>;

MC_API context make_method_context_arg();

template <typename F>
decltype(auto) with_context(context ctx, F&& fn)
{
    context_stack::context guard(ctx.get_service_ptr(), ctx);
    return std::invoke(std::forward<F>(fn));
}

template <typename F>
auto bind_context(F&& fn)
{
    return bind_context(context::current(), std::forward<F>(fn));
}

template <typename F>
auto bind_context(context ctx, F&& fn)
{
    using function_type = std::decay_t<F>;

    return [ctx = std::move(ctx), fn = function_type(std::forward<F>(fn))](auto&&... args) mutable -> decltype(auto) {
        return with_context(ctx, [&]() -> decltype(auto) {
            return std::invoke(fn, std::forward<decltype(args)>(args)...);
        });
    };
}

// MC_REPLY_ERROR: 设置上下文调用错误
//
// @param error 错误名称或 error_info 结构表示的消息
// @param args  错误参数
// 注意：
// 1. 这里的错误参数必须是一个已经注册到错误引擎的错误名称，或者是一个 error_info 结构表示的消息
// 2. 我们不建议在调用上下文中返回任意错误消息，这不利于错误消息的统一管理
// 3. 这里仅仅将错误设置到上下文中，函数还是需要正常返回
#define MC_REPLY_ERROR_1(err) mc::engine::context::get_current_context().report_error(err)
#define MC_REPLY_ERROR_2(err, ...)                                                                                     \
    mc::engine::context::get_current_context().report_error(                                                           \
        err, mc::log::detail::make_args(MC_PP_SEQ_FOR_EACH(MC_FORMAT_CHECK_ARG, MC_FORMAT_APPLY_ARG_NAMED,             \
                                                           MC_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) std::monostate{}))
#define MC_REPLY_ERROR(...)                                                                                            \
    MC_PP_IIF(MC_PP_GREATER(MC_PP_VARIADIC_SIZE(__VA_ARGS__), 1), MC_REPLY_ERROR_2, MC_REPLY_ERROR_1)                  \
    (__VA_ARGS__)

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
#define MC_REPLY_ERROR_AND_THROW_1(err)      mc::engine::context::throw_error(err)
#define MC_REPLY_ERROR_AND_THROW_2(err, ...) mc::engine::context::throw_error(err, mc::dict() __VA_ARGS__)
#define MC_REPLY_ERROR_AND_THROW(...)                                                                                  \
    MC_PP_IIF(MC_PP_GREATER(MC_PP_VARIADIC_SIZE(__VA_ARGS__), 1), MC_REPLY_ERROR_AND_THROW_2,                          \
              MC_REPLY_ERROR_AND_THROW_1)                                                                              \
    (__VA_ARGS__)

// MC_IGNORE: 忽略当前请求，消息分发机制会继续路由到下一个处理者
//
// 这个宏仅仅标记调用上下文忽略当前请求，函数还是需要正常返回
#define MC_IGNORE() mc::engine::context::get_current_context().ignore()

// MC_IGNORE_AND_THROW: 忽略当前请求并抛出异常，消息分发机制会继续路由到下一个处理者
//
// 与 MC_IGNORE 的差别是这里会抛出异常，消息仍然会路由给下一个处理者，
// 注意：抛出异常的开销远高于函数正常返回，使用这个宏的唯一理由是某些方法返回值难以构造
// 或构造成本过高，可以利用这个机制可以快速返回并路由到下一个处理者，否则应该使用 MC_IGNORE
// 并正常返回
#define MC_IGNORE_AND_THROW()                                                                                          \
    MC_IGNORE();                                                                                                       \
    MC_THROW(mc::method_call_exception, "ignore method call");

} // namespace mc::engine

namespace mc {

inline void to_variant(const mc::engine::context& ctx, mc::variant& value)
{
    value = ctx.get_args();
}

inline void from_variant(const mc::variant& value, mc::engine::context& ctx)
{
    if (!value.is_dict()) {
        MC_THROW(mc::bad_cast_exception, "context expects dict variant, actual ${actual}",
                 ("actual", value.get_type_name()));
    }
    ctx = mc::engine::context();
    ctx.set_args(value.as_dict());
}

} // namespace mc

namespace mc::reflect::detail {

template <>
struct signature_helper<mc::engine::context> {
    static void apply(mc::string& sig)
    {
        sig += mc::engine::context_signature_string();
    }
};

} // namespace mc::reflect::detail

#endif // MC_ENGINE_CONTEXT_H