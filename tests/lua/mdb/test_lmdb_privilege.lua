#!/usr/bin/env lua
--[[
Copyright (c) 2026 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
]]

local lu = require('luaunit')
local lmdb = require('lmdb')

-- 测试套件: lmdb.privilege 模块
TestLMDBPrivilege = {}

function TestLMDBPrivilege:setUp()
    self.privilege = lmdb.privilege
end

-- ========== 测试权限常量 ==========

function TestLMDBPrivilege:test_01_privilege_constants_values()
    -- 验证权限常量的值
    lu.assertEquals(self.privilege.ReadOnly, 1, "ReadOnly should be 1")
    lu.assertEquals(self.privilege.DiagnoseMgmt, 2, "DiagnoseMgmt should be 2")
    lu.assertEquals(self.privilege.SecurityMgmt, 4, "SecurityMgmt should be 4")
    lu.assertEquals(self.privilege.BasicSetting, 8, "BasicSetting should be 8")
    lu.assertEquals(self.privilege.UserMgmt, 16, "UserMgmt should be 16")
    lu.assertEquals(self.privilege.PowerMgmt, 32, "PowerMgmt should be 32")
    lu.assertEquals(self.privilege.VMMMgmt, 64, "VMMMgmt should be 64")
    lu.assertEquals(self.privilege.KVMMgmt, 128, "KVMMgmt should be 128")
    lu.assertEquals(self.privilege.ConfigureSelf, 256, "ConfigureSelf should be 256")
end

function TestLMDBPrivilege:test_02_vmm_kvm_different_values()
    -- 验证 VMMMgmt 和 KVMMgmt 的值不同
    lu.assertNotEquals(self.privilege.VMMMgmt, self.privilege.KVMMgmt, 
                      "VMMMgmt and KVMMgmt should have different values")
end

function TestLMDBPrivilege:test_03_auth_state_constants()
    -- 验证认证状态常量
    lu.assertEquals(self.privilege.NoAuth, 0, "NoAuth should be 0")
    lu.assertEquals(self.privilege.AuthRequired, 1, "AuthRequired should be 1")
end

-- ========== 测试 get_privilege_str 函数 ==========

function TestLMDBPrivilege:test_10_get_privilege_str_empty_array()
    -- 空数组应该返回 "0"
    local result = self.privilege.get_privilege_str({})
    lu.assertEquals(result, "0", "Empty array should return '0'")
end

function TestLMDBPrivilege:test_11_get_privilege_str_single_privilege()
    -- 单个权限
    local result = self.privilege.get_privilege_str({self.privilege.ReadOnly})
    lu.assertEquals(result, "1", "ReadOnly should return '1'")
end

function TestLMDBPrivilege:test_12_get_privilege_str_multiple_privileges()
    -- 多个权限组合: ReadOnly | UserMgmt | PowerMgmt = 1 | 16 | 32 = 49
    local result = self.privilege.get_privilege_str({
        self.privilege.ReadOnly,
        self.privilege.UserMgmt,
        self.privilege.PowerMgmt
    })
    lu.assertEquals(result, "49", "ReadOnly|UserMgmt|PowerMgmt should return '49'")
end

function TestLMDBPrivilege:test_13_get_privilege_str_all_privileges()
    -- 所有权限: 1|2|4|8|16|32|64|128|256 = 511
    local result = self.privilege.get_privilege_str({
        self.privilege.ReadOnly,
        self.privilege.DiagnoseMgmt,
        self.privilege.SecurityMgmt,
        self.privilege.BasicSetting,
        self.privilege.UserMgmt,
        self.privilege.PowerMgmt,
        self.privilege.VMMMgmt,
        self.privilege.KVMMgmt,
        self.privilege.ConfigureSelf
    })
    lu.assertEquals(result, "511", "All privileges should return '511'")
end

function TestLMDBPrivilege:test_14_get_privilege_str_duplicate_privileges()
    -- 重复权限：1 | 1 | 16 = 17
    local result = self.privilege.get_privilege_str({
        self.privilege.ReadOnly,
        self.privilege.ReadOnly,
        self.privilege.UserMgmt
    })
    lu.assertEquals(result, "17", "Duplicate privileges should be handled correctly")
end

function TestLMDBPrivilege:test_15_get_privilege_str_vmm_only()
    -- 仅 VMM 权限
    local result = self.privilege.get_privilege_str({self.privilege.VMMMgmt})
    lu.assertEquals(result, "64", "VMMMgmt only should return '64'")
end

function TestLMDBPrivilege:test_16_get_privilege_str_kvm_only()
    -- 仅 KVM 权限
    local result = self.privilege.get_privilege_str({self.privilege.KVMMgmt})
    lu.assertEquals(result, "128", "KVMMgmt only should return '128'")
end

function TestLMDBPrivilege:test_17_get_privilege_str_vmm_and_kvm()
    -- VMM 和 KVM 组合：64 | 128 = 192
    local result = self.privilege.get_privilege_str({
        self.privilege.VMMMgmt,
        self.privilege.KVMMgmt
    })
    lu.assertEquals(result, "192", "VMMMgmt|KVMMgmt should return '192'")
end

-- ========== 测试错误处理 ==========

function TestLMDBPrivilege:test_20_get_privilege_str_error_non_table()
    -- 传入非表类型应该抛出错误
    lu.assertError(function()
        self.privilege.get_privilege_str(123)
    end)
end

function TestLMDBPrivilege:test_21_get_privilege_str_error_nil()
    -- 传入 nil 应该抛出错误
    lu.assertError(function()
        self.privilege.get_privilege_str(nil)
    end)
end

-- 独立运行此测试文件
if not lu then
    print("Error: luaunit module not found")
    os.exit(1)
end

-- 如果直接运行此文件
if arg and arg[0]:match("test_lmdb_privilege.lua") then
    os.exit(lu.LuaUnit.run())
end
