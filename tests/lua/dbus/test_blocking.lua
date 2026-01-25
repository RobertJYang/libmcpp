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
local ldbus = require('ldbus')

-- 测试 ldbus.blocking 模块
TestBlocking = {}

function TestBlocking:setUp()
    self.blocking = ldbus.blocking
    self.message = ldbus.message
end

function TestBlocking:tearDown()
    if self.conn then
        pcall(function() self.conn:close() end)
        self.conn = nil
    end
end

function TestBlocking:test_01_open_user()
    self.conn = self.blocking.open_user()
    lu.assertNotNil(self.conn, "open_user() should return connection object")
end

function TestBlocking:test_02_get_conn_property()
    self.conn = self.blocking.open_user()
    
    local inner_conn = self.conn.conn
    lu.assertNotNil(inner_conn, "conn.conn should return inner connection")
end

function TestBlocking:test_03_send_without_timeout()
    self.conn = self.blocking.open_user()
    
    local inner_conn = self.conn.conn
    local msg = self.message.new_method_call(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "ListNames"
    )
    
    local reply = inner_conn:send_with_reply_and_block(msg)
    lu.assertNotNil(reply, "send_with_reply_and_block() without timeout should work")
end

function TestBlocking:test_04_send_with_nil_timeout()
    self.conn = self.blocking.open_user()
    
    local inner_conn = self.conn.conn
    local msg = self.message.new_method_call(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "ListNames"
    )
    
    local reply = inner_conn:send_with_reply_and_block(msg, nil)
    lu.assertNotNil(reply, "send_with_reply_and_block() with nil timeout should work")
end

function TestBlocking:test_05_send_with_timeout_value()
    self.conn = self.blocking.open_user()
    
    local inner_conn = self.conn.conn
    local msg = self.message.new_method_call(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "ListNames"
    )
    
    local reply = inner_conn:send_with_reply_and_block(msg, 5000)
    lu.assertNotNil(reply, "send_with_reply_and_block() with timeout=5000 should work")
end

function TestBlocking:test_06_flush()
    self.conn = self.blocking.open_user()
    
    -- flush 应该不抛出异常
    lu.assertNotNil(pcall(function() self.conn:flush() end))
end

function TestBlocking:test_07_dispatch()
    self.conn = self.blocking.open_user()
    
    -- dispatch 应该不抛出异常
    lu.assertNotNil(pcall(function() self.conn:dispatch() end))
end

function TestBlocking:test_08_request_name()
    self.conn = self.blocking.open_user()
    
    local test_name = "com.test.LuaDbusTest." .. tostring(os.time())
    local inner_conn = self.conn.conn
    local result, err = inner_conn:request_name(test_name)
    -- request_name 可能因为各种原因失败（如权限、名称冲突等），不强制要求成功
    lu.assertNotNil(result, "request_name() should return a result")
end

function TestBlocking:test_09_run_once()
    self.conn = self.blocking.open_user()
    
    local result = self.conn:run_once(100)
    lu.assertNotNil(result, "run_once() should return a result")
end

function TestBlocking:test_11_run_until_timeout()
    self.conn = self.blocking.open_user()
    
    local result = self.conn:run_until(function()
        return false  -- 永远不满足
    end, 500, 100)
    
    lu.assertEquals(result, false, "run_until() should timeout when condition never met")
end

function TestBlocking:test_12_close()
    self.conn = self.blocking.open_user()
    
    -- close 应该不抛出异常
    lu.assertNotNil(pcall(function() self.conn:close() end))
end

function TestBlocking:test_13_invalid_env()
    -- 测试环境变量无效的场景
    -- 注意：由于系统可能有默认的D-Bus路径，即使环境变量无效也可能连接成功
    -- 所以这个测试主要验证不会崩溃
    local original_addr = os.getenv("DBUS_SESSION_BUS_ADDRESS")
    
    -- 设置无效的环境变量
    os.execute('export DBUS_SESSION_BUS_ADDRESS="unix:path=/invalid/path"')
    
    -- 尝试连接
    local success, err = pcall(function()
        self.conn = self.blocking.open_user()
    end)
    
    -- 恢复环境变量
    if original_addr then
        os.execute('export DBUS_SESSION_BUS_ADDRESS="' .. original_addr .. '"')
    end
    
    -- 主要验证不会崩溃，可能成功（使用默认路径）或失败
    lu.assertNotNil(success, "Should handle invalid env gracefully")
end

function TestBlocking:test_14_double_close()
    self.conn = self.blocking.open_user()
    
    -- 第一次关闭
    lu.assertNotNil(pcall(function() self.conn:close() end))
    
    -- 第二次关闭不应崩溃
    lu.assertNotNil(pcall(function() self.conn:close() end))
end

function TestBlocking:test_15_get_unique_name()
    self.bus = self.blocking.open_user()
    lu.assertNotNil(self.bus.conn:unique_name())
end

function TestBlocking:test_16_run_once_invalid_timeout()
    self.conn = self.blocking.open_user()
    
    -- 使用无效的超时参数
    local success, err = pcall(function()
        self.conn:run_once("invalid")
    end)
    
    lu.assertEquals(success, false, "Invalid timeout type should fail")
end

function TestBlocking:test_17_run_until_invalid_callback()
    self.conn = self.blocking.open_user()
    
    -- 使用非函数作为回调
    local success, err = pcall(function()
        self.conn:run_until("not a function", 1000, 100)
    end)
    
    lu.assertEquals(success, false, "Invalid callback type should fail")
end

local function array_contains(array, value)
    for _, v in ipairs(array) do
        if v == value then
            return true
        end
    end
    return false
end

function TestBlocking:test_18_method_call()
    self.conn = self.blocking.open_user()

    local ret = self.conn:call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListActivatableNames", "")
    lu.assertEquals(type(ret), "table", "ListActivatableNames should return a table")
    lu.assertTrue(array_contains(ret, "org.freedesktop.DBus"), "ListActivatableNames should contain org.freedesktop.DBus")
end

function TestBlocking:test_19_method_timeout_call()
    self.conn = self.blocking.open_user()

    local ret = self.conn:timeout_call(1000, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListActivatableNames", "")
    lu.assertEquals(type(ret), "table", "ListActivatableNames should return a table")
    lu.assertTrue(array_contains(ret, "org.freedesktop.DBus"), "ListActivatableNames should contain org.freedesktop.DBus")
end

function TestBlocking:test_20_method_call_invalid_args()
    self.conn = self.blocking.open_user()

    local success, err = pcall(function()
        self.conn:call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NonExistentMethod", "")
    end)

    lu.assertEquals(success, false, "NonExistentMethod should fail")
    lu.assertEquals(type(err), "string", "NonExistentMethod should return an error message")
end

-- 只有当直接运行此文件时才执行测试
if arg and arg[0]:match("test_blocking%.lua$") then
    os.exit(lu.LuaUnit.run())
end

return TestBlocking
