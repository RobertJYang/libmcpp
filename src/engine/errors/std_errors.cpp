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

#include <mc/engine/errors/std_errors.h>

namespace mc::engine::errors {
// TODO:: 暂时将错误的 fmt 字符串用错误名字，后续应该替换成错误格式化文本，有些错误需要填充错误参数

REGISTER_CONST_ERROR(failed, mc::dbus::error_names::failed);
REGISTER_CONST_ERROR(no_memory, mc::dbus::error_names::no_memory);
REGISTER_CONST_ERROR(service_unknown, mc::dbus::error_names::service_unknown);
REGISTER_CONST_ERROR(name_has_no_owner, mc::dbus::error_names::name_has_no_owner);
REGISTER_CONST_ERROR(no_reply, mc::dbus::error_names::no_reply);
REGISTER_CONST_ERROR(io_error, mc::dbus::error_names::io_error);
REGISTER_CONST_ERROR(bad_address, mc::dbus::error_names::bad_address);
REGISTER_CONST_ERROR(not_supported, mc::dbus::error_names::not_supported);
REGISTER_CONST_ERROR(limits_exceeded, mc::dbus::error_names::limits_exceeded);
REGISTER_CONST_ERROR(access_denied, mc::dbus::error_names::access_denied);
REGISTER_CONST_ERROR(auth_failed, mc::dbus::error_names::auth_failed);
REGISTER_CONST_ERROR(no_server, mc::dbus::error_names::no_server);
REGISTER_CONST_ERROR(timeout, mc::dbus::error_names::timeout);
REGISTER_CONST_ERROR(no_network, mc::dbus::error_names::no_network);
REGISTER_CONST_ERROR(address_in_use, mc::dbus::error_names::address_in_use);
REGISTER_CONST_ERROR(disconnected, mc::dbus::error_names::disconnected);
REGISTER_CONST_ERROR(invalid_args, mc::dbus::error_names::invalid_args);
REGISTER_CONST_ERROR(file_not_found, mc::dbus::error_names::file_not_found);
REGISTER_CONST_ERROR(file_exists, mc::dbus::error_names::file_exists);
REGISTER_CONST_ERROR(unknown_method, mc::dbus::error_names::unknown_method);
REGISTER_CONST_ERROR(unknown_object, mc::dbus::error_names::unknown_object);
REGISTER_CONST_ERROR(unknown_interface, mc::dbus::error_names::unknown_interface, "Unknown interface ${interface}");
REGISTER_CONST_ERROR(unknown_property, mc::dbus::error_names::unknown_property, "Unknown property ${property}");
REGISTER_CONST_ERROR(property_read_only, mc::dbus::error_names::property_read_only);

} // namespace mc::engine::errors
