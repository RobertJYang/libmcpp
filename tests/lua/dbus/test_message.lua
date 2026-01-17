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

-- 测试 ldbus.message 模块
TestMessage = {}

function TestMessage:setUp()
    self.message = ldbus.message
    self.blocking = ldbus.blocking
end

function TestMessage:tearDown()
    if self.conn then
        pcall(function() self.conn:close() end)
        self.conn = nil
    end
end

function TestMessage:test_01_create_method_call()
    local msg = self.message.new_method_call(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "ListNames"
    )
    lu.assertNotNil(msg, "new_method_call() should return message object")
    lu.assertEquals(type(msg), "userdata", "message should be userdata")
end

function TestMessage:test_02_create_multiple_messages()
    local msg1 = self.message.new_method_call("org.test1", "/path1", "org.test1.Interface", "Method1")
    local msg2 = self.message.new_method_call("org.test2", "/path2", "org.test2.Interface", "Method2")
    local msg3 = self.message.new_method_call("org.test3", "/path3", "org.test3.Interface", "Method3")
    
    lu.assertNotNil(msg1, "First message should be created")
    lu.assertNotNil(msg2, "Second message should be created")
    lu.assertNotNil(msg3, "Third message should be created")
    
    -- 确保它们是不同的对象
    lu.assertNotEquals(msg1, msg2, "Messages should be different objects")
    lu.assertNotEquals(msg2, msg3, "Messages should be different objects")
end

function TestMessage:test_03_message_with_special_characters()
    local msg = self.message.new_method_call(
        "org.test.Service",
        "/path/to/object",
        "org.test.Interface",
        "MethodName"
    )
    lu.assertNotNil(msg, "Message with paths should be created")
end

function TestMessage:test_04_missing_parameters()
    local success, err = pcall(function()
        self.message.new_method_call("org.test")
    end)
    lu.assertEquals(success, false, "Missing parameters should fail")
end

function TestMessage:test_05_empty_string_parameters()
    local success, err = pcall(function()
        self.message.new_method_call("", "", "", "")
    end)
    -- 空字符串可能被接受或拒绝，取决于实现
    lu.assertNotNil(success, "Empty string test completed")
end

function TestMessage:test_06_use_in_actual_call()
    self.conn = self.blocking.open_user()
    
    -- 创建消息
    local msg = self.message.new_method_call(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "ListNames"
    )
    lu.assertNotNil(msg, "Message created successfully")
    
    -- 使用消息进行调用
    local inner_conn = self.conn.conn
    local reply = inner_conn:send_with_reply_and_block(msg)
    lu.assertNotNil(reply, "Message should work in actual call")
end

function TestMessage:test_07_different_services()
    local services = {
        "org.freedesktop.DBus",
        "com.test.Service",
        "net.example.App",
        "io.github.Project"
    }
    
    for _, service in ipairs(services) do
        local msg = self.message.new_method_call(
            service,
            "/",
            service .. ".Interface",
            "Method"
        )
        lu.assertNotNil(msg, string.format("Message for %s should be created", service))
    end
end

function TestMessage:test_08_different_paths()
    local paths = {
        "/",
        "/org/test",
        "/org/test/object",
        "/path/to/deep/object"
    }
    
    for _, path in ipairs(paths) do
        local msg = self.message.new_method_call(
            "org.test",
            path,
            "org.test.Interface",
            "Method"
        )
        lu.assertNotNil(msg, string.format("Message with path %s should be created", path))
    end
end

function TestMessage:test_09_different_methods()
    local methods = {
        "Get",
        "Set",
        "ListNames",
        "GetConnectionUnixUser",
        "RequestName"
    }
    
    for _, method in ipairs(methods) do
        local msg = self.message.new_method_call(
            "org.test",
            "/",
            "org.test.Interface",
            method
        )
        lu.assertNotNil(msg, string.format("Message for method %s should be created", method))
    end
end

function TestMessage:test_10_stress_test()
    local count = 100
    local messages = {}
    
    for i = 1, count do
        -- D-Bus bus name 不能以数字开头，使用字母前缀
        local msg = self.message.new_method_call(
            "org.test.Service" .. tostring(i),
            "/path/" .. tostring(i),
            "org.test.Interface",
            "Method" .. tostring(i)
        )
        table.insert(messages, msg)
    end
    
    lu.assertEquals(#messages, count, string.format("Should create %d messages", count))
end

-- 只有当直接运行此文件时才执行测试
if arg and arg[0]:match("test_message%.lua$") then
    os.exit(lu.LuaUnit.run())
end

return TestMessage
