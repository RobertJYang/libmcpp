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

#include <mc/engine/base.h>
#include <mc/engine/context.h>
#include <mc/engine/object.h>
#include <mc/engine/service.h>
#include <mc/exception.h>
#include <mc/string.h>
#include <mc/variant.h>

#include <gtest/gtest.h>

using namespace mc::mdb::privilege;

namespace {

// 测试用的服务类
class test_service : public mc::engine::service {
public:
    test_service()
        : mc::engine::service("org.test.privilege.service")
    {
    }

    bool start() override
    {
        return true;
    }

    bool stop() override
    {
        return true;
    }
};

// 测试用的对象类
class test_object : public mc::engine::object<test_object> {
public:
    MC_OBJECT(test_object, "TestObject", "/org/test/privilege")

    void init()
    {
        set_object_name("TestObject");
        set_object_path("/org/test/privilege");
    }
};

} // namespace

MC_REFLECT(test_object, ())

// ========== 测试 get_privilege_str 函数 ==========

TEST(privilege_test, get_privilege_str_empty_array)
{
    std::vector<uint32_t> privileges;
    auto                  result = get_privilege_str(privileges);
    EXPECT_EQ(result, "0");
}

TEST(privilege_test, get_privilege_str_single_privilege)
{
    std::vector<uint32_t> privileges = {privilege::read_only};
    auto                  result     = get_privilege_str(privileges);
    EXPECT_EQ(result, "1");
}

TEST(privilege_test, get_privilege_str_multiple_privileges)
{
    std::vector<uint32_t> privileges = {
        privilege::read_only,
        privilege::user_mgmt,
        privilege::power_mgmt};
    auto result = get_privilege_str(privileges);
    // 1 | 16 | 32 = 49
    EXPECT_EQ(result, "49");
}

TEST(privilege_test, get_privilege_str_all_privileges)
{
    std::vector<uint32_t> privileges = {
        privilege::read_only,
        privilege::diagnose_mgmt,
        privilege::security_mgmt,
        privilege::basic_setting,
        privilege::user_mgmt,
        privilege::power_mgmt,
        privilege::vmm_mgmt,
        privilege::kvm_mgmt,
        privilege::configure_self};
    auto result = get_privilege_str(privileges);
    // 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 = 511
    EXPECT_EQ(result, "511");
}

TEST(privilege_test, get_privilege_str_duplicate_privileges)
{
    std::vector<uint32_t> privileges = {
        privilege::read_only,
        privilege::read_only,
        privilege::user_mgmt};
    auto result = get_privilege_str(privileges);
    // 1 | 1 | 16 = 17
    EXPECT_EQ(result, "17");
}

TEST(privilege_test, vmm_kvm_different_values)
{
    // 验证 VMM 和 KVM 管理权限值不同
    EXPECT_NE(privilege::vmm_mgmt, privilege::kvm_mgmt);
    EXPECT_EQ(privilege::vmm_mgmt, 64u);
    EXPECT_EQ(privilege::kvm_mgmt, 128u);
    EXPECT_EQ(privilege::configure_self, 256u);
}

TEST(privilege_test, auth_state_values)
{
    // 验证认证状态枚举值
    EXPECT_EQ(static_cast<int>(auth_state::no_auth), 0);
    EXPECT_EQ(static_cast<int>(auth_state::auth_required), 1);
}

// ========== 测试 validate 函数 ==========

class privilege_validate_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        obj.init();
    }

    test_service service;
    test_object  obj;
};

TEST_F(privilege_validate_test, validate_no_context)
{
    // 无上下文时应该直接返回
    EXPECT_NO_THROW(validate(privilege::read_only));
}

TEST_F(privilege_validate_test, validate_auth_not_required)
{
    mc::engine::context ctx(service, obj);

    // Auth 不为 "1"（auth_required），应该直接返回
    ctx.set_arg("Auth", "0");
    ctx.set_arg("Privilege", "255");

    {
        mc::engine::context_stack::context guard(&service, ctx);
        EXPECT_NO_THROW(validate(privilege::read_only));
    }
}

TEST_F(privilege_validate_test, validate_auth_not_string)
{
    mc::engine::context ctx(service, obj);

    // Auth 不是字符串，应该直接返回
    ctx.set_arg("Auth", mc::variant(1));
    ctx.set_arg("Privilege", "255");

    {
        mc::engine::context_stack::context guard(&service, ctx);
        EXPECT_NO_THROW(validate(privilege::read_only));
    }
}

TEST_F(privilege_validate_test, validate_privilege_not_string)
{
    mc::engine::context ctx(service, obj);

    // Privilege 不是字符串，应该抛出 insufficient_privilege_exception
    ctx.set_arg("Auth", "1");
    ctx.set_arg("Privilege", mc::variant(255));

    {
        mc::engine::context_stack::context guard(&service, ctx);
        EXPECT_THROW(validate(privilege::read_only), mc::insufficient_privilege_exception);
    }
}

