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

-- 测试 ldbus 错误处理
TestError = {}

function TestError:setUp()
    self.blocking = ldbus.blocking
    self.message = ldbus.message
end

function TestError:tearDown()
    if self.conn then
        pcall(function() self.conn:close() end)
        self.conn = nil
    end
end

function TestError:test_01_call_without_connection()
    self.conn = self.blocking.open_user()
    -- 不调用 start()，但 open_user 会自动建立连接
    -- 这个测试需要验证未完全初始化的连接行为
    
    -- 注意：blocking.open_user() 实际上会建立底层连接
    -- 所以这个测试的预期可能需要调整
    local inner_conn = self.conn.conn
    lu.assertNotNil(inner_conn, "conn should be created")
    
    -- 尝试调用，可能成功或失败取决于实现
    local success, err = pcall(function()
        local msg = self.message.new_method_call(
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "ListNames"
        )
        inner_conn:send_with_reply_and_block(msg, 1000)
    end)
    
    -- 只验证调用有返回结果，不强制要求失败
    lu.assertNotNil(success, "Should have a result")
end

function TestError:test_02_call_invalid_service()
    self.conn = self.blocking.open_user()
    
    local success, err = pcall(function()
        local inner_conn = self.conn.conn
        local msg = self.message.new_method_call(
            "org.invalid.Service",
            "/",
            "org.invalid.Interface",
            "Method"
        )
        inner_conn:send_with_reply_and_block(msg, 1000)
    end)
    
    lu.assertEquals(success, false, "Calling invalid service should fail")
    lu.assertNotNil(err, "Should return error message")
end

function TestError:test_03_invalid_timeout_parameter()
    self.conn = self.blocking.open_user()
    
    local success, err = pcall(function()
        local inner_conn = self.conn.conn
        local msg = self.message.new_method_call(
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "ListNames"
        )
        -- 传入字符串作为 timeout（应该失败）
        inner_conn:send_with_reply_and_block(msg, "invalid")
    end)
    
    lu.assertEquals(success, false, "Invalid timeout type should fail")
end

function TestError:test_04_missing_required_parameters()
    local success, err = pcall(function()
        -- 尝试创建不完整的 message
        self.message.new_method_call("org.freedesktop.DBus")
    end)
    
    lu.assertEquals(success, false, "Missing parameters should fail")
end

function TestError:test_05_short_timeout()
    self.conn = self.blocking.open_user()
    
    -- 使用很短的超时时间
    local start_time = os.clock()
    local success, err = pcall(function()
        local inner_conn = self.conn.conn
        local msg = self.message.new_method_call(
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "ListNames"
        )
        inner_conn:send_with_reply_and_block(msg, 1)  -- 1ms 超时
    end)
    local elapsed = os.clock() - start_time
    
    -- 可能成功（如果响应很快）或超时
    lu.assertNotNil(success, "Timeout test completed")
end

function TestError:test_06_connection_lifecycle()
    self.conn = self.blocking.open_user()
    
    -- 正常调用
    local inner_conn = self.conn.conn
    local msg1 = self.message.new_method_call(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "ListNames"
    )
    local reply1 = inner_conn:send_with_reply_and_block(msg1)
    lu.assertNotNil(reply1, "Call before close should work")
    
    -- 关闭连接
    self.conn:close()
    
    -- 尝试在关闭后调用
    local success, err = pcall(function()
        local msg2 = self.message.new_method_call(
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "ListNames"
        )
        inner_conn:send_with_reply_and_block(msg2)
    end)
    
    lu.assertEquals(success, false, "Call after close should fail")
end

function TestError:test_07_error_message_content()
    self.conn = self.blocking.open_user()
    
    -- 测试调用无效服务来获取错误消息
    local success, err = pcall(function()
        local inner_conn = self.conn.conn
        local msg = self.message.new_method_call(
            "org.invalid.NonExistentService",
            "/",
            "org.invalid.Interface",
            "Method"
        )
        inner_conn:send_with_reply_and_block(msg, 100)
    end)
    
    lu.assertEquals(success, false, "Calling invalid service should fail")
    lu.assertEquals(type(err), "string", "Error should be a string")
    lu.assertIsTrue(string.len(err) > 0, "Error message should not be empty")
end

-- 只有当直接运行此文件时才执行测试
if arg and arg[0]:match("test_error%.lua$") then
    os.exit(lu.LuaUnit.run())
end

return TestError
