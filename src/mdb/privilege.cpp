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

std::string get_privilege_str(const std::vector<uint32_t>& privileges)
{
    uint32_t result = 0;

    // 对数组内的元素做按位或
    for (const auto& priv : privileges) {
        result |= priv;
    }

    // 转化为字符串返回
    return mc::string::to_string(result);
}

void validate(uint32_t expected_privilege)
{
    mc::engine::context* ctx_ptr = nullptr;
    try {
        ctx_ptr = &mc::engine::context::get_current_context();
    } catch (const std::exception& e) {
        return;
    }

    auto& ctx = *ctx_ptr;

    auto auth = ctx.auth();
    if (!auth.has_value() || *auth != mc::engine::auth_state::auth_required) {
        return;
    }

    auto priv = ctx.privilege();
    if (!priv.has_value()) {
        MC_THROW(mc::insufficient_privilege_exception,
                 "There are insufficient privileges for the account or credentials associated with the current"
                 " session to perform the requested operation.");
    }

    // 如果期望权限为 0 且 priv 为 ConfigureSelf，则抛出错误
    if (expected_privilege == 0) {
        if (*priv == privilege::configure_self) {
            MC_THROW(mc::password_changed_required_exception,
                     "Indicates that the password for the account provided must be changed before accessing the"
                     " service. The password can be changed with a PATCH to the Password property in the manager"
                     " account resource instance. Implementations that provide a default password for an account"
                     " may require a password change prior to first access to the service.");
        }
        return;
    }

    // 将 priv 与期望权限做逻辑与得到交集
    uint32_t intersection = *priv & expected_privilege;
    // 如果交集不等于期望权限，则抛出错误
    if (intersection != expected_privilege) {
        MC_THROW(mc::insufficient_privilege_exception,
                 "There are insufficient privileges for the account or credentials associated with the current"
                 " session to perform the requested operation.");
    }

    ctx.set_auth(mc::engine::auth_state::no_auth);
}

} // namespace mc::mdb::privilege
