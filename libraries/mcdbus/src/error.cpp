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

#include <mc/dbus/error.h>
#include <mc/fmt/format_dict.h>

namespace mc::dbus {

error::error()
{
    dbus_error_init(this);
}

error::error(const error& other)
{
    dbus_error_init(this);
    if (other.is_set()) {
        dbus_set_error(this, other.name, "%s", other.message);
    }
}

error& error::operator=(const error& other)
{
    if (this != &other) {
        dbus_error_free(this);
        dbus_error_init(this);
        if (other.is_set()) {
            dbus_set_error(this, other.name, "%s", other.message);
        }
    }
    return *this;
}

error::~error()
{
    dbus_error_free(this);
}

error::error(error&& other) noexcept
{
    // dbus_move_error 要求目标对象必须是未设置错误的
    // 先初始化 this 为未设置状态
    dbus_error_init(this);
    dbus_move_error(&other, this);
}

error& error::operator=(error&& other) noexcept
{
    // dbus_move_error 要求目标对象必须是未设置错误的
    // 先清理并初始化 this 为未设置状态
    dbus_error_free(this);
    dbus_error_init(this);
    dbus_move_error(&other, this);
    return *this;
}

bool error::is_set() const
{
    return dbus_error_is_set(this);
}

void error::set_error(std::string_view name, std::string_view message)
{
    dbus_set_error(this, name.data(), "%s", message.data());
}

void error::set_error(std::string_view name, std::string_view message, const mc::dict& args)
{
    if (args.empty()) {
        dbus_set_error(this, name.data(), "%s", message.data());
    } else {
        auto msg = mc::format_dict(message, args);
        dbus_set_error(this, name.data(), "%s", msg.c_str());
    }
}

void error::set_error_const(std::string_view name, std::string_view message)
{
    dbus_set_error_const(this, name.data(), message.data());
}

} // namespace mc::dbus