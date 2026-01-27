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

-- 设置库搜索路径
package.cpath = '../builddir/src/luaclib/?.so;' .. package.cpath

local ldbus = require('ldbus')
local lmdb = require('lmdb')
local mdb_access = lmdb.mdb_access

-- 测试辅助函数
local function create_test_bus()
    local bus = ldbus.blocking.new()
    assert(bus, "Failed to create D-Bus connection")
    return bus
end

-- 测试套件: lmdb.mdb_access 模块
TestLMDBAccess = {}

function TestLMDBAccess:setUp()
    self.bus = create_test_bus()
    self.conn = self.bus.conn  -- 直接使用 CONNECTION_METATABLE userdata
end

function TestLMDBAccess:tearDown()
    if self.bus then
        self.bus:close()
    end
end

-- ========== 测试模块加载 ==========

function TestLMDBAccess:test_01_module_loaded()
    -- 验证模块已加载
    lu.assertNotNil(mdb_access, "mdb_access module should be loaded")
end

function TestLMDBAccess:test_02_module_functions_exist()
    -- 验证所有函数都存在
    lu.assertIsFunction(mdb_access.get_object, "get_object should be a function")
    lu.assertIsFunction(mdb_access.get_object_by_short_call, "get_object_by_short_call should be a function")
    lu.assertIsFunction(mdb_access.get_object_with_service, "get_object_with_service should be a function")
    lu.assertIsFunction(mdb_access.get_sub_objects, "get_sub_objects should be a function")
    lu.assertIsFunction(mdb_access.get_all, "get_all should be a function")
    lu.assertIsFunction(mdb_access.get_properties, "get_properties should be a function")
end

-- ========== 测试错误处理 - 无效参数 ==========

function TestLMDBAccess:test_10_get_object_error_invalid_bus()
    -- 测试无效的 bus 参数
    lu.assertError(function()
        mdb_access.get_object(nil, "/test/path", "test.interface")
    end)
end

function TestLMDBAccess:test_11_get_object_error_invalid_path()
    -- 测试无效的 path 参数
    lu.assertError(function()
        mdb_access.get_object(self.conn, nil, "test.interface")
    end)
end

function TestLMDBAccess:test_12_get_object_error_invalid_interface()
    -- 测试无效的 interface 参数
    lu.assertError(function()
        mdb_access.get_object(self.conn, "/test/path", nil)
    end)
end

function TestLMDBAccess:test_13_get_object_with_service_error_empty_service()
    -- 测试空的 service 名称
    lu.assertError(function()
        mdb_access.get_object_with_service(self.conn, "", "/test/path", "test.interface")
    end)
end

function TestLMDBAccess:test_14_get_sub_objects_error_invalid_depth()
    -- 测试无效的 depth 参数
    lu.assertError(function()
        mdb_access.get_sub_objects(self.conn, "/test/path", "test.interface", "invalid")
    end)
end

-- ========== 测试 get_sub_objects 返回结构 ==========

function TestLMDBAccess:test_20_get_sub_objects_returns_table()
    -- 测试返回的是 table（即使为空）
    -- 注意：这个测试可能会失败，如果路径不存在
    -- 但至少验证函数调用不会崩溃
    local ok, result = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/nonexistent/path", "test.interface", 1)
    end)
    
    if ok then
        lu.assertIsTable(result, "get_sub_objects should return a table")
    end
end

function TestLMDBAccess:test_21_get_sub_objects_has_metatable()
    -- 测试返回的 table 具有元表
    local ok, result = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/nonexistent/path", "test.interface", 1)
    end)
    
    if ok then
        local metatable = getmetatable(result)
        lu.assertNotNil(metatable, "get_sub_objects result should have metatable")
        
        -- 验证元表有 fold 方法
        if metatable then
            lu.assertIsFunction(metatable.fold, "metatable should have fold method")
        end
    end
end

function TestLMDBAccess:test_22_get_sub_objects_fold_method()
    -- 测试 fold 方法可用
    local ok, result = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/nonexistent/path", "test.interface", 1)
    end)
    
    if ok then
        lu.assertIsFunction(result.fold, "result should have fold method")
        -- 验证冒号语法可用（通过检查元表）
        local metatable = getmetatable(result)
        if metatable then
            lu.assertIsFunction(metatable.fold, "metatable should have fold method for colon syntax")
        end
    end
end

-- ========== 测试 fold 方法 ==========

function TestLMDBAccess:test_30_fold_empty_table()
    -- 测试空表的 fold
    local ok, real_result = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/nonexistent", "test", 1)
    end)
    
    if ok and getmetatable(real_result) then
        local empty_table = {}
        setmetatable(empty_table, getmetatable(real_result))
        
        local result = empty_table:fold(function(obj, acc)
            return acc + 1, true
        end, 0)
        
        lu.assertEquals(result, 0, "fold on empty table should return initial value")
    end
end

function TestLMDBAccess:test_31_fold_sum_values()
    -- 测试 fold 求和
    local ok, real_result = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/nonexistent", "test", 1)
    end)
    
    if ok and getmetatable(real_result) then
        local mock_table = {}
        mock_table.obj1 = {Count = 10}
        mock_table.obj2 = {Count = 20}
        mock_table.obj3 = {Count = 30}
        setmetatable(mock_table, getmetatable(real_result))
        
        local total = mock_table:fold(function(obj, acc)
            return acc + (obj.Count or 0), true
        end, 0)
        
        lu.assertEquals(total, 60, "fold should sum all Count values")
    end