TEST_F(privilege_validate_test, validate_privilege_conversion_error)
{
    mc::engine::context ctx(service, obj);

    // Privilege 字段格式错误，无法转换为数字
    ctx.set_arg("Auth", "1");
    ctx.set_arg("Privilege", "invalid");

    {
        mc::engine::context_stack::context guard(&service, ctx);
        EXPECT_THROW(validate(privilege::user_mgmt), mc::insufficient_privilege_exception);
    }
}

TEST_F(privilege_validate_test, validate_expected_zero_with_configure_self)
{
    mc::engine::context ctx(service, obj);

    // 期望权限为 0 且 priv 为 ConfigureSelf，应该抛出 password_changed_required_exception
    ctx.set_arg("Auth", "1");
    ctx.set_arg("Privilege", "256");

    {
        mc::engine::context_stack::context guard(&service, ctx);
        EXPECT_THROW(validate(0), mc::password_changed_required_exception);
    }
}

TEST_F(privilege_validate_test, validate_expected_zero_with_other_privilege)
{
    mc::engine::context ctx(service, obj);

    // 期望权限为 0 且 priv 不为 ConfigureSelf，应该直接返回
    ctx.set_arg("Auth", "1");
    ctx.set_arg("Privilege", "1");

    {
        mc::engine::context_stack::context guard(&service, ctx);
        EXPECT_NO_THROW(validate(0));
    }
}

TEST_F(privilege_validate_test, validate_insufficient_privilege)
{
    mc::engine::context ctx(service, obj);

    // 当前权限不满足期望权限，应该抛出 insufficient_privilege_exception
    ctx.set_arg("Auth", "1");
    ctx.set_arg("Privilege", "1"); // 只有 read_only

    {
        mc::engine::context_stack::context guard(&service, ctx);
        EXPECT_THROW(validate(privilege::user_mgmt), mc::insufficient_privilege_exception);
    }
}

TEST_F(privilege_validate_test, validate_sufficient_privilege)
{
    mc::engine::context ctx(service, obj);

    // 当前权限满足期望权限，应该成功并将 Auth 置为 "0" (no_auth)
    ctx.set_arg("Auth", "1");
    ctx.set_arg("Privilege", "255"); // 所有权限

    {
        mc::engine::context_stack::context guard(&service, ctx);
        EXPECT_NO_THROW(validate(privilege::user_mgmt));
    }

    // 验证 Auth 已被置为 "0"
    auto auth_var = ctx.get_arg("Auth");
    EXPECT_EQ(auth_var.as_string(), "0");
}

TEST_F(privilege_validate_test, validate_exact_privilege)
{
    mc::engine::context ctx(service, obj);

    // 当前权限正好等于期望权限
    ctx.set_arg("Auth", "1");
    ctx.set_arg("Privilege", "16"); // user_mgmt

    {
        mc::engine::context_stack::context guard(&service, ctx);
        EXPECT_NO_THROW(validate(privilege::user_mgmt));
    }

    // 验证 Auth 已被置为 "0"
    auto auth_var = ctx.get_arg("Auth");
    EXPECT_EQ(auth_var.as_string(), "0");
}

TEST_F(privilege_validate_test, validate_multiple_privileges)
{
    mc::engine::context ctx(service, obj);

    // 当前权限包含多个权限，满足期望的单个权限
    ctx.set_arg("Auth", "1");
    ctx.set_arg("Privilege", "49"); // read_only | user_mgmt | power_mgmt

    {
        mc::engine::context_stack::context guard(&service, ctx);
        EXPECT_NO_THROW(validate(privilege::user_mgmt));
    }

    // 验证 Auth 已被置为 "0"
    auto auth_var = ctx.get_arg("Auth");
    EXPECT_EQ(auth_var.as_string(), "0");
}

TEST_F(privilege_validate_test, validate_combined_privilege_requirement)
{
    mc::engine::context ctx(service, obj);

    // 测试组合权限要求
    ctx.set_arg("Auth", "1");
    ctx.set_arg("Privilege", "49"); // read_only | user_mgmt | power_mgmt

    {
        mc::engine::context_stack::context guard(&service, ctx);
        // 要求同时具有 user_mgmt 和 power_mgmt
        uint32_t required = privilege::user_mgmt | privilege::power_mgmt;
        EXPECT_NO_THROW(validate(required));
    }
}

TEST_F(privilege_validate_test, validate_combined_privilege_insufficient)
{
    mc::engine::context ctx(service, obj);

    // 测试组合权限不足的情况
    ctx.set_arg("Auth", "1");
    ctx.set_arg("Privilege", "1"); // 只有 read_only

    {
        mc::engine::context_stack::context guard(&service, ctx);
        // 要求同时具有 user_mgmt 和 power_mgmt
        uint32_t required = privilege::user_mgmt | privilege::power_mgmt;
        EXPECT_THROW(validate(required), mc::insufficient_privilege_exception);
    }
}
