/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/engine/dispatcher.h>

#include <mc/engine/errors/std_errors.h>
#include <mc/engine/object.h>
#include <mc/engine/service.h>
#include <mc/error.h>
#include <mc/error_engine.h>

namespace mc::engine {
namespace {

class scoped_last_error {
public:
    scoped_last_error() : m_previous(mc::error_engine::get_instance().set_last_error(mc::error_ptr{}))
    {}

    ~scoped_last_error()
    {
        mc::error_engine::get_instance().set_last_error(std::move(m_previous));
    }

    mc::error_ptr current() const
    {
        return mc::error_engine::get_instance().last_error();
    }

private:
    mc::error_ptr m_previous;
};

message make_response(const message& request, message_type type, mc::shared_ptr<const abstract_payload> body)
{
    message response;
    response.header.type           = type;
    response.header.destination    = request.header.sender;
    response.header.sender         = request.header.destination;
    response.header.path           = request.header.path;
    response.header.interface_name = request.header.interface_name;
    response.header.member_name    = request.header.member_name;
    response.header.serial         = request.header.serial + 1;
    response.header.reply_serial   = request.header.serial;
    response.body                  = std::move(body);
    return response;
}

message make_error_response(const message& request, mc::string_view name, mc::string_view text)
{
    auto body             = make_payload<error_payload>(name, text);
    auto rsp              = make_response(request, message_type::error, std::move(body));
    rsp.header.error_name = mc::string(name);
    return rsp;
}

message make_engine_error_response(const message& request, const mc::error_info& info, mc::dict args = {})
{
    auto err = mc::error_engine::get_instance().report_error(info, std::move(args));
    return make_error_response(request, err->get_name(), err->get_message());
}

message make_exception_response(const message& request, const mc::exception& ex)
{
    auto err = mc::error::from_exception(ex);
    if (err && err->is_set()) {
        return make_error_response(request, err->get_name(), err->get_message());
    }
    return make_error_response(request, errors::internal_error.name, ex.top_message());
}

message make_reported_error_response(const message& request, const mc::error_ptr& err)
{
    if (err && err->is_set()) {
        return make_error_response(request, err->get_name(), err->get_message());
    }
    return {};
}

message dispatch_method_call(const service& svc, const message& request, const method_call_payload& payload)
{
    auto& table = svc.get_object_table();
    auto  it    = table.find<by_path>(request.header.path);
    if (it.is_end()) {
        return make_engine_error_response(request, errors::unknown_object, {{"path", request.header.path}});
    }

    abstract_object& obj = const_cast<abstract_object&>(*it);
    if (!obj.has_interface(request.header.interface_name)) {
        return make_engine_error_response(request, errors::unknown_interface,
                                          {{"interface", request.header.interface_name}});
    }
    if (!obj.has_method(request.header.member_name, request.header.interface_name)) {
        return make_engine_error_response(request, errors::unknown_method, {{"method", request.header.member_name}});
    }

    auto info  = obj.get_metadata().get_method_info(request.header.member_name, request.header.interface_name);
    auto value = obj.invoke(request.header.member_name, payload.args, request.header.interface_name);
    auto body  = make_payload<method_return_payload>(
        std::move(value), info.item == nullptr ? mc::string_view{} : info.item->get_result_signature());
    return make_response(request, message_type::method_return, std::move(body));
}

} // namespace

message dispatch(const service& svc, const message& request)
{
    if (request.header.type != message_type::method_call) {
        return make_engine_error_response(request, errors::invalid_args);
    }

    auto* payload = request.try_as<method_call_payload>();
    if (payload == nullptr) {
        return make_engine_error_response(request, errors::invalid_args);
    }

    scoped_last_error last_error_scope;

    try {
        return dispatch_method_call(svc, request, *payload);
    } catch (const mc::exception& ex) {
        auto reported = make_reported_error_response(request, last_error_scope.current());
        if (reported.header.type == message_type::error) {
            return reported;
        }
        return make_exception_response(request, ex);
    } catch (const std::exception& ex) {
        auto reported = make_reported_error_response(request, last_error_scope.current());
        if (reported.header.type == message_type::error) {
            return reported;
        }
        return make_error_response(request, errors::internal_error.name, ex.what());
    }
}

} // namespace mc::engine
