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
local lshm_tree = require('lshm_tree')

-- 测试类：基础功能测试
TestShmTreeBasic = {}

function TestShmTreeBasic:setUp()
end

function TestShmTreeBasic:tearDown()
end

-- 测试模块是否加载成功
function TestShmTreeBasic:test_module_loaded()
    lu.assertNotNil(lshm_tree, "lshm_tree module should be loaded")
    lu.assertIsTable(lshm_tree, "lshm_tree should be a table")
end

-- 测试所有函数是否存在
function TestShmTreeBasic:test_all_functions_exist()
    -- MDB 查询函数
    lu.assertNotNil(lshm_tree.get_mdb_object, "get_mdb_object should exist")
    lu.assertNotNil(lshm_tree.get_mdb_sub_paths, "get_mdb_sub_paths should exist")
    lu.assertNotNil(lshm_tree.is_valid_mdb_path, "is_valid_mdb_path should exist")
    lu.assertNotNil(lshm_tree.get_mdb_path, "get_mdb_path should exist")
    lu.assertNotNil(lshm_tree.get_mdb_interface_owners, "get_mdb_interface_owners should exist")
    lu.assertNotNil(lshm_tree.get_mdb_service_name, "get_mdb_service_name should exist")
    lu.assertNotNil(lshm_tree.get_mdb_sub_objects, "get_mdb_sub_objects should exist")
    lu.assertNotNil(lshm_tree.get_mdb_parent_objects, "get_mdb_parent_objects should exist")
    lu.assertNotNil(lshm_tree.get_mdb_service_names, "get_mdb_service_names should exist")
    lu.assertNotNil(lshm_tree.get_mdb_classes, "get_mdb_classes should exist")
    lu.assertNotNil(lshm_tree.get_mdb_object_list, "get_mdb_object_list should exist")
    lu.assertNotNil(lshm_tree.get_mdb_object_owner, "get_mdb_object_owner should exist")
    lu.assertNotNil(lshm_tree.get_mdb_matched_objects, "get_mdb_matched_objects should exist")
    
    -- SHM 属性访问函数
    lu.assertNotNil(lshm_tree.call_shm_get_property, "call_shm_get_property should exist")
    lu.assertNotNil(lshm_tree.call_shm_get_all_properties, "call_shm_get_all_properties should exist")
    lu.assertNotNil(lshm_tree.call_shm_get_properties_by_names, "call_shm_get_properties_by_names should exist")
end

-- 测试所有函数都是可调用的
function TestShmTreeBasic:test_all_functions_callable()
    lu.assertIsFunction(lshm_tree.get_mdb_object, "get_mdb_object should be callable")
    lu.assertIsFunction(lshm_tree.get_mdb_sub_paths, "get_mdb_sub_paths should be callable")
    lu.assertIsFunction(lshm_tree.is_valid_mdb_path, "is_valid_mdb_path should be callable")
    lu.assertIsFunction(lshm_tree.get_mdb_path, "get_mdb_path should be callable")
    lu.assertIsFunction(lshm_tree.get_mdb_interface_owners, "get_mdb_interface_owners should be callable")
    lu.assertIsFunction(lshm_tree.get_mdb_service_name, "get_mdb_service_name should be callable")
    lu.assertIsFunction(lshm_tree.get_mdb_sub_objects, "get_mdb_sub_objects should be callable")
    lu.assertIsFunction(lshm_tree.get_mdb_parent_objects, "get_mdb_parent_objects should be callable")
    lu.assertIsFunction(lshm_tree.get_mdb_service_names, "get_mdb_service_names should be callable")
    lu.assertIsFunction(lshm_tree.get_mdb_classes, "get_mdb_classes should be callable")
    lu.assertIsFunction(lshm_tree.get_mdb_object_list, "get_mdb_object_list should be callable")
    lu.assertIsFunction(lshm_tree.get_mdb_object_owner, "get_mdb_object_owner should be callable")
    lu.assertIsFunction(lshm_tree.get_mdb_matched_objects, "get_mdb_matched_objects should be callable")
    lu.assertIsFunction(lshm_tree.call_shm_get_property, "call_shm_get_property should be callable")
    lu.assertIsFunction(lshm_tree.call_shm_get_all_properties, "call_shm_get_all_properties should be callable")
    lu.assertIsFunction(lshm_tree.call_shm_get_properties_by_names, "call_shm_get_properties_by_names should be callable")
