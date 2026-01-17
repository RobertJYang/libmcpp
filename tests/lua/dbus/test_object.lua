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
local l_object = ldbus.object
TestObject = {}

function TestObject:setUp()
    -- Create a test object instance
    self.object_path = "/test/object/path"
    self.test_interface_name = "test.interface.name"
    self.object = l_object.new(self.object_path)
    
    -- Create and register a test interface
    self.test_interface = l_interface.new(self.test_interface_name)
    self.object:register_interface(self.test_interface)
end

function TestObject:tearDown()
    -- Clean up resources
    self.object = nil
    self.test_interface = nil
end

function TestObject:test_object_creation()
    -- Test that object was created successfully
    lu.assertNotNil(self.object, "Object should be created")
    lu.assertEquals(type(self.object), "userdata", "Object should be userdata type")
end

function TestObject:test_interface_registration()
    -- Test that interface can be registered to object
    local interface_name = "another.test.interface"
    local another_interface = l_interface.new(interface_name)
    
    local status, err = pcall(function()
        self.object:register_interface(another_interface)
    end)
    
    lu.assertTrue(status, "Interface should be registered without error")
end

function TestObject:test_set_and_get_property_on_registered_interface()
    -- Test setting and getting property on registered interface
    local prop_name = "test_property"
    local prop_value = "test_value"
    
    -- Add property to the interface first
    self.test_interface:add_property(prop_name, "s", prop_value, false, 0)
    
    -- Set property through object
    self.object:set_property(self.test_interface_name, prop_name, prop_value)
    
    -- Get property through object
    local retrieved_value = self.object:get_property(self.test_interface_name, prop_name)
    
    lu.assertEquals(retrieved_value, prop_value, "Retrieved property should match set value")
end

function TestObject:test_set_and_get_numeric_property()
    -- Test numeric property through object-interface interaction
    local prop_name = "numeric_prop"
    local prop_value = 42
    
    self.test_interface:add_property(prop_name, "i", prop_value, false, 0)
    self.object:set_property(self.test_interface_name, prop_name, prop_value)
    
    local retrieved_value = self.object:get_property(self.test_interface_name, prop_name)
    lu.assertEquals(retrieved_value, prop_value, "Numeric property should be preserved")
end

function TestObject:test_set_and_get_boolean_property()
    -- Test boolean property through object-interface interaction
    local prop_name = "boolean_prop"
    local prop_value = true
    
    self.test_interface:add_property(prop_name, "b", prop_value, false, 0)
    self.object:set_property(self.test_interface_name, prop_name, prop_value)
    
    local retrieved_value = self.object:get_property(self.test_interface_name, prop_name)
    lu.assertEquals(retrieved_value, prop_value, "Boolean property should be preserved")
end

function TestObject:test_set_and_get_table_property()
    -- Test complex property like table/array through object
    local prop_name = "table_prop"
    local prop_value = {key1 = "value1", key2 = 42}
    
    self.test_interface:add_property(prop_name, "a{sv}", prop_value, false, 0)
    self.object:set_property(self.test_interface_name, prop_name, prop_value)
    
    local retrieved_value = self.object:get_property(self.test_interface_name, prop_name)
    lu.assertEquals(retrieved_value.key1, prop_value.key1, "Table property key1 should match")
    lu.assertEquals(retrieved_value.key2, prop_value.key2, "Table property key2 should match")
end

function TestObject:test_property_not_found_error()
    -- Test error when accessing non-existent property through object
    local status, err = pcall(function() 
        return self.object:get_property(self.test_interface_name, "non_existent_property") 
    end)
    
    lu.assertFalse(status, "Should fail when accessing non-existent property")
    lu.assertStrContains(tostring(err), "property not found", "Error message should indicate property not found")
end

function TestObject:test_interface_not_found_error()
    -- Test error when accessing non-existent interface through object
    local status, err = pcall(function() 
        return self.object:get_property("nonexistent.interface", "some_property") 
    end)
    
    lu.assertFalse(status, "Should fail when accessing non-existent interface")
    lu.assertStrContains(tostring(err), "interface not found", "Error message should indicate interface not found")
end

