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

#ifndef MC_ENGINE_ERRORS_H
#define MC_ENGINE_ERRORS_H
#include <mc/dbus/error.h>
#include <mc/engine/error.h>

namespace mc::engine::errors {

constexpr error_info failed{mc::dbus::error_names::failed};
constexpr error_info no_memory{mc::dbus::error_names::no_memory};
constexpr error_info service_unknown{mc::dbus::error_names::service_unknown};
constexpr error_info name_has_no_owner{mc::dbus::error_names::name_has_no_owner};
constexpr error_info no_reply{mc::dbus::error_names::no_reply};
constexpr error_info io_error{mc::dbus::error_names::io_error};
constexpr error_info bad_address{mc::dbus::error_names::bad_address};
constexpr error_info not_supported{mc::dbus::error_names::not_supported};
constexpr error_info limits_exceeded{mc::dbus::error_names::limits_exceeded};
constexpr error_info access_denied{mc::dbus::error_names::access_denied};
constexpr error_info auth_failed{mc::dbus::error_names::auth_failed};
constexpr error_info no_server{mc::dbus::error_names::no_server};
constexpr error_info timeout{mc::dbus::error_names::timeout};
constexpr error_info no_network{mc::dbus::error_names::no_network};
constexpr error_info address_in_use{mc::dbus::error_names::address_in_use};
constexpr error_info disconnected{mc::dbus::error_names::disconnected};
constexpr error_info invalid_args{mc::dbus::error_names::invalid_args};
constexpr error_info file_not_found{mc::dbus::error_names::file_not_found};
constexpr error_info file_exists{mc::dbus::error_names::file_exists};
constexpr error_info unknown_method{mc::dbus::error_names::unknown_method};
constexpr error_info unknown_object{mc::dbus::error_names::unknown_object};
constexpr error_info unknown_interface{mc::dbus::error_names::unknown_interface};
constexpr error_info unknown_property{mc::dbus::error_names::unknown_property};
constexpr error_info property_read_only{mc::dbus::error_names::property_read_only};
constexpr error_info timed_out{mc::dbus::error_names::timed_out};

} // namespace mc::engine::errors

#define REGISTER_ERRORS(REGISTER)                                                                  \
    REGISTER(mc::engine::errors::failed)                                                           \
    REGISTER(mc::engine::errors::no_memory)                                                        \
    REGISTER(mc::engine::errors::service_unknown)                                                  \
    REGISTER(mc::engine::errors::name_has_no_owner)                                                \
    REGISTER(mc::engine::errors::no_reply)                                                         \
    REGISTER(mc::engine::errors::io_error)                                                         \
    REGISTER(mc::engine::errors::bad_address)                                                      \
    REGISTER(mc::engine::errors::not_supported)                                                    \
    REGISTER(mc::engine::errors::limits_exceeded)                                                  \
    REGISTER(mc::engine::errors::access_denied)                                                    \
    REGISTER(mc::engine::errors::auth_failed)                                                      \
    REGISTER(mc::engine::errors::no_server)                                                        \
    REGISTER(mc::engine::errors::timeout)                                                          \
    REGISTER(mc::engine::errors::no_network)                                                       \
    REGISTER(mc::engine::errors::address_in_use)                                                   \
    REGISTER(mc::engine::errors::disconnected)                                                     \
    REGISTER(mc::engine::errors::invalid_args)                                                     \
    REGISTER(mc::engine::errors::file_not_found)                                                   \
    REGISTER(mc::engine::errors::file_exists)                                                      \
    REGISTER(mc::engine::errors::unknown_method)                                                   \
    REGISTER(mc::engine::errors::unknown_object)                                                   \
    REGISTER(mc::engine::errors::unknown_interface)                                                \
    REGISTER(mc::engine::errors::unknown_property)                                                 \
    REGISTER(mc::engine::errors::property_read_only)                                               \
    REGISTER(mc::engine::errors::timed_out)

#endif // MC_ENGINE_ERRORS_H
