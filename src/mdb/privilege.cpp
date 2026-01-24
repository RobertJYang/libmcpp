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

#include <mc/mdb/privilege.h>

#include <mc/engine/context.h>
#include <mc/exception.h>
#include <mc/string.h>

namespace mc::mdb::privilege {

std::string get_privilege_str(const std::vector<uint32_t>& privileges) {
    uint32_t result = 0;

    // 对数组内的元素做按位或
    for (const auto& priv : privileges) {
        result |= priv;
    }

    // 转化为字符串返回
    return mc::string::to_string(result);
}

void validate(uint32_t expected_privilege) {
    // 获取上下文
    mc::engine::context* ctx_ptr = nullptr;
    try {
        ctx_ptr = &mc::engine::context::get_current_context();
    } catch (const std::exception& e) {
        return;
    }

    auto& ctx = *ctx_ptr;

    // 获取上下文中的 Auth 字段
    auto auth_var = ctx.get_arg("Auth");
    // 如果 Auth 字段不是字符串，直接返回
    if (!auth_var.is_string()) {
        return;
    }

    auto auth_str = auth_var.as_string();
    // 与 auth_required 比较，不等则不做处理直接返回
    if (auth_str != std::to_string(static_cast<int>(auth_state::auth_required))) {
        return;
    }

    // 获取上下文中的 Privilege 字段
    auto privilege_var = ctx.get_arg("Privilege");
    // 如果 Privilege 字段不是字符串，直接返回
    if (!privilege_var.is_string()) {
        MC_THROW(mc::insufficient_privilege_exception,
                 "There are insufficient privileges for the account or credentials associated with the current"
                 " session to perform the requested operation.");
    }

    // 将 Privilege 字段转化为整型 priv
    uint32_t priv = 0;
    try {
        priv = mc::string::to_number<uint32_t>(privilege_var.as_string());
    } catch (const std::exception& e) {
        MC_THROW(mc::insufficient_privilege_exception,
                 "There are insufficient privileges for the account or credentials associated with the current"
                 " session to perform the requested operation.");
    }

    // 如果期望权限为 0 且 priv 为 ConfigureSelf，则抛出错误
    if (expected_privilege == 0) {
        if (priv == privilege::configure_self) {
            MC_THROW(mc::password_changed_required_exception,
                     "Indicates that the password for the account provided must be changed before accessing the"
                     " service. The password can be changed with a PATCH to the Password property in the manager"
                     " account resource instance. Implementations that provide a default password for an account"
                     " may require a password change prior to first access to the service.");
        }
        return;
    }

    // 将 priv 与期望权限做逻辑与得到交集
    uint32_t intersection = priv & expected_privilege;
    // 如果交集不等于期望权限，则抛出错误
    if (intersection != expected_privilege) {
        MC_THROW(mc::insufficient_privilege_exception,
                 "There are insufficient privileges for the account or credentials associated with the current"
                 " session to perform the requested operation.");
    }

    // 将上下文中的 Auth 字段置为 no_auth
    ctx.set_arg("Auth", std::to_string(static_cast<int>(auth_state::no_auth)));
}

} // namespace mc::mdb::privilege
