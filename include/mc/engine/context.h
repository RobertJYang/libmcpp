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

class MC_API context {
public:
    MC_API context(service& s, abstract_object& object);
    MC_API ~context();

    MC_API void set_arg(std::string_view key, mc::variant value);
    MC_API mc::variant get_arg(std::string_view key) const;

    MC_API const mc::dict& get_args() const;
    MC_API mc::dict& get_args();
    MC_API void      set_args(mc::dict args);

    MC_API service&         get_service() const;
    MC_API abstract_object& get_object() const;

    MC_API detail::call_info& get_call_info();
    MC_API void               set_call_info(detail::call_info call_info);

    MC_API const method_type_info* get_method() const;
    MC_API void                    set_method(const method_type_info* method);

    MC_API error_ptr get_error();
    MC_API bool      is_error() const;
    MC_API void      report_error(std::string_view error_name, mc::dict args = {});

    MC_API std::string_view get_path() const;
    MC_API std::string_view get_method_name() const;
    MC_API std::string_view get_interface_name() const;
    MC_API std::string_view get_sender() const;

    [[noreturn]] static MC_API void throw_error(std::string_view error_name, mc::dict args = {});
    [[noreturn]] static MC_API void throw_error(const error_info& error, mc::dict args = {});

private:
    service&                m_service;
    abstract_object&        m_object;
    error_ptr               m_error;
    mc::mutable_dict        m_args;
    detail::call_info       m_call_info;
    const method_type_info* m_method{nullptr};
};

using context_stack = detail::call_stack<service, context>;

#define MC_REPLY_ERROR(error, args) mc::engine::context::throw_error(error, mc::mutable_dict() args)

} // namespace mc::engine

#endif // MC_ENGINE_CONTEXT_H