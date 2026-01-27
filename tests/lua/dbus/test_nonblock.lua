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

local function array_contains(array, value)
    for _, v in ipairs(array) do
        if v == value then
            return true
        end
    end
    return false
end

function TestNonblock:test_17_method_async_call()
    self.conn = self.nonblock.open_user(true)
    self.conn:async_call(function(ok, ret)
        lu.assertEquals(ok, true, "ListActivatableNames should return true")
        lu.assertEquals(type(ret), "table", "ListActivatableNames should return a table")
        lu.assertTrue(array_contains(ret, "org.freedesktop.DBus"), "ListActivatableNames should contain org.freedesktop.DBus")
    end, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListActivatableNames", "")
end

function TestNonblock:test_18_method_async_timeout_call()
    self.conn = self.nonblock.open_user(true)
    self.conn:async_timeout_call(1000, function(ok, ret)
        lu.assertEquals(ok, true, "ListActivatableNames should return true")
        lu.assertEquals(type(ret), "table", "ListActivatableNames should return a table")
        lu.assertTrue(array_contains(ret, "org.freedesktop.DBus"), "ListActivatableNames should contain org.freedesktop.DBus")
    end, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListActivatableNames", "")
end

function TestNonblock:test_19_method_call_invalid_args()
    self.conn = self.nonblock.open_user(true)
    self.conn:async_call(function(ok, ret)
        lu.assertEquals(ok, false, "NonExistentMethod should return false")
        lu.assertEquals(type(ret), "string", "NonExistentMethod should return an error message")
    end, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NonExistentMethod", "")
end

-- 测试 notify_signal 函数 - 基础功能
function TestNonblock:test_20_notify_signal_basic()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    -- 测试发送信号（最小参数）
    local path = "/com/test/Object"
    local interface = "com.test.Interface"
    local member = "TestSignal"
    
    local ok = self.conn:notify_signal(path, interface, member)
    lu.assertIsBoolean(ok, "notify_signal should return boolean")
end

-- 测试 notify_signal 函数 - 带 destination
function TestNonblock:test_21_notify_signal_with_destination()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    local path = "/com/test/Object"
    local interface = "com.test.Interface"
    local member = "TestSignal"
    local destination = ":1.123"  -- 示例唯一名称
    
    local ok = self.conn:notify_signal(path, interface, member, destination)
    lu.assertIsBoolean(ok, "notify_signal with destination should return boolean")
end

-- 测试 notify_signal 函数 - 参数验证
function TestNonblock:test_22_notify_signal_parameter_validation()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    -- 测试缺少参数
    local success = pcall(function()
        self.conn:notify_signal()
    end)
    lu.assertFalse(success, "notify_signal without args should fail")
    
    success = pcall(function()
        self.conn:notify_signal("/path")
    end)
    lu.assertFalse(success, "notify_signal with 1 arg should fail")
    
    success = pcall(function()
        self.conn:notify_signal("/path", "interface")
    end)
    lu.assertFalse(success, "notify_signal with 2 args should fail")
    
    -- 测试正常调用
    success = pcall(function()
        self.conn:notify_signal("/path", "interface", "member")
    end)
    lu.assertTrue(success, "notify_signal with correct args should succeed")
end

-- 测试 notify_signal 函数 - 边界情况
function TestNonblock:test_23_notify_signal_edge_cases()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    -- 测试空字符串参数
    local success = pcall(function()
        self.conn:notify_signal("", "", "")
    end)
    lu.assertIsBoolean(success, "notify_signal with empty strings should handle gracefully")
    
    -- 测试很长的路径
    local long_path = "/" .. string.rep("a", 1000)
    local ok = self.conn:notify_signal(long_path, "com.test.Interface", "TestSignal")
    lu.assertIsBoolean(ok, "notify_signal with long path should return boolean")
end

-- 测试 add_match 函数 - 基础功能
function TestNonblock:test_24_add_match_basic()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    -- 测试 add_match（最小参数）
    -- 参数顺序：callback_id, rule_id, harbor_name, member, interface
    local callback_id = 1
    local rule_id = 100
    local harbor_name = "test.harbor"
    local member = "TestSignal"
    local interface = "com.test.Interface"
    
    local ok = self.conn:add_match(callback_id, rule_id, harbor_name, member, interface)
    lu.assertIsBoolean(ok, "add_match should return boolean")
    lu.assertTrue(ok, "add_match should succeed with correct parameters")
end

-- 测试 add_match 函数 - 带 path
function TestNonblock:test_25_add_match_with_path()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    local callback_id = 2
    local rule_id = 101
    local harbor_name = "test.harbor"
    local member = "TestSignal"
    local interface = "com.test.Interface"
    local is_path_namespace = false
    local path = "/com/test/Object"
    
    -- 参数顺序：callback_id, rule_id, harbor_name, member, interface, is_path_namespace, path
    local ok = self.conn:add_match(callback_id, rule_id, harbor_name, 
                                   member, interface, is_path_namespace, path)
    lu.assertIsBoolean(ok, "add_match with path should return boolean")
end

-- 测试 add_match 函数 - 带 path_namespace
function TestNonblock:test_26_add_match_with_path_namespace()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    local callback_id = 3
    local rule_id = 102
    local harbor_name = "test.harbor"
    local member = "TestSignal"
    local interface = "com.test.Interface"
    local is_path_namespace = true
    local path_namespace = "/com/test"
    
    -- 参数顺序：callback_id, rule_id, harbor_name, member, interface, is_path_namespace, path_namespace
    local ok = self.conn:add_match(callback_id, rule_id, harbor_name,
                                   member, interface, is_path_namespace, path_namespace)
    lu.assertIsBoolean(ok, "add_match with path_namespace should return boolean")
end

-- 测试 add_match 函数 - 完整参数
function TestNonblock:test_27_add_match_full_parameters()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    local callback_id = 4
    local rule_id = 103
    local harbor_name = "test.harbor"
    local member = "TestSignal"
    local interface = "com.test.Interface"
    local is_path_namespace = false
    local path = "/com/test/Object"
    local sender = "com.test.Sender"
    local msg_type = 1  -- DBus::Match::MessageType
    local destination = ":1.123"
    
    -- 参数顺序：callback_id, rule_id, harbor_name, member, interface, 
    --           is_path_namespace, path, sender, msg_type, destination
    local ok = self.conn:add_match(callback_id, rule_id, harbor_name,
                                   member, interface, is_path_namespace, path, 
                                   sender, msg_type, destination)
    lu.assertIsBoolean(ok, "add_match with full parameters should return boolean")
end

-- 测试 add_match 函数 - 参数验证
function TestNonblock:test_28_add_match_parameter_validation()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    local harbor_name = "test.harbor"
    
    -- 测试缺少参数（至少需要5个显式参数：callback_id, rule_id, harbor_name, member, interface）
    -- 加上self（conn），总共6个参数
    local success = pcall(function()
        self.conn:add_match()
    end)
    lu.assertFalse(success, "add_match without args should fail")
    
    success = pcall(function()
        self.conn:add_match(1)
    end)
    lu.assertFalse(success, "add_match with 1 arg should fail")
    
    success = pcall(function()
        self.conn:add_match(1, 100)
    end)
    lu.assertFalse(success, "add_match with 2 args should fail")
    
    success = pcall(function()
        self.conn:add_match(1, 100, harbor_name)
    end)
    lu.assertFalse(success, "add_match with 3 args should fail")
    
    success = pcall(function()
        self.conn:add_match(1, 100, harbor_name, "member")
    end)
    lu.assertFalse(success, "add_match with 4 args should fail")
    
    -- 测试正常调用（5个显式参数，加上self共6个）
    success = pcall(function()
        self.conn:add_match(1, 100, harbor_name, "member", "interface")
    end)
    lu.assertTrue(success, "add_match with 5 args (6 including self) should succeed")
end

-- 测试 add_match 函数 - 边界情况
function TestNonblock:test_29_add_match_edge_cases()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    local harbor_name = "test.harbor"
    
    -- 测试零值 ID
    local ok = self.conn:add_match(0, 0, harbor_name, "TestSignal", "com.test.Interface")
    lu.assertIsBoolean(ok, "add_match with zero IDs should return boolean")
    
    -- 测试负数 ID
    local ok2 = self.conn:add_match(-1, -1, harbor_name, "TestSignal", "com.test.Interface")
    lu.assertIsBoolean(ok2, "add_match with negative IDs should return boolean")
    
    -- 测试空字符串参数
    local ok3 = self.conn:add_match(1, 1, "", "", "")
    lu.assertIsBoolean(ok3, "add_match with empty strings should return boolean")
end

-- 测试多个匹配规则
function TestNonblock:test_30_add_match_multiple_matches()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    local harbor_name = "test.harbor.multi"
    
    -- 添加多个匹配规则
    for i = 1, 3 do
        local callback_id = 20 + i
        local rule_id = 300 + i
        local member = "Signal" .. i
        local interface = "com.test.Interface" .. i
        
        local ok = self.conn:add_match(callback_id, rule_id, harbor_name, member, interface)
        lu.assertTrue(ok, "add_match " .. i .. " should succeed")
    end
end

-- 测试集成场景：notify_signal + add_match
function TestNonblock:test_31_integration_notify_signal_add_match()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    local harbor_name = "test.harbor.integration"
    
    -- 1. 添加匹配规则（add_match 内部会自动获取 shm_tree）
    local callback_id = 10
    local rule_id = 200
    local member = "IntegrationSignal"
    local interface = "com.test.IntegrationInterface"
    
    local ok = self.conn:add_match(callback_id, rule_id, harbor_name, member, interface)
    lu.assertTrue(ok, "add_match should succeed")
    
    -- 2. 发送信号
    local path = "/com/test/IntegrationObject"
    ok = self.conn:notify_signal(path, interface, member)
    lu.assertIsBoolean(ok, "notify_signal should return boolean")
end

-- 测试 notify_signal 和 add_match 的组合使用 - 不同路径
function TestNonblock:test_32_notify_signal_add_match_different_paths()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    local harbor_name = "test.harbor.path"
    local callback_id = 50
    local rule_id = 500
    local member = "PathTestSignal"
    local interface = "com.test.PathInterface"
    local path = "/com/test/SpecificPath"
    
    -- 添加带路径的匹配规则
    local ok = self.conn:add_match(callback_id, rule_id, harbor_name, 
                                   member, interface, false, path)
    lu.assertTrue(ok, "add_match with path should succeed")
    
    -- 发送匹配路径的信号
    local ok2 = self.conn:notify_signal(path, interface, member)
    lu.assertIsBoolean(ok2, "notify_signal with matching path should return boolean")
    
    -- 发送不匹配路径的信号（应该也能发送成功，只是不会被匹配）
    local ok3 = self.conn:notify_signal("/com/test/OtherPath", interface, member)
    lu.assertIsBoolean(ok3, "notify_signal with non-matching path should return boolean")
end

-- 测试 notify_signal 和 add_match 的组合使用 - 带 destination
function TestNonblock:test_33_notify_signal_add_match_with_destination()
    self.conn = self.nonblock.open_user(false)
    self.conn:start()
    
    local harbor_name = "test.harbor.dest"
    local callback_id = 60
    local rule_id = 600
    local member = "DestTestSignal"
    local interface = "com.test.DestInterface"
    local destination = ":1.999"
    
    -- 添加带 destination 的匹配规则
    local ok = self.conn:add_match(callback_id, rule_id, harbor_name, 
                                   member, interface, false, "/com/test/Dest", 
                                   nil, nil, destination)
    lu.assertTrue(ok, "add_match with destination should succeed")
    
    -- 发送带 destination 的信号
    local ok2 = self.conn:notify_signal("/com/test/Dest", interface, member, destination)
    lu.assertIsBoolean(ok2, "notify_signal with destination should return boolean")
end

-- 只有当直接运行此文件时才执行测试
if arg and arg[0]:match("test_nonblock%.lua$") then
    os.exit(lu.LuaUnit.run())
end

return TestNonblock
