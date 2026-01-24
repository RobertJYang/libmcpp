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

#ifndef MC_MDB_PRIVILEGE_H
#define MC_MDB_PRIVILEGE_H

#include <mc/common.h>
#include <mc/variant.h>
#include <string>
#include <vector>

namespace mc::mdb::privilege {

/**
 * @brief 认证状态枚举
 */
enum class auth_state {
    no_auth       = 0, // 无需认证
    auth_required = 1  // 需要认证
};

/**
 * @brief 权限枚举
 */
enum privilege : uint32_t {
    read_only      = 1,   // 只读
    diagnose_mgmt  = 2,   // 诊断管理
    security_mgmt  = 4,   // 安全管理
    basic_setting  = 8,   // 基础设置
    user_mgmt      = 16,  // 用户管理
    power_mgmt     = 32,  // 电源管理
    vmm_mgmt       = 64,  // VMM管理
    kvm_mgmt       = 128, // KVM管理
    configure_self = 256  // 自我配置
};

/**
 * @brief 通过权限数组获取权限字符串
 * @param privileges [in] 权限数组
 * @return 返回权限值的字符串表示
 */
MC_API std::string get_privilege_str(const std::vector<uint32_t>& privileges);

/**
 * @brief 验证权限
 * @param expected_privilege [in] 期望的权限值
 */
MC_API void validate(uint32_t expected_privilege);

} // namespace mc::mdb::privilege

#endif // MC_MDB_PRIVILEGE_H