end

-- 测试类：参数验证测试
TestShmTreeParameterValidation = {}

function TestShmTreeParameterValidation:setUp()
end

function TestShmTreeParameterValidation:tearDown()
end

-- 测试 get_mdb_object 参数验证
function TestShmTreeParameterValidation:test_get_mdb_object_missing_args()
    local success = pcall(lshm_tree.get_mdb_object)
    lu.assertFalse(success, "get_mdb_object without args should fail")

    success = pcall(lshm_tree.get_mdb_object, "/path")
    lu.assertFalse(success, "get_mdb_object with 1 arg should fail")
end

function TestShmTreeParameterValidation:test_get_mdb_object_invalid_types()
    -- 测试无法转换为字符串的类型（table）
    local success = pcall(lshm_tree.get_mdb_object, {}, {})
    lu.assertFalse(success, "get_mdb_object with non-string path should fail")

    success = pcall(lshm_tree.get_mdb_object, "/path", "not_array")
    lu.assertFalse(success, "get_mdb_object with non-array interfaces should fail")
end

-- 测试 get_mdb_sub_paths 参数验证
function TestShmTreeParameterValidation:test_get_mdb_sub_paths_missing_args()
    local success = pcall(lshm_tree.get_mdb_sub_paths)
    lu.assertFalse(success, "get_mdb_sub_paths without args should fail")

    success = pcall(lshm_tree.get_mdb_sub_paths, "/path")
    lu.assertFalse(success, "get_mdb_sub_paths with 1 arg should fail")

    success = pcall(lshm_tree.get_mdb_sub_paths, "/path", 1)
    lu.assertFalse(success, "get_mdb_sub_paths with 2 args should fail")
end

function TestShmTreeParameterValidation:test_get_mdb_sub_paths_invalid_types()
    -- 测试无法转换为字符串的类型（table）
    local success = pcall(lshm_tree.get_mdb_sub_paths, {}, 1, {})
    lu.assertFalse(success, "get_mdb_sub_paths with non-string path should fail")

    success = pcall(lshm_tree.get_mdb_sub_paths, "/path", "not_number", {})
    lu.assertFalse(success, "get_mdb_sub_paths with non-number depth should fail")

    success = pcall(lshm_tree.get_mdb_sub_paths, "/path", 1, "not_array")
    lu.assertFalse(success, "get_mdb_sub_paths with non-array interfaces should fail")
end

-- 测试 is_valid_mdb_path 参数验证
function TestShmTreeParameterValidation:test_is_valid_mdb_path_missing_args()
    local success = pcall(lshm_tree.is_valid_mdb_path)
    lu.assertFalse(success, "is_valid_mdb_path without args should fail")

    success = pcall(lshm_tree.is_valid_mdb_path, "/path")
    lu.assertFalse(success, "is_valid_mdb_path with 1 arg should fail")
end

function TestShmTreeParameterValidation:test_is_valid_mdb_path_invalid_types()
    -- 测试无法转换为字符串的类型（table）
    local success = pcall(lshm_tree.is_valid_mdb_path, {}, false)
    lu.assertFalse(success, "is_valid_mdb_path with non-string path should fail")

    success = pcall(lshm_tree.is_valid_mdb_path, "/path", "not_bool")
    lu.assertFalse(success, "is_valid_mdb_path with non-bool ignore_case should fail")
end

