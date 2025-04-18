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

#ifndef MC_DBUS_ERROR_H
#define MC_DBUS_ERROR_H

#include <mc/exception.h>
#include <string>

namespace mc::dbus {

// DBus 模块错误码定义 (使用 1000-1999 范围)
enum dbus_error_code {
    // 基础错误 (1000-1099)
    dbus_common_error_code = 1000,
    method_invocation_error_code,
    invalid_path_error_code,
    invalid_signature_error_code,
    invalid_message_error_code,
    invalid_connection_error_code,

    // 传输和会话错误 (1100-1199)
    transport_error_code = 1100,
    no_memory_error_code,
    invalid_args_error_code,
    timeout_error_code,

    // 数据处理错误 (1200-1299)
    marshaling_error_code = 1200,
    demarshaling_error_code,
    invalid_type_cast_error_code,
    no_reply_error_code,

    // 认证和访问错误 (1300-1399)
    authentication_error_code = 1300,
    invalid_object_path_error_code,
    invalid_interface_name_error_code,
    invalid_service_name_error_code,

    // 其他错误 (1400-1499)
    not_supported_error_code = 1400,
    property_error_code,
    dispatcher_error_code,
    invalid_thread_error_code
};

// 基础 DBus 错误类
class dbus_exception : public mc::exception {
public:
    dbus_exception(int64_t            code       = dbus_common_error_code,
                   const std::string& name_value = "dbus_exception",
                   const std::string& what_value = "未指定异常")
        : mc::exception(code, name_value, what_value) {
    }

    dbus_exception(mc::log::message&& msg, int64_t code = dbus_common_error_code,
                   const std::string& name_value = "dbus_exception",
                   const std::string& what_value = "未指定异常")
        : mc::exception(std::move(msg), code, name_value, what_value) {
    }

    dbus_exception(mc::log::messages&& msgs, int64_t code = dbus_common_error_code,
                   const std::string& name_value = "dbus_exception",
                   const std::string& what_value = "未指定异常")
        : mc::exception(std::move(msgs), code, name_value, what_value) {
    }

    dbus_exception(const mc::log::messages& msgs, int64_t code = dbus_common_error_code,
                   const std::string& name_value = "dbus_exception",
                   const std::string& what_value = "未指定异常")
        : mc::exception(msgs, code, name_value, what_value) {
    }

