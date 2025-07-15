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

#include <dbus/dbus.h>
#include <mc/exception.h>
#include <mc/string.h>

namespace mc::dbus {

struct error : DBusError {
    error();
    error(const error& other);
    ~error();

    error(error&& other) noexcept;

    error& operator=(const error& other);
    error& operator=(error&& other) noexcept;

    bool is_set() const;
    void set_error(std::string_view name, std::string_view message);
    void set_error(std::string_view name, std::string_view message, const mc::dict& args);
    void set_error_const(std::string_view name, std::string_view message);
};

/**
 * 标准DBus错误名称常量
 */
namespace error_names {
// 标准DBus错误名称
constexpr std::string_view failed             = "org.freedesktop.DBus.Error.Failed";
constexpr std::string_view no_memory          = "org.freedesktop.DBus.Error.NoMemory";
constexpr std::string_view service_unknown    = "org.freedesktop.DBus.Error.ServiceUnknown";
constexpr std::string_view name_has_no_owner  = "org.freedesktop.DBus.Error.NameHasNoOwner";
constexpr std::string_view no_reply           = "org.freedesktop.DBus.Error.NoReply";
constexpr std::string_view io_error           = "org.freedesktop.DBus.Error.IOError";
constexpr std::string_view bad_address        = "org.freedesktop.DBus.Error.BadAddress";
constexpr std::string_view not_supported      = "org.freedesktop.DBus.Error.NotSupported";
constexpr std::string_view limits_exceeded    = "org.freedesktop.DBus.Error.LimitsExceeded";
constexpr std::string_view access_denied      = "org.freedesktop.DBus.Error.AccessDenied";
constexpr std::string_view auth_failed        = "org.freedesktop.DBus.Error.AuthFailed";
constexpr std::string_view no_server          = "org.freedesktop.DBus.Error.NoServer";
constexpr std::string_view timeout            = "org.freedesktop.DBus.Error.Timeout";
constexpr std::string_view no_network         = "org.freedesktop.DBus.Error.NoNetwork";
constexpr std::string_view address_in_use     = "org.freedesktop.DBus.Error.AddressInUse";
constexpr std::string_view disconnected       = "org.freedesktop.DBus.Error.Disconnected";
constexpr std::string_view invalid_args       = "org.freedesktop.DBus.Error.InvalidArgs";
constexpr std::string_view file_not_found     = "org.freedesktop.DBus.Error.FileNotFound";
constexpr std::string_view file_exists        = "org.freedesktop.DBus.Error.FileExists";
constexpr std::string_view unknown_method     = "org.freedesktop.DBus.Error.UnknownMethod";
constexpr std::string_view unknown_object     = "org.freedesktop.DBus.Error.UnknownObject";
constexpr std::string_view unknown_interface  = "org.freedesktop.DBus.Error.UnknownInterface";
constexpr std::string_view unknown_property   = "org.freedesktop.DBus.Error.UnknownProperty";
constexpr std::string_view property_read_only = "org.freedesktop.DBus.Error.PropertyReadOnly";
} // namespace error_names

} // namespace mc::dbus

#endif // MC_DBUS_ERROR_H