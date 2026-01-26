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
]] local lu = require('luaunit')

-- 设置库搜索路径
package.cpath = '../builddir/src/luaclib/?.so;' .. package.cpath

local ldbus = require('ldbus')
local lmdb = require('lmdb')
local mdb_service = lmdb.mdb_service

-- 测试辅助函数
local function create_test_bus()
    local bus = ldbus.blocking.new()
    assert(bus, "Failed to create D-Bus connection")

    return bus
end

-- 测试套件: lmdb_service 基础功能
TestLMDBService = {}

function TestLMDBService:setUp()
    self.bus = create_test_bus()
    self.conn = self.bus.conn  -- 直接使用 CONNECTION_METATABLE userdata
end

function TestLMDBService:tearDown() if self.bus then self.bus:close() end end

-- 测试错误处理 - 无效连接
function TestLMDBService:test_error_invalid_connection()
    lu.assertError(function() mdb_service.get_service_names(nil) end)
end

-- 独立运行此测试文件
if not lu then
    print("Error: luaunit module not found")
    os.exit(1)
end

-- 如果直接运行此文件
if arg and arg[0]:match("test_mdb_service.lua") then os.exit(lu.LuaUnit.run()) end