-- 测试 get_mdb_path 参数验证
function TestShmTreeParameterValidation:test_get_mdb_path_missing_args()
    local success = pcall(lshm_tree.get_mdb_path)
    lu.assertFalse(success, "get_mdb_path without args should fail")

    success = pcall(lshm_tree.get_mdb_path, "iface")
    lu.assertFalse(success, "get_mdb_path with 1 arg should fail")

    success = pcall(lshm_tree.get_mdb_path, "iface", {})
    lu.assertFalse(success, "get_mdb_path with 2 args should fail")
end

function TestShmTreeParameterValidation:test_get_mdb_path_invalid_types()
    -- 测试无法转换为字符串的类型（table）
    local success = pcall(lshm_tree.get_mdb_path, {}, "{}", false)
    lu.assertFalse(success, "get_mdb_path with non-string interface should fail")

    -- filter_json 必须是字符串，table 无法转换为字符串
    success = pcall(lshm_tree.get_mdb_path, "iface", {}, false)
    lu.assertFalse(success, "get_mdb_path with non-string filter_json should fail")
end

-- 测试 call_shm_get_property 参数验证
function TestShmTreeParameterValidation:test_call_shm_get_property_missing_args()
    local success = pcall(lshm_tree.call_shm_get_property)
    lu.assertFalse(success, "call_shm_get_property without args should fail")

    success = pcall(lshm_tree.call_shm_get_property, "service")
    lu.assertFalse(success, "call_shm_get_property with 1 arg should fail")

    success = pcall(lshm_tree.call_shm_get_property, "service", "/path")
    lu.assertFalse(success, "call_shm_get_property with 2 args should fail")

    success = pcall(lshm_tree.call_shm_get_property, "service", "/path", "iface")
    lu.assertFalse(success, "call_shm_get_property with 3 args should fail")
end

function TestShmTreeParameterValidation:test_call_shm_get_property_invalid_types()
    local success = pcall(lshm_tree.call_shm_get_property, 123, "/path", "iface", "prop")
    lu.assertFalse(success, "call_shm_get_property with non-string service_name should fail")

    success = pcall(lshm_tree.call_shm_get_property, "service", 123, "iface", "prop")
    lu.assertFalse(success, "call_shm_get_property with non-string path should fail")
end

-- 测试 call_shm_get_properties_by_names 参数验证
function TestShmTreeParameterValidation:test_call_shm_get_properties_by_names_missing_args()
    local success = pcall(lshm_tree.call_shm_get_properties_by_names)
    lu.assertFalse(success, "call_shm_get_properties_by_names without args should fail")

    success = pcall(lshm_tree.call_shm_get_properties_by_names, "service")
    lu.assertFalse(success, "call_shm_get_properties_by_names with 1 arg should fail")

    success = pcall(lshm_tree.call_shm_get_properties_by_names, "service", "/path")
    lu.assertFalse(success, "call_shm_get_properties_by_names with 2 args should fail")

    success = pcall(lshm_tree.call_shm_get_properties_by_names, "service", "/path", "iface")
    lu.assertFalse(success, "call_shm_get_properties_by_names with 3 args should fail")
end

function TestShmTreeParameterValidation:test_call_shm_get_properties_by_names_invalid_types()
    local success = pcall(lshm_tree.call_shm_get_properties_by_names, "service", "/path", "iface", "not_array")
    lu.assertFalse(success, "call_shm_get_properties_by_names with non-array prop_names should fail")
end

-- 测试类：边界情况测试
TestShmTreeEdgeCases = {}

function TestShmTreeEdgeCases:setUp()
end

function TestShmTreeEdgeCases:tearDown()
end

-- 测试空字符串参数
function TestShmTreeEdgeCases:test_empty_string_path()
    local result = lshm_tree.is_valid_mdb_path("", false)
    lu.assertIsBoolean(result, "is_valid_mdb_path with empty path should return boolean")
end

