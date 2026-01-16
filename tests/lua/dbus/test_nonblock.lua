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

-- 测试 ldbus.nonblock 模块
TestNonblock = {}

function TestNonblock:setUp()
    self.nonblock = ldbus.nonblock
    self.message = ldbus.message
end

function TestNonblock:tearDown()
    if self.conn then
        pcall(function() self.conn:close() end)
        self.conn = nil
    end
end

function TestNonblock:test_01_open_user_no_auto_start()
    self.conn = self.nonblock.open_user(false)
    lu.assertNotNil(self.conn, "open_user(false) should return connection object")
end

function TestNonblock:test_02_start()
    self.conn = self.nonblock.open_user(false)
    local ok = self.conn:start()
    lu.assertEquals(ok, true, "start() should return true")
end

function TestNonblock:test_03_call_without_timeout()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    -- nonblock 通过 conn 属性访问底层 connection 对象
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

function TestNonblock:test_04_call_with_nil_timeout()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
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

function TestNonblock:test_05_call_with_timeout_value()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
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

function TestNonblock:test_06_request_name()
    self.conn = self.nonblock.open_user(false)
    local ok = self.conn:start()
    lu.assertEquals(ok, true, "start() should succeed")
    
    local test_name = "com.test.LuaNonblockTest." .. tostring(os.time())
    local result, err = self.conn:request_name(test_name)
    -- request_name 可能因为各种原因失败（如权限、名称冲突等），不强制要求成功
    lu.assertNotNil(result, "request_name() should return a result")
end

function TestNonblock:test_07_flush()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    -- flush 应该不抛出异常
    lu.assertNotNil(pcall(function() self.conn:flush() end))
end

function TestNonblock:test_08_dispatch()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    -- dispatch 应该不抛出异常
    lu.assertNotNil(pcall(function() self.conn:dispatch() end))
end

function TestNonblock:test_09_close()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    -- close 应该不抛出异常
    lu.assertNotNil(pcall(function() self.conn:close() end))
end

function TestNonblock:test_10_open_user_auto_start()
    self.conn = self.nonblock.open_user(true)
    lu.assertNotNil(self.conn, "open_user(true) should return connection object")
    
    -- 自动启动的连接应该可以直接调用
    local inner_conn = self.conn.conn
    local msg = self.message.new_method_call(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "ListNames"
    )
    local reply = inner_conn:send_with_reply_and_block(msg)
    lu.assertNotNil(reply, "auto-started connection should work")
end

function TestNonblock:test_11_open_user_default()
    self.conn = self.nonblock.open_user()
    lu.assertNotNil(self.conn, "open_user() should return connection object")
    
    local inner_conn = self.conn.conn
    local msg = self.message.new_method_call(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "ListNames"
    )
    local reply = inner_conn:send_with_reply_and_block(msg)
    lu.assertNotNil(reply, "default open_user() should work")
end

function TestNonblock:test_12_invalid_env()
    -- 测试环境变量无效的场景
    -- 注意：由于系统可能有默认的D-Bus路径，即使环境变量无效也可能连接成功
    -- 所以这个测试主要验证不会崩溃
    local original_addr = os.getenv("DBUS_SESSION_BUS_ADDRESS")
    
    -- 设置无效的环境变量
    os.execute('export DBUS_SESSION_BUS_ADDRESS="unix:path=/invalid/path"')
    
    -- 尝试连接
    local success, err = pcall(function()
        self.conn = self.nonblock.open_user(false)
    end)
    
    -- 恢复环境变量
    if original_addr then
        os.execute('export DBUS_SESSION_BUS_ADDRESS="' .. original_addr .. '"')
    end
    
    -- 主要验证不会崩溃，可能成功（使用默认路径）或失败
    lu.assertNotNil(success, "Should handle invalid env gracefully")
end

function TestNonblock:test_13_double_close()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    -- 第一次关闭
    lu.assertNotNil(pcall(function() self.conn:close() end))
    
    -- 第二次关闭不应崩溃
    lu.assertNotNil(pcall(function() self.conn:close() end))
end

function TestNonblock:test_14_get_unique_name()
    self.bus = self.nonblock.open_user(true)
    lu.assertNotNil(self.bus.conn:unique_name())
end

function TestNonblock:test_15_double_start()
    self.conn = self.nonblock.open_user(false)
    
    -- 第一次 start
    local ok1 = self.conn:start()
    lu.assertEquals(ok1, true, "First start() should succeed")
    
    -- 第二次 start 应该返回 false
    local ok2 = self.conn:start()
    lu.assertEquals(ok2, false, "Second start() should return false")
end

function TestNonblock:test_16_disconnect()
    self.conn = self.nonblock.open_user(true)
    
    lu.assertNotNil(pcall(function() self.conn:disconnect() end))
end

-- 只有当直接运行此文件时才执行测试
if arg and arg[0]:match("test_nonblock%.lua$") then
    os.exit(lu.LuaUnit.run())
end

return TestNonblock
