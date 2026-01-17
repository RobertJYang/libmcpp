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

-- 运行所有Lua 测试

local lu = require('luaunit')

-- 获取当前脚本目录
local script_path = debug.getinfo(1, "S").source:sub(2)
local script_dir = script_path:match("(.*/)")

-- 设置 package.path 以便加载测试模块
if script_dir then
    package.path = script_dir .. "?.lua;" .. script_dir .. "?/?.lua;" .. script_dir .. "dbus/?.lua;" .. package.path
end

-- 导入所有测试模块
require('dft')
require('dbus.test_blocking')
require('dbus.test_nonblock')
require('dbus.test_error')
require('dbus.test_message')
require('lvalidate.test_integer')

-- 设置详细输出，显示每个测试用例的执行情况
-- 通过命令行参数传递给 luaunit
-- -v 或 --verbose: 详细输出模式，显示每个测试的执行
local args = {'-v'}

-- Lua 5.2+ 使用 table.unpack 替代 unpack
local unpack = table.unpack or unpack

-- 运行所有测试
os.exit(lu.LuaUnit.run(unpack(args)))