function TestShmTreeEdgeCases:test_empty_string_service()
    local result = lshm_tree.get_mdb_service_name("")
    lu.assertIsString(result, "get_mdb_service_name with empty sender should return string")
end

-- 测试空数组参数
function TestShmTreeEdgeCases:test_empty_interfaces_array()
    local result = lshm_tree.get_mdb_object("/nonexistent", {})
    -- 可能返回 nil 或空字典
    lu.assertTrue(result == nil or type(result) == "table",
        "get_mdb_object with empty interfaces should return nil or table")
end

function TestShmTreeEdgeCases:test_empty_prop_names_array()
    local result = lshm_tree.call_shm_get_properties_by_names("service", "/path", "iface", {})
    -- 可能返回 nil 或空数组
    lu.assertTrue(result == nil or type(result) == "table",
        "call_shm_get_properties_by_names with empty prop_names should return nil or table")
end

-- 测试不存在的资源
function TestShmTreeEdgeCases:test_nonexistent_path()
    local result = lshm_tree.is_valid_mdb_path("/nonexistent/path", false)
    lu.assertIsBoolean(result, "is_valid_mdb_path with nonexistent path should return boolean")
    lu.assertFalse(result, "nonexistent path should return false")
end

function TestShmTreeEdgeCases:test_nonexistent_service()
    local result = lshm_tree.get_mdb_service_name(":nonexistent.unique.name")
    lu.assertIsString(result, "get_mdb_service_name with nonexistent sender should return string")
    lu.assertEquals(result, "", "nonexistent sender should return empty string")
end

