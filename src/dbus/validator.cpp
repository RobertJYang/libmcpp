/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 */

#include "mc/dbus/validator.h"
#include <dbus/dbus.h>
#include <mc/engine/utils.h>

namespace mc::dbus {

bool validator::is_valid_interface_name(std::string_view name)
{
    return mc::is_valid_interface_name(name);
}

bool validator::is_valid_member_name(std::string_view name)
{
    return mc::is_identifier(name);
}

bool validator::is_valid_bus_name(std::string_view name)
{
    if (name.empty() || name.size() > mc::max_identifier_length) {
        return false;
    }

    std::size_t pos = 0;

    // 一个点不能作为第一个字符
    if (name[pos] == '.') {
        return false;
    }

    // 如果第一个字符是冒号，则表示这是一个唯一的bus name
    bool is_unique_name = false;
    if (name[pos] == ':') {
        pos++;
        is_unique_name = true;
    }

    int  num_elements  = 1;
    char previous_char = '\0';
    for (; pos < name.size(); pos++) {
        if (name[pos] != '.' && !mc::is_identifier_char(name[pos])) {
            return false;
        }

        if (previous_char == '.' && mc::is_identifier_char(name[pos])) {
            if (!is_unique_name && std::isdigit(name[pos])) {
                return false;
            }

            num_elements++;
        }

        previous_char = name[pos];
    }

    return num_elements >= 2;
}

bool validator::is_valid_error_name(std::string_view errorname)
{
    return is_valid_interface_name(errorname);
}

bool validator::is_message_too_large(std::size_t size)
{
    return size >= maximum_message_size;
}

bool validator::is_valid_path(std::string_view path)
{
    // TODO: 检查路径是否有效
    return true;
}

} // namespace mc::dbus
