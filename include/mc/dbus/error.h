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
#include <string>

namespace mc::dbus {

struct error : DBusError {
    error() {
        dbus_error_init(this);
    }

    ~error() {
        dbus_error_free(this);
    }

    error(const error& other)            = delete;
    error& operator=(const error& other) = delete;

    error(error&& other) noexcept {
        dbus_move_error(&other, this);
    }

    error& operator=(error&& other) noexcept {
        dbus_move_error(&other, this);
        return *this;
    }

    bool is_set() const {
        return dbus_error_is_set(this);
    }

    template <typename... Args>
    void set_error(std::string_view name, Args&&... args) {
        dbus_set_error(this, name.data(), std::forward<Args>(args)...);
    }

    void set_error_const(std::string_view name, std::string_view message) {
        dbus_set_error_const(this, name.data(), message.data());
    }
};

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