function TestShmTreeEdgeCases:test_nonexistent_interface()
    local result = lshm_tree.get_mdb_interface_owners("nonexistent.interface")
    lu.assertIsTable(result, "get_mdb_interface_owners should return table")
    lu.assertEquals(#result, 0, "nonexistent interface should return empty array")
end

-- 测试 get_mdb_service_names 无参数调用
function TestShmTreeEdgeCases:test_get_mdb_service_names_no_args()
    local result = lshm_tree.get_mdb_service_names()
    lu.assertIsTable(result, "get_mdb_service_names should return table")
    lu.assertIsNumber(#result, "result should be an array")
end

-- 测试 get_mdb_path 返回格式
function TestShmTreeEdgeCases:test_get_mdb_path_return_format()
    -- get_mdb_path 返回多个值 [path, service_name]
    local path, service = lshm_tree.get_mdb_path("nonexistent.interface", "{}", false)
    lu.assertIsString(path, "get_mdb_path should return path as first value")
    lu.assertIsString(service, "get_mdb_path should return service_name as second value")
end

-- 测试 call_shm_get_properties_by_names 返回格式
function TestShmTreeEdgeCases:test_call_shm_get_properties_by_names_return_format()
    local result = lshm_tree.call_shm_get_properties_by_names("service", "/path", "iface", {"prop1"})
    -- 可能返回 nil 或包含两个字典的数组
    if result ~= nil then
        lu.assertIsTable(result, "result should be table")
        lu.assertGreaterOrEqual(#result, 2, "should return at least 2 values [success_dict, error_dict]")
    end
end

-- 测试类：正常场景测试（需要实际环境）
TestShmTreeNormal = {}

-- 注意：这些测试需要实际的 D-Bus 环境和注册的对象
function TestShmTreeNormal:setUp()
end

function TestShmTreeNormal:tearDown()
end

-- 测试 is_valid_mdb_path 正常调用
function TestShmTreeNormal:test_is_valid_mdb_path_valid_call()
    -- 即使路径不存在，函数也应该能正常调用并返回布尔值
    local result = lshm_tree.is_valid_mdb_path("/test/path", false)
    lu.assertIsBoolean(result, "is_valid_mdb_path should return boolean")
    
    result = lshm_tree.is_valid_mdb_path("/test/path", true)
    lu.assertIsBoolean(result, "is_valid_mdb_path with ignore_case=true should return boolean")
end

-- 测试 get_mdb_service_names 正常调用
function TestShmTreeNormal:test_get_mdb_service_names_valid_call()
    local result = lshm_tree.get_mdb_service_names()
    lu.assertIsTable(result, "get_mdb_service_names should return table")
end

-- 测试 get_mdb_classes 正常调用
function TestShmTreeNormal:test_get_mdb_classes_valid_call()
    local result = lshm_tree.get_mdb_classes("")
    lu.assertIsTable(result, "get_mdb_classes should return table")
    
    result = lshm_tree.get_mdb_classes("nonexistent.service")
    lu.assertIsTable(result, "get_mdb_classes with nonexistent service should return table")
end

-- 测试 get_mdb_object_list 正常调用
function TestShmTreeNormal:test_get_mdb_object_list_valid_call()
    local result = lshm_tree.get_mdb_object_list("nonexistent.class")
    lu.assertIsTable(result, "get_mdb_object_list should return table")
end

-- 测试 get_mdb_object_owner 正常调用
function TestShmTreeNormal:test_get_mdb_object_owner_valid_call()
    local result = lshm_tree.get_mdb_object_owner("nonexistent.object")
    lu.assertIsTable(result, "get_mdb_object_owner should return table")
end

-- 测试 get_mdb_matched_objects 正常调用
function TestShmTreeNormal:test_get_mdb_matched_objects_valid_call()
    local result = lshm_tree.get_mdb_matched_objects("", "")
    lu.assertIsTable(result, "get_mdb_matched_objects should return table")
    
    result = lshm_tree.get_mdb_matched_objects("service", "pattern")
    lu.assertIsTable(result, "get_mdb_matched_objects with args should return table")
end

-- 测试 get_mdb_sub_paths 可选参数
function TestShmTreeNormal:test_get_mdb_sub_paths_optional_args()
    local result = lshm_tree.get_mdb_sub_paths("/path", 1, {})
    lu.assertTrue(result == nil or type(result) == "table",
        "get_mdb_sub_paths with minimal args should return nil or table")
    
    result = lshm_tree.get_mdb_sub_paths("/path", 1, {}, 0)
    lu.assertTrue(result == nil or type(result) == "table",
        "get_mdb_sub_paths with skip should return nil or table")
    
    result = lshm_tree.get_mdb_sub_paths("/path", 1, {}, 0, 10)
    lu.assertTrue(result == nil or type(result) == "table",
        "get_mdb_sub_paths with skip and top should return nil or table")
    
    result = lshm_tree.get_mdb_sub_paths("/path", 1, {}, 0, 10, false)
    lu.assertTrue(result == nil or type(result) == "table",
        "get_mdb_sub_paths with all args should return nil or table")
end

-- 测试返回值类型
function TestShmTreeNormal:test_return_value_types()
    -- is_valid_mdb_path 返回 bool
    local result = lshm_tree.is_valid_mdb_path("/path", false)
    lu.assertIsBoolean(result, "is_valid_mdb_path should return boolean")
    
    -- get_mdb_service_name 返回 string
    result = lshm_tree.get_mdb_service_name(":sender")
    lu.assertIsString(result, "get_mdb_service_name should return string")
    
    -- get_mdb_service_names 返回 array
    result = lshm_tree.get_mdb_service_names()
    lu.assertIsTable(result, "get_mdb_service_names should return table")
    
    -- get_mdb_path 返回多个值 [path, service_name]
    local path, service = lshm_tree.get_mdb_path("iface", "{}", false)
    lu.assertIsString(path, "get_mdb_path should return path as first value")
    lu.assertIsString(service, "get_mdb_path should return service_name as second value")
end

-- 返回测试模块
return {
    TestShmTreeBasic = TestShmTreeBasic,
    TestShmTreeParameterValidation = TestShmTreeParameterValidation,
    TestShmTreeEdgeCases = TestShmTreeEdgeCases,
    TestShmTreeNormal = TestShmTreeNormal
}