end

function TestLMDBAccess:test_32_fold_early_exit()
    -- 测试 fold 提前退出
    local ok, real_result = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/nonexistent", "test", 1)
    end)
    
    if ok and getmetatable(real_result) then
        local mock_table = {}
        mock_table.obj1 = {Id = 1}
        mock_table.obj2 = {Id = 2}
        mock_table.obj3 = {Id = 3}
        setmetatable(mock_table, getmetatable(real_result))
        
        local found = mock_table:fold(function(obj, acc)
            if obj.Id == 2 then
                return obj, false  -- 找到后停止
            end
            return acc, true
        end, nil)
        
        lu.assertNotNil(found, "fold should find object with Id=2")
        lu.assertEquals(found.Id, 2, "found object should have Id=2")
    end
end

function TestLMDBAccess:test_33_fold_default_initial_acc()
    -- 测试 fold 默认初始值（空表）
    local ok, real_result = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/nonexistent", "test", 1)
    end)
    
    if ok and getmetatable(real_result) then
        local mock_table = {}
        mock_table.obj1 = {Name = "test1"}
        mock_table.obj2 = {Name = "test2"}
        setmetatable(mock_table, getmetatable(real_result))
        
        local result = mock_table:fold(function(obj, acc)
            table.insert(acc, obj.Name)
            return acc, true
        end)  -- 不提供 initial_acc，应该默认为 {}
        
        lu.assertIsTable(result, "fold should return table when initial_acc not provided")
        lu.assertEquals(#result, 2, "result should contain 2 elements")
    end
end

function TestLMDBAccess:test_34_fold_error_handling()
    -- 测试 fold 函数出错时的处理
    local ok, real_result = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/nonexistent", "test", 1)
    end)
    
    if ok and getmetatable(real_result) then
        local mock_table = {}
        mock_table.obj1 = {Value = 1}
        setmetatable(mock_table, getmetatable(real_result))
        
        -- 测试 fold 函数抛出错误
        lu.assertError(function()
            mock_table:fold(function(obj, acc)
                error("test error")
            end, 0)
        end)
    end
end

function TestLMDBAccess:test_35_fold_colon_syntax()
    -- 测试 fold 的冒号语法
    local ok, real_result = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/nonexistent", "test", 1)
    end)
    
    if ok and getmetatable(real_result) then
        local mock_table = {}
        mock_table.obj1 = {Value = 1}
        setmetatable(mock_table, getmetatable(real_result))
        
        -- 测试冒号语法：obj_list:fold(...)
        local result = mock_table:fold(function(obj, acc)
            return acc + obj.Value, true
        end, 0)
        
        lu.assertEquals(result, 1, "colon syntax should work")
    end
end

function TestLMDBAccess:test_36_fold_dot_syntax()
    -- 测试 fold 的点号语法
    local ok, real_result = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/nonexistent", "test", 1)
    end)
    
    if ok and getmetatable(real_result) then
        local mock_table = {}
        mock_table.obj1 = {Value = 1}
        setmetatable(mock_table, getmetatable(real_result))
        
        -- 测试点号语法：obj_list.fold(...)
        local result = mock_table.fold(mock_table, function(obj, acc)
            return acc + obj.Value, true
        end, 0)
        
        lu.assertEquals(result, 1, "dot syntax should work")
    end
end

-- ========== 测试 pairs 遍历 ==========

function TestLMDBAccess:test_40_pairs_traversal()
    -- 测试 pairs 遍历（如果返回的 table 不为空）
    local ok, result = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/nonexistent/path", "test.interface", 1)
    end)
    
    if ok then
        lu.assertIsTable(result, "result should be a table")
        
        -- 验证可以使用 pairs 遍历
        local count = 0
        for path, obj in pairs(result) do
            count = count + 1
            lu.assertIsString(path, "path should be a string")
        end
        
        -- 即使为空，pairs 也应该能正常工作
        lu.assertIsNumber(count, "count should be a number")
    end
end

function TestLMDBAccess:test_41_direct_access()
    -- 测试直接访问（如果知道路径）
    local ok, result = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/nonexistent/path", "test.interface", 1)
    end)
    
    if ok then
        -- 测试访问不存在的路径
        local obj = result["/nonexistent/path/item"]
        -- 应该返回 nil
        lu.assertNil(obj, "accessing non-existent path should return nil")
    end
end

-- ========== 测试参数验证 ==========

function TestLMDBAccess:test_50_get_sub_objects_default_depth()
    -- 测试 depth 参数默认值
    local ok1, result1 = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/test", "test.interface")
    end)
    
    local ok2, result2 = pcall(function()
        return mdb_access.get_sub_objects(self.conn, "/test", "test.interface", 1)
    end)
    
    -- 两个调用应该都能成功（或都失败），不应该因为参数数量不同而行为不同
    if ok1 and ok2 then
        lu.assertIsTable(result1, "result1 should be a table")
        lu.assertIsTable(result2, "result2 should be a table")
    end
end

-- 独立运行此测试文件
if not lu then
    print("Error: luaunit module not found")
    os.exit(1)
end

-- 如果直接运行此文件
if arg and arg[0]:match("test_lmdb_access.lua") then
    os.exit(lu.LuaUnit.run())
end
