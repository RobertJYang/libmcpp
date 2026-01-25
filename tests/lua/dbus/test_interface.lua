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
local l_interface = ldbus.interface

TestInterface = {}

function TestInterface:setUp()
    -- Create a test interface instance
    self.interface_name = "test.interface.name"
    self.interface = l_interface.new(self.interface_name)
end

function TestInterface:tearDown()
    -- Clean up if necessary
    self.interface = nil
end

function TestInterface:test_set_and_get_property_basic()
    -- Test basic property setting and getting
    local prop_name = "test_property"
    local prop_value = "test_value"
    
    -- Add the property first
    self.interface:add_property(prop_name, "s", prop_value, false, 0)
    
    -- Set the property
    self.interface:set_property(prop_name, prop_value)
    
    -- Get the property and verify
    local retrieved_value = self.interface:get_property(prop_name)
    lu.assertEquals(retrieved_value, prop_value, "Retrieved property should match set value")
end

function TestInterface:test_set_and_get_numeric_property()
    -- Test numeric property
    local prop_name = "numeric_prop"
    local prop_value = 42
    
    self.interface:add_property(prop_name, "i", prop_value, false, 0)
    self.interface:set_property(prop_name, prop_value)
    
    local retrieved_value = self.interface:get_property(prop_name)
    lu.assertEquals(retrieved_value, prop_value, "Numeric property should be preserved")
end

function TestInterface:test_set_and_get_boolean_property()
    -- Test boolean property
    local prop_name = "boolean_prop"
    local prop_value = true
    
    self.interface:add_property(prop_name, "b", prop_value, false, 0)
    self.interface:set_property(prop_name, prop_value)
    
    local retrieved_value = self.interface:get_property(prop_name)
    lu.assertEquals(retrieved_value, prop_value, "Boolean property should be preserved")
end

function TestInterface:test_set_and_get_table_property()
    -- Test complex property like table/array
    local prop_name = "table_prop"
    local prop_value = {key1 = "value1", key2 = 42}
    
    self.interface:add_property(prop_name, "a{sv}", prop_value, false, 0)
    self.interface:set_property(prop_name, prop_value)
    
    local retrieved_value = self.interface:get_property(prop_name)
    lu.assertEquals(retrieved_value.key1, prop_value.key1, "Table property key1 should match")
    lu.assertEquals(retrieved_value.key2, prop_value.key2, "Table property key2 should match")
end

function TestInterface:test_property_not_found_error()
    -- Test error when accessing non-existent property
    local status, err = pcall(function() 
        return self.interface:get_property("non_existent_property") 
    end)
    lu.assertFalse(status, "Should fail when accessing non-existent property")
    lu.assertStrContains(tostring(err), "property not found", "Error message should indicate property not found")
end

function TestInterface:test_add_method()
    -- Test adding a method to the interface
    local method_name = "TestMethod"
    local input_signature = "ss"
    local output_signature = "s"
    local flags = 0
    
    local function test_function()
        return "success"
    end
    
    local status, err = pcall(function()
        self.interface:add_method(method_name, input_signature, output_signature, test_function, flags)
    end)
    
    lu.assertTrue(status, "Method should be added without error")
end

function TestInterface:test_add_signal()
    -- Test adding a signal to the interface
    local signal_name = "TestSignal"
    local signature = "s"
    local flags = 0
    
    local status, err = pcall(function()
        self.interface:add_signal(signal_name, signature, flags)
    end)
    
    lu.assertTrue(status, "Signal should be added without error")
end

function TestInterface:test_readonly_property()
    -- Test behavior with read-only property
    local prop_name = "readonly_prop"
    local initial_value = "initial"
    local new_value = "modified"
    
    -- Add a read-only property
    self.interface:add_property(prop_name, "s", initial_value, true, 0)
    local status, err = pcall(function()
        self.interface:set_property(prop_name, new_value)
    end)
    
    local retrieved_value = self.interface:get_property(prop_name)
    lu.assertEquals(retrieved_value, initial_value, "Read-only property should retain initial value")
end

function TestInterface:test_multiple_properties()
    -- Test multiple properties on same interface
    local props = {
        {name = "prop1", value = "value1", type = "s"},
        {name = "prop2", value = 123, type = "i"},
        {name = "prop3", value = true, type = "b"},
        {name = "prop4", value = 3.14, type = "d"}
    }
    
    -- Add all properties
    for _, prop in ipairs(props) do
        self.interface:add_property(prop.name, prop.type, prop.value, false, 0)
    end
    
    -- Set and verify each property
    for _, prop in ipairs(props) do
        self.interface:set_property(prop.name, prop.value)
        local retrieved = self.interface:get_property(prop.name)
        lu.assertEquals(retrieved, prop.value, string.format("Property %s should match", prop.name))
    end
end

function TestInterface:test_property_signature_handling()
    -- Test different D-Bus signatures
    local test_cases = {
        {name = "string_test", sig = "s", value = "hello"},
        {name = "int32_test", sig = "i", value = 12345},
        {name = "uint32_test", sig = "u", value = 54321},
        {name = "boolean_test", sig = "b", value = true},
        {name = "double_test", sig = "d", value = 3.14159}
    }
    
    for _, case in ipairs(test_cases) do
        self.interface:add_property(case.name, case.sig, case.value, false, 0)
        self.interface:set_property(case.name, case.value)
        
        local retrieved = self.interface:get_property(case.name)
        lu.assertEquals(retrieved, case.value, 
            string.format("Property %s with signature %s should match", case.name, case.sig))
    end
end

return TestInterface