function TestObject:test_multiple_interfaces_with_same_property_name()
    -- Test multiple interfaces having properties with same name
    local prop_name = "common_property"
    local interface_names = {
        "interface.one",
        "interface.two", 
        "interface.three"
    }
    local values = {
        "value_one",
        "value_two", 
        "value_three"
    }
    
    -- Create and register additional interfaces
    for i, intf_name in ipairs(interface_names) do
        local new_interface = l_interface.new(intf_name)
        self.object:register_interface(new_interface)
        new_interface:add_property(prop_name, "s", values[i], false, 0)
        
        -- Set property on each interface
        self.object:set_property(intf_name, prop_name, values[i])
        
        -- Verify each property separately
        local retrieved = self.object:get_property(intf_name, prop_name)
        lu.assertEquals(retrieved, values[i], 
            string.format("Property on interface %s should have correct value", intf_name))
    end
end

function TestObject:test_multiple_properties_per_interface()
    -- Test multiple properties on the same interface accessed through object
    local properties = {
        {name = "prop1", value = "value1", type = "s"},
        {name = "prop2", value = 123, type = "i"},
        {name = "prop3", value = true, type = "b"},
        {name = "prop4", value = 3.14, type = "d"}
    }
    
    -- Add all properties to the interface
    for _, prop in ipairs(properties) do
        self.test_interface:add_property(prop.name, prop.type, prop.value, false, 0)
    end
    
    -- Set and verify each property through the object
    for _, prop in ipairs(properties) do
        self.object:set_property(self.test_interface_name, prop.name, prop.value)
        local retrieved = self.object:get_property(self.test_interface_name, prop.name)
        lu.assertEquals(retrieved, prop.value, 
            string.format("Property %s should match when accessed through object", prop.name))
    end
end

function TestObject:test_complex_data_types()
    -- Test various complex D-Bus data types
    local test_cases = {
        {name = "array_string", sig = "as", value = {"str1", "str2", "str3"}},
        {name = "array_int", sig = "ai", value = {1, 2, 3, 4}},
        {name = "dictionary", sig = "a{ss}", value = {key1 = "val1", key2 = "val2"}},
        {name = "variant", sig = "v", value = "variant_value"},
        {name = "byte_array", sig = "ay", value = {0x48, 0x65, 0x6C, 0x6C, 0x6F}} -- "Hello" in bytes
    }
    
    for _, case in ipairs(test_cases) do
        self.test_interface:add_property(case.name, case.sig, case.value, false, 0)
        self.object:set_property(self.test_interface_name, case.name, case.value)
        
        local retrieved = self.object:get_property(self.test_interface_name, case.name)
        lu.assertEquals(retrieved, case.value, 
            string.format("Complex property %s should match when accessed through object", case.name))
    end
end

function TestObject:test_concurrent_access_simulation()
    -- Simulate concurrent access patterns to test thread safety aspects
    local test_props = {
        {name = "concurrent_prop1", value = "value1"},
        {name = "concurrent_prop2", value = 2},
        {name = "concurrent_prop3", value = true}
    }
    
    -- Set up properties
    for _, prop in ipairs(test_props) do
        self.test_interface:add_property(prop.name, 
            type(prop.value) == "string" and "s" or 
            type(prop.value) == "number" and "i" or "b", 
            prop.value, false, 0)
    end
    
    -- Multiple set/get cycles to simulate usage patterns
    for cycle = 1, 3 do
        for _, prop in ipairs(test_props) do
            self.object:set_property(self.test_interface_name, prop.name, tostring(prop.value) .. "_cycle_" .. cycle)
            local retrieved = self.object:get_property(self.test_interface_name, prop.name)
            lu.assertEquals(retrieved, tostring(prop.value) .. "_cycle_" .. cycle, 
                string.format("Property %s should match in cycle %d", prop.name, cycle))
        end
    end
end

function TestObject:test_object_path_preservation()
    -- Although object path isn't directly accessible through public API in current implementation,
    -- this test verifies the object maintains its identity
    local status, err = pcall(function()
        -- This call should work without errors related to object state
        local dummy_interface = l_interface.new("dummy.interface")
        self.object:register_interface(dummy_interface)
    end)
    
    lu.assertTrue(status, "Object should maintain its state after operations")
end

return TestObject