    virtual ~dbus_exception() noexcept {
    }
};

#define MC_DEFINE_DBUS_EXCEPTION_CLASS(class_name, code_enum_value, default_msg)                   \
    MC_DEFINE_EXCEPTION_CLASS_BASE(class_name, code_enum_value, default_msg, #class_name,          \
                                   dbus_exception)

// 常见的DBus错误定义
MC_DEFINE_DBUS_EXCEPTION_CLASS(dbus_common_exception, dbus_common_error_code, "DBus错误");
MC_DEFINE_DBUS_EXCEPTION_CLASS(method_invocation_exception, method_invocation_error_code,
                               "方法调用错误");
MC_DEFINE_DBUS_EXCEPTION_CLASS(invalid_path_exception, invalid_path_error_code, "无效的路径");
MC_DEFINE_DBUS_EXCEPTION_CLASS(invalid_signature_exception, invalid_signature_error_code,
                               "无效的签名");
MC_DEFINE_DBUS_EXCEPTION_CLASS(invalid_message_exception, invalid_message_error_code, "无效的消息");
MC_DEFINE_DBUS_EXCEPTION_CLASS(invalid_connection_exception, invalid_connection_error_code,
                               "无效的连接");
MC_DEFINE_DBUS_EXCEPTION_CLASS(invalid_transport_exception, transport_error_code, "无效的传输");
MC_DEFINE_DBUS_EXCEPTION_CLASS(no_memory_exception, no_memory_error_code, "内存不足");
MC_DEFINE_DBUS_EXCEPTION_CLASS(invalid_args_exception, invalid_args_error_code, "无效的参数");
MC_DEFINE_DBUS_EXCEPTION_CLASS(dbus_timeout_exception, timeout_error_code, "超时");
MC_DEFINE_DBUS_EXCEPTION_CLASS(marshaling_exception, marshaling_error_code, "消息序列化错误");
MC_DEFINE_DBUS_EXCEPTION_CLASS(demarshaling_exception, demarshaling_error_code, "消息反序列化错误");
MC_DEFINE_DBUS_EXCEPTION_CLASS(invalid_type_cast_exception, invalid_type_cast_error_code,
                               "类型转换错误");
MC_DEFINE_DBUS_EXCEPTION_CLASS(no_reply_exception, no_reply_error_code, "无响应");
MC_DEFINE_DBUS_EXCEPTION_CLASS(authentication_exception, authentication_error_code, "认证错误");
MC_DEFINE_DBUS_EXCEPTION_CLASS(invalid_object_path_exception, invalid_object_path_error_code,
                               "无效的对象路径");
MC_DEFINE_DBUS_EXCEPTION_CLASS(invalid_interface_name_exception, invalid_interface_name_error_code,
                               "无效的接口名称");
MC_DEFINE_DBUS_EXCEPTION_CLASS(invalid_service_name_exception, invalid_service_name_error_code,
                               "无效的服务名称");
MC_DEFINE_DBUS_EXCEPTION_CLASS(not_supported_exception, not_supported_error_code, "不支持的操作");
MC_DEFINE_DBUS_EXCEPTION_CLASS(property_exception, property_error_code, "属性错误");
MC_DEFINE_DBUS_EXCEPTION_CLASS(dispatcher_exception, dispatcher_error_code, "调度器错误");
MC_DEFINE_DBUS_EXCEPTION_CLASS(invalid_thread_exception, invalid_thread_error_code, "无效的线程");

/**
 * 标准DBus错误名称常量
 */
namespace error_names {
// 标准DBus错误名称
const std::string_view failed             = "org.freedesktop.DBus.Error.Failed";
const std::string_view no_memory          = "org.freedesktop.DBus.Error.NoMemory";
const std::string_view service_unknown    = "org.freedesktop.DBus.Error.ServiceUnknown";
const std::string_view name_has_no_owner  = "org.freedesktop.DBus.Error.NameHasNoOwner";
const std::string_view no_reply           = "org.freedesktop.DBus.Error.NoReply";
const std::string_view io_error           = "org.freedesktop.DBus.Error.IOError";
const std::string_view bad_address        = "org.freedesktop.DBus.Error.BadAddress";
const std::string_view not_supported      = "org.freedesktop.DBus.Error.NotSupported";
const std::string_view limits_exceeded    = "org.freedesktop.DBus.Error.LimitsExceeded";
const std::string_view access_denied      = "org.freedesktop.DBus.Error.AccessDenied";
const std::string_view auth_failed        = "org.freedesktop.DBus.Error.AuthFailed";
const std::string_view no_server          = "org.freedesktop.DBus.Error.NoServer";
const std::string_view timeout            = "org.freedesktop.DBus.Error.Timeout";
const std::string_view no_network         = "org.freedesktop.DBus.Error.NoNetwork";
const std::string_view address_in_use     = "org.freedesktop.DBus.Error.AddressInUse";
const std::string_view disconnected       = "org.freedesktop.DBus.Error.Disconnected";
const std::string_view invalid_args       = "org.freedesktop.DBus.Error.InvalidArgs";
const std::string_view file_not_found     = "org.freedesktop.DBus.Error.FileNotFound";
const std::string_view file_exists        = "org.freedesktop.DBus.Error.FileExists";
const std::string_view unknown_method     = "org.freedesktop.DBus.Error.UnknownMethod";
const std::string_view unknown_object     = "org.freedesktop.DBus.Error.UnknownObject";
const std::string_view unknown_interface  = "org.freedesktop.DBus.Error.UnknownInterface";
const std::string_view unknown_property   = "org.freedesktop.DBus.Error.UnknownProperty";
const std::string_view property_read_only = "org.freedesktop.DBus.Error.PropertyReadOnly";
const std::string_view timed_out          = "org.freedesktop.DBus.Error.TimedOut";
} // namespace error_names

} // namespace mc::dbus

#endif // MC_DBUS_ERROR_H