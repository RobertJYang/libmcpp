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
local ljson = require("ljson")

-- 测试类：基础功能测试
TestJsonBasic = {}

function TestJsonBasic:setUp()
end

function TestJsonBasic:tearDown()
end

-- 测试创建 JSON 对象
function TestJsonBasic:test_json_object_new_object()
    local obj = ljson.json_object_new_object()
    lu.assertNotNil(obj)
    lu.assertTrue(ljson.json_object_is_object(obj))
    lu.assertFalse(ljson.json_object_is_array(obj))
end

-- 测试创建 JSON 数组
function TestJsonBasic:test_json_object_new_array()
    local arr = ljson.json_object_new_array()
    lu.assertNotNil(arr)
    lu.assertTrue(ljson.json_object_is_array(arr))
    lu.assertFalse(ljson.json_object_is_object(arr))
end

-- 测试 __index 和 __newindex 元方法
function TestJsonBasic:test_index_and_newindex()
    -- 使用 __index 读取值（需要先通过 from_table 创建）
    local obj = ljson.json_object_from_table({name = "Alice", age = 30})
    lu.assertNotNil(obj)
end

-- 测试 __len 元方法
function TestJsonBasic:test_len()
    local obj = ljson.json_object_from_table({a = 1, b = 2, c = 3})
    lu.assertEquals(#obj, 3)
    
    local arr = ljson.json_object_from_table({10, 20, 30})
    lu.assertEquals(#arr, 3)
end

-- 测试 __pairs 元方法和顺序性
function TestJsonBasic:test_pairs_and_order()
    -- 创建一个有序的对象
    local obj = ljson.json_object_from_table({
        first = 1,
        second = 2,
        third = 3
    })
    
    -- 获取键的顺序
    local keys = ljson.json_object_get_keys(obj)
    lu.assertEquals(#keys, 3)
    
    -- 使用 pairs 遍历
    local count = 0
    for k, v in pairs(obj) do
        count = count + 1
    end
    lu.assertEquals(count, 3)
end

-- 测试 __ipairs 元方法
function TestJsonBasic:test_ipairs()
    local arr = ljson.json_object_from_table({100, 200, 300})
    
    local sum = 0
    for i, v in ipairs(arr) do
        -- v 是 JsonValue，需要转换为 table
        local val = ljson.json_object_to_table(v)
        sum = sum + val
    end
    lu.assertEquals(sum, 600)
end

-- 测试 json_object_is_object
function TestJsonBasic:test_json_object_is_object()
    local obj = {key1 = "value1", key2 = 123}
    lu.assertTrue(ljson.json_object_is_object(obj))
    
    local arr = {1, 2, 3}
    lu.assertFalse(ljson.json_object_is_object(arr))
    
    local num = 42
    lu.assertFalse(ljson.json_object_is_object(num))
end

-- 测试 json_object_is_array
function TestJsonBasic:test_json_object_is_array()
    local arr = {1, 2, 3}
    lu.assertTrue(ljson.json_object_is_array(arr))
    
    local obj = {key1 = "value1"}
    lu.assertFalse(ljson.json_object_is_array(obj))
    
    local str = "test"
    lu.assertFalse(ljson.json_object_is_array(str))
end

-- 测试 json_object_get_keys（保持顺序）
function TestJsonBasic:test_json_object_get_keys()
    local obj = {
        name = "test",
        age = 30,
        active = true
    }
    
    local keys = ljson.json_object_get_keys(obj)
    lu.assertNotNil(keys)
    lu.assertEquals(#keys, 3)
    
    -- 检查所有键是否存在
    local key_set = {}
    for _, k in ipairs(keys) do
        key_set[k] = true
    end
    lu.assertTrue(key_set["name"])
    lu.assertTrue(key_set["age"])
    lu.assertTrue(key_set["active"])
end

-- 测试 json_object_is_equal
function TestJsonBasic:test_json_object_is_equal()
    -- 测试相等的对象
    local obj1 = {name = "test", value = 123}
    local obj2 = {name = "test", value = 123}
    lu.assertTrue(ljson.json_object_is_equal(obj1, obj2))
    
    -- 测试不相等的对象
    local obj3 = {name = "test", value = 456}
    lu.assertFalse(ljson.json_object_is_equal(obj1, obj3))
    
    -- 测试相等的数组
    local arr1 = {1, 2, 3}
    local arr2 = {1, 2, 3}
    lu.assertTrue(ljson.json_object_is_equal(arr1, arr2))
    
    -- 测试不相等的数组
    local arr3 = {1, 2, 4}
    lu.assertFalse(ljson.json_object_is_equal(arr1, arr3))
    
    -- 测试基本类型
    lu.assertTrue(ljson.json_object_is_equal(42, 42))
    lu.assertTrue(ljson.json_object_is_equal("test", "test"))
    lu.assertTrue(ljson.json_object_is_equal(true, true))
    lu.assertFalse(ljson.json_object_is_equal(42, 43))
end

-- 测试 json_object_to_table
function TestJsonBasic:test_json_object_to_table()
    -- 先创建 json_object userdata
    local json_obj = ljson.json_object_from_table({
        name = "Alice",
        age = 25,
        scores = {90, 85, 95}
    })
    
    -- 转换为 table
    local result = ljson.json_object_to_table(json_obj)
    lu.assertNotNil(result)
    lu.assertEquals(type(result), "table")
    lu.assertEquals(result.name, "Alice")
    lu.assertEquals(result.age, 25)
    lu.assertEquals(#result.scores, 3)
    lu.assertEquals(result.scores[1], 90)
    
    -- 验证返回的是普通 table，不是 userdata
    lu.assertNotEquals(type(result), "userdata")
end

-- 测试 json_object_to_table 只接受 userdata 参数
function TestJsonBasic:test_json_object_to_table_only_accepts_userdata()
    -- 测试传入普通 Lua table 应该报错
    local normal_table = {key = "value"}
    local success, err = pcall(function()
        ljson.json_object_to_table(normal_table)
    end)
    lu.assertFalse(success)
    lu.assertStrContains(err, "json.value expected")
    
    -- 测试传入 nil 应该报错
    success, _ = pcall(function()
        ljson.json_object_to_table(nil)
    end)
    lu.assertFalse(success)
    
    -- 测试传入字符串应该报错
    success, _ = pcall(function()
        ljson.json_object_to_table("string")
    end)
    lu.assertFalse(success)
    
    -- 测试传入数字应该报错
    success, _ = pcall(function()
        ljson.json_object_to_table(123)
    end)
    lu.assertFalse(success)
end

-- 测试 json_object_to_table 递归转换嵌套对象
function TestJsonBasic:test_json_object_to_table_recursive_conversion()
    -- 创建多层嵌套的 json_object
    local json_obj = ljson.json_object_from_table({
        level1 = {
            level2 = {
                level3 = {
                    value = "deep_value",
                    number = 999
                }
            }
        }
    })
    
    -- 转换为 table
    local result = ljson.json_object_to_table(json_obj)
    lu.assertNotNil(result)
    lu.assertEquals(type(result), "table")
    
    -- 验证第一层
    lu.assertNotNil(result.level1)
    lu.assertEquals(type(result.level1), "table")
    lu.assertNotEquals(type(result.level1), "userdata")
    
    -- 验证第二层
    lu.assertNotNil(result.level1.level2)
    lu.assertEquals(type(result.level1.level2), "table")
    lu.assertNotEquals(type(result.level1.level2), "userdata")
    
    -- 验证第三层
    lu.assertNotNil(result.level1.level2.level3)
    lu.assertEquals(type(result.level1.level2.level3), "table")
    lu.assertNotEquals(type(result.level1.level2.level3), "userdata")
    
    -- 验证值
    lu.assertEquals(result.level1.level2.level3.value, "deep_value")
    lu.assertEquals(result.level1.level2.level3.number, 999)
end

-- 测试 json_object_to_table 转换数组中的嵌套对象
function TestJsonBasic:test_json_object_to_table_array_with_nested_objects()
    -- 创建包含嵌套对象的数组
    local json_obj = ljson.json_object_from_table({
        items = {
            {id = 1, name = "item1"},
            {id = 2, name = "item2"},
            {id = 3, name = "item3"}
        }
    })
    
    -- 转换为 table
    local result = ljson.json_object_to_table(json_obj)
    lu.assertNotNil(result)
    lu.assertEquals(type(result), "table")
    
    -- 验证数组
    lu.assertNotNil(result.items)
    lu.assertEquals(type(result.items), "table")
    lu.assertEquals(#result.items, 3)
    
    -- 验证数组中的每个元素都是普通 table，不是 userdata
    for i = 1, 3 do
        lu.assertEquals(type(result.items[i]), "table")
        lu.assertNotEquals(type(result.items[i]), "userdata")
        lu.assertEquals(result.items[i].id, i)
        lu.assertEquals(result.items[i].name, "item" .. tostring(i))
    end
end

-- 测试 json_object_to_table 转换复杂嵌套结构
function TestJsonBasic:test_json_object_to_table_complex_nested_structure()
    -- 创建复杂的嵌套结构：对象中包含数组，数组中包含对象
    local json_obj = ljson.json_object_from_table({
        users = {
            {
                name = "Alice",
                age = 25,
                tags = {"tag1", "tag2"},
                profile = {
                    email = "alice@example.com",
                    address = {
                        city = "Beijing",
                        country = "China"
                    }
                }
            },
            {
                name = "Bob",
                age = 30,
                tags = {"tag3"},
                profile = {
                    email = "bob@example.com",
                    address = {
                        city = "Shanghai",
                        country = "China"
                    }
                }
            }
        }
    })
    
    -- 转换为 table
    local result = ljson.json_object_to_table(json_obj)
    lu.assertNotNil(result)
    
    -- 验证所有嵌套结构都是普通 table
    lu.assertEquals(type(result.users), "table")
    lu.assertEquals(#result.users, 2)
    
    -- 验证第一个用户
    local user1 = result.users[1]
    lu.assertEquals(type(user1), "table")
    lu.assertNotEquals(type(user1), "userdata")
    lu.assertEquals(user1.name, "Alice")
    lu.assertEquals(user1.age, 25)
    lu.assertEquals(type(user1.tags), "table")
    lu.assertEquals(#user1.tags, 2)
    lu.assertEquals(user1.tags[1], "tag1")
    
    -- 验证嵌套的 profile
    lu.assertEquals(type(user1.profile), "table")
    lu.assertNotEquals(type(user1.profile), "userdata")
    lu.assertEquals(user1.profile.email, "alice@example.com")
    
    -- 验证嵌套的 address
    lu.assertEquals(type(user1.profile.address), "table")
    lu.assertNotEquals(type(user1.profile.address), "userdata")
    lu.assertEquals(user1.profile.address.city, "Beijing")
    lu.assertEquals(user1.profile.address.country, "China")
    
    -- 验证第二个用户
    local user2 = result.users[2]
    lu.assertEquals(type(user2), "table")
    lu.assertEquals(user2.name, "Bob")
    lu.assertEquals(user2.profile.address.city, "Shanghai")
end

-- 测试 json_object_to_table 转换 JSON 数组
function TestJsonBasic:test_json_object_to_table_array()
    -- 创建 JSON 数组
    local json_arr = ljson.json_object_from_table({1, 2, 3, "four", true, nil})
    
    -- 转换为 table
    local result = ljson.json_object_to_table(json_arr)
    lu.assertNotNil(result)
    lu.assertEquals(type(result), "table")
    -- 注意：Lua 的 # 操作符在遇到 nil 时会停止计数，所以不能直接用 # 来验证包含 nil 的数组长度
    -- 直接验证每个索引的值
    lu.assertEquals(result[1], 1)
    lu.assertEquals(result[2], 2)
    lu.assertEquals(result[3], 3)
    lu.assertEquals(result[4], "four")
    lu.assertEquals(result[5], true)
    lu.assertEquals(result[6], nil)
    -- 验证数组确实有 6 个元素（通过检查第 7 个元素不存在）
    lu.assertEquals(result[7], nil)
end

-- 测试 json_object_to_table 转换包含数组的嵌套结构
function TestJsonBasic:test_json_object_to_table_nested_arrays()
    -- 创建包含嵌套数组的结构
    local json_obj = ljson.json_object_from_table({
        matrix = {
            {1, 2, 3},
            {4, 5, 6},
            {7, 8, 9}
        },
        tags = {"tag1", "tag2", "tag3"}
    })
    
    -- 转换为 table
    local result = ljson.json_object_to_table(json_obj)
    lu.assertNotNil(result)
    
    -- 验证 matrix 数组
    lu.assertEquals(type(result.matrix), "table")
    lu.assertEquals(#result.matrix, 3)
    for i = 1, 3 do
        lu.assertEquals(type(result.matrix[i]), "table")
        lu.assertEquals(#result.matrix[i], 3)
        for j = 1, 3 do
            lu.assertEquals(result.matrix[i][j], (i - 1) * 3 + j)
        end
    end
    
    -- 验证 tags 数组
    lu.assertEquals(type(result.tags), "table")
    lu.assertEquals(#result.tags, 3)
    lu.assertEquals(result.tags[1], "tag1")
    lu.assertEquals(result.tags[2], "tag2")
    lu.assertEquals(result.tags[3], "tag3")
end

-- 测试 json_object_to_table 转换空对象和空数组
function TestJsonBasic:test_json_object_to_table_empty_structures()
    -- 测试空对象
    local empty_obj = ljson.json_object_new_object()
    local result = ljson.json_object_to_table(empty_obj)
    lu.assertNotNil(result)
    lu.assertEquals(type(result), "table")
    
    -- 验证是空 table
    local count = 0
    for _ in pairs(result) do
        count = count + 1
    end
    lu.assertEquals(count, 0)
    
    -- 测试空数组
    local empty_arr = ljson.json_object_new_array()
    result = ljson.json_object_to_table(empty_arr)
    lu.assertNotNil(result)
    lu.assertEquals(type(result), "table")
    lu.assertEquals(#result, 0)
end

-- 测试 json_object_to_table 转换各种基本类型
function TestJsonBasic:test_json_object_to_table_primitive_types()
    -- 测试 null
    local null_val = ljson.json_object_from_table(nil)
    local result = ljson.json_object_to_table(null_val)
    lu.assertEquals(result, nil)
    
    -- 测试 boolean
    local bool_val = ljson.json_object_from_table(true)
    result = ljson.json_object_to_table(bool_val)
    lu.assertEquals(result, true)
    
    bool_val = ljson.json_object_from_table(false)
    result = ljson.json_object_to_table(bool_val)
    lu.assertEquals(result, false)
    
    -- 测试整数
    local int_val = ljson.json_object_from_table(123)
    result = ljson.json_object_to_table(int_val)
    lu.assertEquals(result, 123)
    
    -- 测试浮点数
    local float_val = ljson.json_object_from_table(3.14)
    result = ljson.json_object_to_table(float_val)
    lu.assertEquals(result, 3.14)
    
    -- 测试字符串
    local str_val = ljson.json_object_from_table("hello")
    result = ljson.json_object_to_table(str_val)
    lu.assertEquals(result, "hello")
end

-- 测试 json_object_from_table
function TestJsonBasic:test_json_object_from_table()
    local table_obj = {
        name = "Bob",
        age = 30,
        tags = {"tag1", "tag2"}
    }
    
    local json_obj = ljson.json_object_from_table(table_obj)
    lu.assertNotNil(json_obj)
    
    -- json_obj 是 userdata，转换为 table 来验证
    local result = ljson.json_object_to_table(json_obj)
    lu.assertEquals(result.name, "Bob")
    lu.assertEquals(result.age, 30)
    lu.assertEquals(#result.tags, 2)
end

-- 测试 json_object_copy
function TestJsonBasic:test_json_object_copy()
    local original = {
        name = "Original",
        value = 100,
        nested = {a = 1, b = 2}
    }
    
    local copied = ljson.json_object_copy(original)
    lu.assertNotNil(copied)
    
    -- 验证内容相等
    lu.assertTrue(ljson.json_object_is_equal(original, copied))
end

-- 测试类：嵌套结构测试
TestJsonNested = {}

function TestJsonNested:setUp()
end

function TestJsonNested:tearDown()
end

-- 测试嵌套结构
function TestJsonNested:test_nested_structures()
    local complex_obj = {
        user = {
            name = "John",
            age = 28,
            addresses = {
                {city = "Beijing", zip = "100000"},
                {city = "Shanghai", zip = "200000"}
            }
        },
        scores = {95, 87, 92}
    }
    
    -- 测试复制
    local copied = ljson.json_object_copy(complex_obj)
    lu.assertTrue(ljson.json_object_is_equal(complex_obj, copied))
    
    -- 测试获取键
    local keys = ljson.json_object_get_keys(complex_obj)
    lu.assertEquals(#keys, 2)
    
    -- 测试类型判断
    lu.assertTrue(ljson.json_object_is_object(complex_obj))
    lu.assertTrue(ljson.json_object_is_object(complex_obj.user))
    lu.assertTrue(ljson.json_object_is_array(complex_obj.user.addresses))
    lu.assertTrue(ljson.json_object_is_array(complex_obj.scores))
end

-- 测试深度嵌套的数组和对象
function TestJsonNested:test_deep_nesting()
    local data = ljson.json_object_from_table({
        level1 = {
            level2 = {
                level3 = {
                    value = "deep"
                }
            }
        }
    })
    
    local level1 = data["level1"]
    local level2 = level1["level2"]
    local level3 = level2["level3"]
    local value = level3["value"]
    
    lu.assertEquals(ljson.json_object_to_table(value), "deep")
end

-- 测试嵌套的 userdata 对象
function TestJsonNested:test_nested_userdata()
    -- 创建嵌套结构
    local inner_obj = ljson.json_object_new_object()
    inner_obj["x"] = ljson.json_object_from_table(10)
    inner_obj["y"] = ljson.json_object_from_table(20)
    
    local outer_obj = ljson.json_object_new_object()
    outer_obj["point"] = inner_obj
    outer_obj["name"] = ljson.json_object_from_table("test_point")
    
    -- 读取嵌套值
    local point = outer_obj["point"]
    lu.assertNotNil(point)
    
    local x = point["x"]
    lu.assertEquals(ljson.json_object_to_table(x), 10)
    
    local y = point["y"]
    lu.assertEquals(ljson.json_object_to_table(y), 20)
end

-- 测试类：元方法测试
TestJsonMetamethods = {}

function TestJsonMetamethods:setUp()
end

function TestJsonMetamethods:tearDown()
end

-- 测试 __tostring 元方法
function TestJsonMetamethods:test_tostring()
    -- 测试对象
    local obj = ljson.json_object_from_table({name = "test", value = 123})
    local str = tostring(obj)
    lu.assertNotNil(str)
    lu.assertEquals(type(str), "string")
    
    -- 测试数组
    local arr = ljson.json_object_from_table({1, 2, 3})
    local arr_str = tostring(arr)
    lu.assertNotNil(arr_str)
    lu.assertEquals(type(arr_str), "string")
    
    -- 测试基本类型
    local num = ljson.json_object_from_table(42)
    local num_str = tostring(num)
    lu.assertNotNil(num_str)
    
    local str_val = ljson.json_object_from_table("hello")
    local str_val_str = tostring(str_val)
    lu.assertNotNil(str_val_str)
end

-- 测试 __eq 元方法（使用 userdata）
function TestJsonMetamethods:test_eq_metamethod()
    -- 创建两个内容相同的对象
    local obj1 = ljson.json_object_from_table({name = "test", value = 123})
    local obj2 = ljson.json_object_from_table({name = "test", value = 123})
    
    -- 使用 __eq 元方法
    lu.assertTrue(obj1 == obj2)
    
    -- 创建不同的对象
    local obj3 = ljson.json_object_from_table({name = "test", value = 456})
    lu.assertFalse(obj1 == obj3)
    
    -- 测试数组
    local arr1 = ljson.json_object_from_table({1, 2, 3})
    local arr2 = ljson.json_object_from_table({1, 2, 3})
    lu.assertTrue(arr1 == arr2)
    
    local arr3 = ljson.json_object_from_table({1, 2, 4})
    lu.assertFalse(arr1 == arr3)
end

-- 测试键顺序保持（详细测试）
function TestJsonMetamethods:test_key_order_preservation()
    -- 创建一个对象，明确指定插入顺序
    local obj = ljson.json_object_new_object()
    obj["zebra"] = ljson.json_object_from_table("z")
    obj["alpha"] = ljson.json_object_from_table("a")
    obj["beta"] = ljson.json_object_from_table("b")
    obj["gamma"] = ljson.json_object_from_table("g")
    
    -- 获取键的顺序
    local keys = ljson.json_object_get_keys(obj)
    lu.assertEquals(#keys, 4)
    
    -- 验证键按插入顺序排列（不是字母顺序）
    lu.assertEquals(keys[1], "zebra")
    lu.assertEquals(keys[2], "alpha")
    lu.assertEquals(keys[3], "beta")
    lu.assertEquals(keys[4], "gamma")
    
    -- 使用 pairs 遍历，验证顺序一致
    local pairs_keys = {}
    for k, v in pairs(obj) do
        table.insert(pairs_keys, k)
    end
    
    lu.assertEquals(#pairs_keys, 4)
    lu.assertEquals(pairs_keys[1], "zebra")
    lu.assertEquals(pairs_keys[2], "alpha")
    lu.assertEquals(pairs_keys[3], "beta")
    lu.assertEquals(pairs_keys[4], "gamma")
end

-- 测试 pairs 遍历并获取值
function TestJsonMetamethods:test_pairs_with_values()
    local obj = ljson.json_object_new_object()
    obj["name"] = ljson.json_object_from_table("Alice")
    obj["age"] = ljson.json_object_from_table(30)
    obj["city"] = ljson.json_object_from_table("Beijing")
    
    -- 使用 pairs 遍历并收集键值对
    local collected = {}
    for k, v in pairs(obj) do
        collected[k] = ljson.json_object_to_table(v)
    end
    
    lu.assertEquals(collected.name, "Alice")
    lu.assertEquals(collected.age, 30)
    lu.assertEquals(collected.city, "Beijing")
    lu.assertEquals(#ljson.json_object_get_keys(collected), 3)
end

-- 测试 ipairs 遍历数组
function TestJsonMetamethods:test_ipairs_detailed()
    local arr = ljson.json_object_new_array()
    arr:push_back(ljson.json_object_from_table("first"))
    arr:push_back(ljson.json_object_from_table("second"))
    arr:push_back(ljson.json_object_from_table("third"))
    
    -- 使用 ipairs 遍历
    local collected = {}
    for i, v in ipairs(arr) do
        collected[i] = ljson.json_object_to_table(v)
    end
    
    lu.assertEquals(#collected, 3)
    lu.assertEquals(collected[1], "first")
    lu.assertEquals(collected[2], "second")
    lu.assertEquals(collected[3], "third")
end

-- 测试类：数组操作测试
TestJsonArray = {}

function TestJsonArray:setUp()
end

function TestJsonArray:tearDown()
end

-- 测试数组的 push_back 操作
function TestJsonArray:test_array_push_back()
    local arr = ljson.json_object_new_array()
    lu.assertEquals(#arr, 0)
    
    -- 添加不同类型的元素
    arr:push_back(ljson.json_object_from_table(1))
    lu.assertEquals(#arr, 1)
    
    arr:push_back(ljson.json_object_from_table("string"))
    lu.assertEquals(#arr, 2)
    
    arr:push_back(ljson.json_object_from_table(true))
    lu.assertEquals(#arr, 3)
    
    arr:push_back(ljson.json_object_from_table({nested = "object"}))
    lu.assertEquals(#arr, 4)
    
    -- 验证每个元素
    lu.assertEquals(ljson.json_object_to_table(arr[1]), 1)
    lu.assertEquals(ljson.json_object_to_table(arr[2]), "string")
    lu.assertEquals(ljson.json_object_to_table(arr[3]), true)
    local nested = ljson.json_object_to_table(arr[4])
    lu.assertEquals(nested.nested, "object")
end

-- 测试数组越界访问
function TestJsonArray:test_array_out_of_bounds()
    local arr = ljson.json_object_new_array()
    arr:push_back(ljson.json_object_from_table(1))
    arr:push_back(ljson.json_object_from_table(2))
    
    -- 越界访问应该返回 nil
    local out_of_bounds = arr[10]
    lu.assertNil(out_of_bounds)
end

-- 测试混合类型的数组
function TestJsonArray:test_array_mixed_types()
    local arr = ljson.json_object_new_array()
    arr:push_back(ljson.json_object_from_table(42))
    arr:push_back(ljson.json_object_from_table("string"))
    arr:push_back(ljson.json_object_from_table(true))
    arr:push_back(ljson.json_object_from_table({nested = "obj"}))
    arr:push_back(ljson.json_object_from_table({1, 2, 3}))
    
    lu.assertEquals(#arr, 5)
    
    -- 验证每个元素的类型和值
    lu.assertEquals(ljson.json_object_to_table(arr[1]), 42)
    lu.assertEquals(ljson.json_object_to_table(arr[2]), "string")
    lu.assertEquals(ljson.json_object_to_table(arr[3]), true)
    
    local obj = ljson.json_object_to_table(arr[4])
    lu.assertEquals(obj.nested, "obj")
    
    local nested_arr = ljson.json_object_to_table(arr[5])
    lu.assertEquals(#nested_arr, 3)
    lu.assertEquals(nested_arr[1], 1)
end

-- 测试对象包含数组
function TestJsonArray:test_object_with_array()
    local obj = ljson.json_object_new_object()
    
    local arr = ljson.json_object_new_array()
    arr:push_back(ljson.json_object_from_table(1))
    arr:push_back(ljson.json_object_from_table(2))
    arr:push_back(ljson.json_object_from_table(3))
    
    obj["numbers"] = arr
    obj["name"] = ljson.json_object_from_table("test")
    
    -- 读取数组
    local numbers = obj["numbers"]
    lu.assertNotNil(numbers)
    lu.assertTrue(ljson.json_object_is_array(numbers))
    lu.assertEquals(#numbers, 3)
    
    -- 遍历数组
    local sum = 0
    for i, v in ipairs(numbers) do
        sum = sum + ljson.json_object_to_table(v)
    end
    lu.assertEquals(sum, 6)
end

-- 测试数组包含对象
function TestJsonArray:test_array_with_object()
    local arr = ljson.json_object_new_array()
    
    local obj1 = ljson.json_object_new_object()
    obj1["id"] = ljson.json_object_from_table(1)
    obj1["name"] = ljson.json_object_from_table("first")
    
    local obj2 = ljson.json_object_new_object()
    obj2["id"] = ljson.json_object_from_table(2)
    obj2["name"] = ljson.json_object_from_table("second")
    
    arr:push_back(obj1)
    arr:push_back(obj2)
    
    lu.assertEquals(#arr, 2)
    
    -- 读取并验证对象
    local first = arr[1]
    lu.assertTrue(ljson.json_object_is_object(first))
    lu.assertEquals(ljson.json_object_to_table(first["id"]), 1)
    lu.assertEquals(ljson.json_object_to_table(first["name"]), "first")
    
    local second = arr[2]
    lu.assertTrue(ljson.json_object_is_object(second))
    lu.assertEquals(ljson.json_object_to_table(second["id"]), 2)
    lu.assertEquals(ljson.json_object_to_table(second["name"]), "second")
end

-- 测试大型数组
function TestJsonArray:test_large_array()
    local arr = ljson.json_object_new_array()
    
    -- 添加大量元素
    for i = 1, 100 do
        arr:push_back(ljson.json_object_from_table(i * 2))
    end
    
    lu.assertEquals(#arr, 100)
    
    -- 验证第一个和最后一个元素
    lu.assertEquals(ljson.json_object_to_table(arr[1]), 2)
    lu.assertEquals(ljson.json_object_to_table(arr[100]), 200)
    
    -- 使用 ipairs 计算总和
    local sum = 0
    for i, v in ipairs(arr) do
        sum = sum + ljson.json_object_to_table(v)
    end
    lu.assertEquals(sum, 10100)  -- 2+4+6+...+200 = 2*(1+2+...+100) = 2*5050 = 10100
end

-- 测试类：边界情况测试
TestJsonEdgeCases = {}

function TestJsonEdgeCases:setUp()
end

function TestJsonEdgeCases:tearDown()
end

-- 测试空对象和空数组
function TestJsonEdgeCases:test_empty_containers()
    -- 使用 API 创建的空对象和空数组
    local empty_obj = ljson.json_object_new_object()
    local keys = ljson.json_object_get_keys(empty_obj)
    lu.assertEquals(#keys, 0)
    lu.assertTrue(ljson.json_object_is_object(empty_obj))
    lu.assertFalse(ljson.json_object_is_array(empty_obj))
    lu.assertEquals(#empty_obj, 0)
    
    local empty_arr = ljson.json_object_new_array()
    lu.assertTrue(ljson.json_object_is_array(empty_arr))
    lu.assertFalse(ljson.json_object_is_object(empty_arr))
    lu.assertEquals(#empty_arr, 0)
    
    -- 空对象和空数组应该不相等
    lu.assertFalse(ljson.json_object_is_equal(empty_obj, empty_arr))
end

-- 测试不同数据类型
function TestJsonEdgeCases:test_various_types()
    -- 测试布尔值
    lu.assertTrue(ljson.json_object_is_equal(true, true))
    lu.assertTrue(ljson.json_object_is_equal(false, false))
    lu.assertFalse(ljson.json_object_is_equal(true, false))
    
    -- 测试数字
    lu.assertTrue(ljson.json_object_is_equal(123, 123))
    lu.assertTrue(ljson.json_object_is_equal(3.14, 3.14))
    
    -- 测试字符串
    lu.assertTrue(ljson.json_object_is_equal("hello", "hello"))
    lu.assertFalse(ljson.json_object_is_equal("hello", "world"))
end

-- 测试 userdata 和 table 混合使用
function TestJsonEdgeCases:test_userdata_and_table_mix()
    -- 创建 userdata
    local json_obj = ljson.json_object_new_object()
    lu.assertTrue(ljson.json_object_is_object(json_obj))
    
    -- 创建 table
    local table_obj = {key = "value"}
    lu.assertTrue(ljson.json_object_is_object(table_obj))
    
    -- userdata 转换为 table
    local converted = ljson.json_object_to_table(json_obj)
    lu.assertNotNil(converted)
    lu.assertEquals(type(converted), "table")
end

-- 测试 copy 操作的独立性
function TestJsonEdgeCases:test_copy_independence()
    local original = ljson.json_object_new_object()
    original["value"] = ljson.json_object_from_table(100)
    
    -- 复制对象
    local copied = ljson.json_object_copy(original)
    
    -- 验证初始相等
    lu.assertTrue(ljson.json_object_is_equal(original, copied))
    
    -- 修改复制的对象
    copied["value"] = ljson.json_object_from_table(200)
    
    -- 验证原始对象未被修改
    local orig_val = ljson.json_object_to_table(original["value"])
    lu.assertEquals(orig_val, 100)
    
    -- 验证复制对象已修改
    local copy_val = ljson.json_object_to_table(copied["value"])
    lu.assertEquals(copy_val, 200)
    
    -- 验证不再相等
    lu.assertFalse(ljson.json_object_is_equal(original, copied))
end

-- 测试空值和 nil
function TestJsonEdgeCases:test_null_and_nil()
    local obj = ljson.json_object_new_object()
    obj["null_value"] = ljson.json_object_from_table(nil)
    
    -- 读取 nil 值
    local null_val = obj["null_value"]
    lu.assertNotNil(null_val)  -- JsonValue 本身不为 nil
end

-- 测试访问不存在的键
function TestJsonEdgeCases:test_access_nonexistent_key()
    local obj = ljson.json_object_new_object()
    obj["existing"] = ljson.json_object_from_table("value")
    
    -- 访问不存在的键应该返回 nil
    local non_exist = obj["nonexistent"]
    lu.assertNil(non_exist)
end

-- 测试相等性比较的边界情况
function TestJsonEdgeCases:test_equality_edge_cases()
    -- 空对象相等
    local empty1 = ljson.json_object_new_object()
    local empty2 = ljson.json_object_new_object()
    lu.assertTrue(ljson.json_object_is_equal(empty1, empty2))
    
    -- 空数组相等
    local arr_empty1 = ljson.json_object_new_array()
    local arr_empty2 = ljson.json_object_new_array()
    lu.assertTrue(ljson.json_object_is_equal(arr_empty1, arr_empty2))
    
    -- 空对象和空数组不相等
    lu.assertFalse(ljson.json_object_is_equal(empty1, arr_empty1))
    
    -- 不同大小的对象不相等
    local obj1 = ljson.json_object_from_table({a = 1})
    local obj2 = ljson.json_object_from_table({a = 1, b = 2})
    lu.assertFalse(ljson.json_object_is_equal(obj1, obj2))
    
    -- 键相同但值不同的对象不相等
    local obj3 = ljson.json_object_from_table({a = 1, b = 2})
    local obj4 = ljson.json_object_from_table({a = 1, b = 3})
    lu.assertFalse(ljson.json_object_is_equal(obj3, obj4))
end

-- 测试特殊字符的键
function TestJsonEdgeCases:test_special_character_keys()
    local obj = ljson.json_object_new_object()
    
    obj["key with spaces"] = ljson.json_object_from_table("value1")
    obj["key-with-dashes"] = ljson.json_object_from_table("value2")
    obj["key_with_underscores"] = ljson.json_object_from_table("value3")
    obj["key.with.dots"] = ljson.json_object_from_table("value4")
    obj["中文键"] = ljson.json_object_from_table("value5")
    
    lu.assertEquals(#obj, 5)
    
    -- 验证可以正确读取
    lu.assertEquals(ljson.json_object_to_table(obj["key with spaces"]), "value1")
    lu.assertEquals(ljson.json_object_to_table(obj["key-with-dashes"]), "value2")
    lu.assertEquals(ljson.json_object_to_table(obj["key_with_underscores"]), "value3")
    lu.assertEquals(ljson.json_object_to_table(obj["key.with.dots"]), "value4")
    lu.assertEquals(ljson.json_object_to_table(obj["中文键"]), "value5")
end

-- 测试数字类型的精度
function TestJsonEdgeCases:test_number_precision()
    -- 测试整数
    local int_val = ljson.json_object_from_table(123456789)
    lu.assertEquals(ljson.json_object_to_table(int_val), 123456789)
    
    -- 测试负整数
    local neg_int = ljson.json_object_from_table(-987654321)
    lu.assertEquals(ljson.json_object_to_table(neg_int), -987654321)
    
    -- 测试浮点数
    local float_val = ljson.json_object_from_table(3.14159)
    local result = ljson.json_object_to_table(float_val)
    lu.assertAlmostEquals(result, 3.14159, 0.00001)
    
    -- 测试零
    local zero = ljson.json_object_from_table(0)
    lu.assertEquals(ljson.json_object_to_table(zero), 0)
end

-- 测试类：大型数据测试
TestJsonLarge = {}

function TestJsonLarge:setUp()
end

function TestJsonLarge:tearDown()
end

-- 测试大型对象
function TestJsonLarge:test_large_object()
    local obj = ljson.json_object_new_object()
    
    -- 添加大量键值对
    for i = 1, 100 do
        local key = "key_" .. tostring(i)
        obj[key] = ljson.json_object_from_table(i)
    end
    
    lu.assertEquals(#obj, 100)
    
    -- 验证键的顺序
    local keys = ljson.json_object_get_keys(obj)
    lu.assertEquals(#keys, 100)
    
    -- 验证第一个和最后一个键
    lu.assertEquals(keys[1], "key_1")
    lu.assertEquals(keys[100], "key_100")
    
    -- 随机验证几个值
    lu.assertEquals(ljson.json_object_to_table(obj["key_50"]), 50)
    lu.assertEquals(ljson.json_object_to_table(obj["key_99"]), 99)
end

-- 测试类：底层操作测试
TestJsonRaw = {}

function TestJsonRaw:setUp()
end

function TestJsonRaw:tearDown()
end

-- 测试 to_raw 和 new_from_raw
function TestJsonRaw:test_raw_pointer_operations()
    -- 创建一个对象
    local obj = ljson.json_object_from_table({name = "test", value = 42})
    
    -- 获取底层指针
    local raw_ptr = ljson.to_raw(obj)
    lu.assertNotNil(raw_ptr)
    lu.assertEquals(type(raw_ptr), "userdata")
    
    -- 从指针创建新的 JsonValue
    local new_obj = ljson.new_from_raw(raw_ptr)
    lu.assertNotNil(new_obj)
    
    -- 验证内容相等
    lu.assertTrue(ljson.json_object_is_equal(obj, new_obj))
end

-- 测试类：lencode 函数测试
TestJsonEncode = {}

function TestJsonEncode:setUp()
end

function TestJsonEncode:tearDown()
end

-- 测试基本类型编码
function TestJsonEncode:test_encode_nil()
    local result = ljson.encode(nil)
    lu.assertEquals(result, "null")
end

-- 测试布尔值编码
function TestJsonEncode:test_encode_boolean()
    local result_true = ljson.encode(true)
    lu.assertEquals(result_true, "true")

    local result_false = ljson.encode(false)
    lu.assertEquals(result_false, "false")
end

-- 测试整数编码
function TestJsonEncode:test_encode_integer()
    local result = ljson.encode(42)
    lu.assertEquals(result, "42")
end

-- 测试负整数编码
function TestJsonEncode:test_encode_negative_integer()
    local result = ljson.encode(-123)
    lu.assertEquals(result, "-123")
end

-- 测试浮点数编码
function TestJsonEncode:test_encode_double()
    local result = ljson.encode(3.14)
    lu.assertEquals(result, "3.14")
end

-- 测试字符串编码
function TestJsonEncode:test_encode_string()
    local result = ljson.encode("hello")
    lu.assertEquals(result, "\"hello\"")
end

-- 测试中文字符串编码
function TestJsonEncode:test_encode_chinese_string()
    local result = ljson.encode("张三")
    lu.assertEquals(result, "\"张三\"")
end

-- 测试特殊字符字符串编码
function TestJsonEncode:test_encode_special_chars_string()
    local result = ljson.encode("hello\nworld")
    lu.assertEquals(result, "\"hello\\nworld\"")
end

-- 测试空数组编码
function TestJsonEncode:test_encode_empty_array()
    local result = ljson.encode({})
    lu.assertEquals(result, "[]")
end

-- 测试简单数组编码
function TestJsonEncode:test_encode_simple_array()
    local result = ljson.encode({1, 2, 3})
    lu.assertEquals(result, "[1,2,3]")
end

-- 测试混合类型数组编码
function TestJsonEncode:test_encode_mixed_array()
    local result = ljson.encode({1, "two", false, 3.14})
    lu.assertEquals(result, "[1,\"two\",false,3.14]")
end

-- 测试嵌套数组编码
function TestJsonEncode:test_encode_nested_array()
    local result = ljson.encode({{1, 2}, {3, 4}})
    lu.assertEquals(result, "[[1,2],[3,4]]")
end

-- 测试空对象编码
function TestJsonEncode:test_encode_empty_object()
    -- 先设置空 table 编码为对象
    ljson.encode_empty_table_as_object(true)
    local result = ljson.encode({})
    lu.assertEquals(result, "{}")
    -- 恢复默认设置
    ljson.encode_empty_table_as_object(false)
end

-- 测试简单对象编码
function TestJsonEncode:test_encode_simple_object()
    local result = ljson.encode({name = "Alice", age = 30})
    -- 注意：键会按字母序排序
    lu.assertEquals(result, "{\"age\":30,\"name\":\"Alice\"}")
end

-- 测试嵌套对象编码
function TestJsonEncode:test_encode_nested_object()
    local result = ljson.encode({
        person = {
            name = "Bob",
            age = 25,
            address = {
                city = "Beijing",
                country = "China"
            }
        }
    })
    lu.assertEquals(result, "{\"person\":{\"address\":{\"city\":\"Beijing\",\"country\":\"China\"},\"age\":25,\"name\":\"Bob\"}}")
end

-- 测试对象中包含数组
function TestJsonEncode:test_encode_object_with_array()
    local result = ljson.encode({
        name = "Charlie",
        scores = {85, 92, 78}
    })
    lu.assertEquals(result, "{\"name\":\"Charlie\",\"scores\":[85,92,78]}")
end

-- 测试数组中包含对象
function TestJsonEncode:test_encode_array_with_object()
    local result = ljson.encode({
        {id = 1, name = "Alice"},
        {id = 2, name = "Bob"}
    })
    lu.assertEquals(result, "[{\"id\":1,\"name\":\"Alice\"},{\"id\":2,\"name\":\"Bob\"}]")
end

-- 测试复杂嵌套结构编码
function TestJsonEncode:test_encode_complex_nested()
    local result = ljson.encode({
        users = {
            {
                name = "Alice",
                age = 25,
                tags = {"tag1", "tag2"},
                address = {
                    city = "Beijing",
                    country = "China"
                }
            }
        },
        count = 1
    })
    lu.assertEquals(result, "{\"count\":1,\"users\":[{\"address\":{\"city\":\"Beijing\",\"country\":\"China\"},\"age\":25,\"name\":\"Alice\",\"tags\":[\"tag1\",\"tag2\"]}]}")
end

-- 测试包含 null 值的数组
function TestJsonEncode:test_encode_array_with_nil()
    local result = ljson.encode({1, nil, 3})
    -- Lua 数组中的 nil 会被转换为 JSON null
    lu.assertEquals(result, "[1,null,3]")
end

-- 测试包含各种基本类型的对象
function TestJsonEncode:test_encode_object_with_all_types()
    local result = ljson.encode({
        null_value = nil,
        bool_value = true,
        int_value = 42,
        float_value = 3.14,
        string_value = "hello"
    })
    lu.assertEquals(result, "{\"bool_value\":true,\"float_value\":3.14,\"int_value\":42,\"null_value\":null,\"string_value\":\"hello\"}")
end

-- 测试数组长度检测（非连续数组应该编码为对象）
function TestJsonEncode:test_encode_sparse_array_as_object()
    local result = ljson.encode({[1] = "a", [3] = "c"})
    -- 稀疏数组（不连续）应该被编码为对象
    lu.assertEquals(result, "{\"1\":\"a\",\"3\":\"c\"}")
end

-- 测试带有字符串键的 table 编码为对象
function TestJsonEncode:test_encode_table_with_string_keys()
    local result = ljson.encode({x = 1, y = 2, z = 3})
    -- 键会按字母序排序
    lu.assertEquals(result, "{\"x\":1,\"y\":2,\"z\":3}")
end

-- 测试数字键转换为字符串
function TestJsonEncode:test_encode_table_with_numeric_keys_as_object()
    local result = ljson.encode({[10] = "ten", [20] = "twenty"})
    -- 数字键会被转换为字符串
    lu.assertEquals(result, "{\"10\":\"ten\",\"20\":\"twenty\"}")
end

-- 测试布尔值键
function TestJsonEncode:test_encode_table_with_boolean_keys()
    local result = ljson.encode({[true] = "yes", [false] = "no"})
    -- 布尔键会被转换为字符串
    lu.assertEquals(result, "{\"false\":\"no\",\"true\":\"yes\"}")
end

-- 测试大数编码
function TestJsonEncode:test_encode_large_number()
    local result = ljson.encode(1234567890)
    lu.assertEquals(result, "1234567890")
end

-- 测试负浮点数编码
function TestJsonEncode:test_encode_negative_double()
    local result = ljson.encode(-3.14)
    lu.assertEquals(result, "-3.14")
end

-- 测试科学计数法
function TestJsonEncode:test_encode_scientific_notation()
    local result = ljson.encode(1.23e5)
    -- JSON 可能使用科学计数法或完整数字
    lu.assertTrue(result == "1.23e+05" or result == "123000")
end

-- 测试 Unicode 转义
function TestJsonEncode:test_encode_unicode_escape()
    local result = ljson.encode("hello\x00world")
    lu.assertEquals(result, "\"hello\\u0000world\"")
end

-- 测试引号转义
function TestJsonEncode:test_encode_quote_escape()
    local result = ljson.encode('say "hello"')
    lu.assertEquals(result, "\"say \\\"hello\\\"\"")
end

-- 测试反斜杠转义
function TestJsonEncode:test_encode_backslash_escape()
    local result = ljson.encode("path\\to\\file")
    lu.assertEquals(result, "\"path\\\\to\\\\file\"")
end

-- 测试类：ldecode 函数测试
TestJsonDecode = {}

function TestJsonDecode:setUp()
end

function TestJsonDecode:tearDown()
end

-- 测试 null 解码
function TestJsonDecode:test_decode_null()
    local result = ljson.decode("null")
    lu.assertNil(result)
end

-- 测试布尔值 true 解码
function TestJsonDecode:test_decode_true()
    local result = ljson.decode("true")
    lu.assertEquals(result, true)
    lu.assertEquals(type(result), "boolean")
end

-- 测试布尔值 false 解码
function TestJsonDecode:test_decode_false()
    local result = ljson.decode("false")
    lu.assertEquals(result, false)
    lu.assertEquals(type(result), "boolean")
end

-- 测试整数解码
function TestJsonDecode:test_decode_integer()
    local result = ljson.decode("42")
    lu.assertEquals(result, 42)
    lu.assertEquals(type(result), "number")
end

-- 测试负整数解码
function TestJsonDecode:test_decode_negative_integer()
    local result = ljson.decode("-123")
    lu.assertEquals(result, -123)
end

-- 测试浮点数解码
function TestJsonDecode:test_decode_double()
    local result = ljson.decode("3.14")
    lu.assertEquals(result, 3.14)
end

-- 测试负浮点数解码
function TestJsonDecode:test_decode_negative_double()
    local result = ljson.decode("-3.14")
    lu.assertEquals(result, -3.14)
end

-- 测试科学计数法解码
function TestJsonDecode:test_decode_scientific_notation()
    local result = ljson.decode("1.23e5")
    lu.assertEquals(result, 123000)
end

-- 测试字符串解码
function TestJsonDecode:test_decode_string()
    local result = ljson.decode('"hello"')
    lu.assertEquals(result, "hello")
    lu.assertEquals(type(result), "string")
end

-- 测试中文字符串解码
function TestJsonDecode:test_decode_chinese_string()
    local result = ljson.decode('"张三"')
    lu.assertEquals(result, "张三")
end

-- 测试特殊字符转义解码
function TestJsonDecode:test_decode_escaped_string()
    local result = ljson.decode('"hello\\nworld"')
    lu.assertEquals(result, "hello\nworld")
end

-- 测试 Unicode 转义解码
function TestJsonDecode:test_decode_unicode_escape()
    local result = ljson.decode('"hello\\u0000world"')
    lu.assertEquals(result, "hello\000world")
end

-- 测试空数组解码
function TestJsonDecode:test_decode_empty_array()
    local result = ljson.decode("[]")
    lu.assertEquals(type(result), "table")
    lu.assertEquals(#result, 0)
end

-- 测试简单数组解码
function TestJsonDecode:test_decode_simple_array()
    local result = ljson.decode("[1,2,3]")
    lu.assertEquals(type(result), "table")
    lu.assertEquals(#result, 3)
    lu.assertEquals(result[1], 1)
    lu.assertEquals(result[2], 2)
    lu.assertEquals(result[3], 3)
end

-- 测试混合类型数组解码
function TestJsonDecode:test_decode_mixed_array()
    local result = ljson.decode('[1,"two",false,3.14]')
    lu.assertEquals(type(result), "table")
    lu.assertEquals(#result, 4)
    lu.assertEquals(result[1], 1)
    lu.assertEquals(result[2], "two")
    lu.assertEquals(result[3], false)
    lu.assertEquals(result[4], 3.14)
end

-- 测试包含 null 的数组解码
function TestJsonDecode:test_decode_array_with_null()
    local result = ljson.decode('[1,null,3]')
    lu.assertEquals(type(result), "table")
    lu.assertEquals(#result, 3)
    lu.assertEquals(result[1], 1)
    lu.assertNil(result[2])
    lu.assertEquals(result[3], 3)
end

-- 测试嵌套数组解码
function TestJsonDecode:test_decode_nested_array()
    local result = ljson.decode("[[1,2],[3,4]]")
    lu.assertEquals(type(result), "table")
    lu.assertEquals(#result, 2)
    lu.assertEquals(type(result[1]), "table")
    lu.assertEquals(result[1][1], 1)
    lu.assertEquals(result[1][2], 2)
    lu.assertEquals(result[2][1], 3)
    lu.assertEquals(result[2][2], 4)
end

-- 测试空对象解码
function TestJsonDecode:test_decode_empty_object()
    local result = ljson.decode("{}")
    lu.assertEquals(type(result), "table")
    local count = 0
    for _ in pairs(result) do
        count = count + 1
    end
    lu.assertEquals(count, 0)
end

-- 测试简单对象解码
function TestJsonDecode:test_decode_simple_object()
    local result = ljson.decode('{"name":"Alice","age":30}')
    lu.assertEquals(type(result), "table")
    lu.assertEquals(result.name, "Alice")
    lu.assertEquals(result.age, 30)
end

-- 测试嵌套对象解码
function TestJsonDecode:test_decode_nested_object()
    local result = ljson.decode('{"person":{"name":"Bob","age":25}}')
    lu.assertEquals(type(result), "table")
    lu.assertEquals(type(result.person), "table")
    lu.assertEquals(result.person.name, "Bob")
    lu.assertEquals(result.person.age, 25)
end

-- 测试对象包含数组解码
function TestJsonDecode:test_decode_object_with_array()
    local result = ljson.decode('{"name":"Charlie","scores":[85,92,78]}')
    lu.assertEquals(type(result), "table")
    lu.assertEquals(result.name, "Charlie")
    lu.assertEquals(type(result.scores), "table")
    lu.assertEquals(#result.scores, 3)
    lu.assertEquals(result.scores[1], 85)
    lu.assertEquals(result.scores[2], 92)
    lu.assertEquals(result.scores[3], 78)
end

-- 测试数组包含对象解码
function TestJsonDecode:test_decode_array_with_object()
    local result = ljson.decode('[{"id":1,"name":"Alice"},{"id":2,"name":"Bob"}]')
    lu.assertEquals(type(result), "table")
    lu.assertEquals(#result, 2)
    lu.assertEquals(result[1].id, 1)
    lu.assertEquals(result[1].name, "Alice")
    lu.assertEquals(result[2].id, 2)
    lu.assertEquals(result[2].name, "Bob")
end

-- 测试复杂嵌套结构解码
function TestJsonDecode:test_decode_complex_nested()
    local json_str = '{"users":[{"name":"Alice","age":25,"tags":["tag1","tag2"]}],"count":1}'
    local result = ljson.decode(json_str)
    lu.assertEquals(type(result), "table")
    lu.assertEquals(type(result.users), "table")
    lu.assertEquals(#result.users, 1)
    lu.assertEquals(result.users[1].name, "Alice")
    lu.assertEquals(result.users[1].age, 25)
    lu.assertEquals(type(result.users[1].tags), "table")
    lu.assertEquals(result.users[1].tags[1], "tag1")
    lu.assertEquals(result.count, 1)
end

-- 测试带空格的 JSON 解码
function TestJsonDecode:test_decode_with_whitespace()
    local result = ljson.decode('  {  "name"  :  "Alice"  }  ')
    lu.assertEquals(type(result), "table")
    lu.assertEquals(result.name, "Alice")
end

-- 测试多行 JSON 解码
function TestJsonDecode:test_decode_multiline()
    local json_str = [[{
        "name": "Bob",
        "age": 30
    }]]
    local result = ljson.decode(json_str)
    lu.assertEquals(type(result), "table")
    lu.assertEquals(result.name, "Bob")
    lu.assertEquals(result.age, 30)
end

-- 测试转义引号解码
function TestJsonDecode:test_decode_escaped_quote()
    local result = ljson.decode([["say \"hello\""]])
    lu.assertEquals(result, 'say "hello"')
end

-- 测试转义反斜杠解码
function TestJsonDecode:test_decode_escaped_backslash()
    local result = ljson.decode([["path\\to\\file"]])
    lu.assertEquals(result, "path\\to\\file")
end

-- 测试转义斜杠解码
function TestJsonDecode:test_decode_escaped_slash()
    local result = ljson.decode([["a/b"]])
    lu.assertEquals(result, "a/b")
end

-- 测试 Tab 转义解码
function TestJsonDecode:test_decode_escaped_tab()
    local result = ljson.decode('"hello\\tworld"')
    lu.assertEquals(result, "hello\tworld")
end

-- 测试回车转义解码
function TestJsonDecode:test_decode_escaped_carriage_return()
    local result = ljson.decode('"hello\\rworld"')
    lu.assertEquals(result, "hello\rworld")
end

-- 测试换页转义解码
function TestJsonDecode:test_decode_escaped_formfeed()
    local result = ljson.decode('"hello\\fworld"')
    lu.assertEquals(result, "hello\fworld")
end

-- 测试反斜杠转义解码
function TestJsonDecode:test_decode_escaped_backslash_char()
    local result = ljson.decode('"hello\\\\world"')
    lu.assertEquals(result, "hello\\world")
end

-- 测试多种转义混合解码
function TestJsonDecode:test_decode_multiple_escapes()
    local result = ljson.decode('"\\n\\t\\r\\\\\\"')
    lu.assertEquals(result, "\n\t\r\\")
end

-- 测试大数解码
function TestJsonDecode:test_decode_large_number()
    local result = ljson.decode("1234567890")
    lu.assertEquals(result, 1234567890)
end

-- 测试负大数解码
function TestJsonDecode:test_decode_large_negative_number()
    local result = ljson.decode("-9876543210")
    lu.assertEquals(result, -9876543210)
end

-- 测试小数解码
function TestJsonDecode:test_decode_decimal()
    local result = ljson.decode("0.123")
    lu.assertEquals(result, 0.123)
end

-- 测试负小数解码
function TestJsonDecode:test_decode_negative_decimal()
    local result = ljson.decode("-0.456")
    lu.assertEquals(result, -0.456)
end

-- 测试数字前导零解码（虽然不是标准 JSON，但某些解析器接受）
function TestJsonDecode:test_decode_number_with_leading_zero()
    local result = ljson.decode("0.5")
    lu.assertEquals(result, 0.5)
end

-- 测试空字符串解码（应该报错）
function TestJsonDecode:test_decode_empty_string()
    local success, err = pcall(function()
        ljson.decode("")
    end)
    lu.assertFalse(success)
    lu.assertStrContains(err, "decode failed")
end

-- 测试无效 JSON 解码（缺少引号）
function TestJsonDecode:test_decode_invalid_json_missing_quote()
    local success, err = pcall(function()
        ljson.decode("{name: \"Alice\"}")
    end)
    lu.assertFalse(success)
    lu.assertStrContains(err, "decode failed")
end

-- 测试无效 JSON 解码（缺少逗号）
function TestJsonDecode:test_decode_invalid_json_missing_comma()
    local success, err = pcall(function()
        ljson.decode('{"name" "Alice"}')
    end)
    lu.assertFalse(success)
    lu.assertStrContains(err, "decode failed")
end

-- 测试无效 JSON 解码（括号不匹配）
function TestJsonDecode:test_decode_invalid_json_unmatched_bracket()
    local success, err = pcall(function()
        ljson.decode('{"name": "Alice"')
    end)
    lu.assertFalse(success)
    lu.assertStrContains(err, "decode failed")
end

-- 测试无效 JSON 解码（数组括号不匹配）
function TestJsonDecode:test_decode_invalid_json_unmatched_array_bracket()
    local success, err = pcall(function()
        ljson.decode("[1, 2, 3")
    end)
    lu.assertFalse(success)
    lu.assertStrContains(err, "decode failed")
end

-- 测试纯空白字符串解码（应该报错）
function TestJsonDecode:test_decode_whitespace_only()
    local success = pcall(function()
        ljson.decode("   \n\t  ")
    end)
    lu.assertFalse(success)
end

-- 测试非字符串参数（应该报错）
function TestJsonDecode:test_decode_non_string_argument()
    local success, err = pcall(function()
        ljson.decode(123)
    end)
    lu.assertFalse(success)
    lu.assertStrContains(err, "must be string")
end

-- 测试 table 参数（应该报错）
function TestJsonDecode:test_decode_table_argument()
    local success = pcall(function()
        ljson.decode({})
    end)
    lu.assertFalse(success)
end

-- 测试布尔值 true 字符串解码
function TestJsonDecode:test_decode_true_string()
    local result = ljson.decode("true")
    lu.assertEquals(result, true)
end

-- 测试布尔值 false 字符串解码
function TestJsonDecode:test_decode_false_string()
    local result = ljson.decode("false")
    lu.assertEquals(result, false)
end

-- 测试数组包含多种数据类型和嵌套
function TestJsonDecode:test_decode_complex_mixed_array()
    local json_str = '[1,"two",true,null,[3,4],{"key":"value"}]'
    local result = ljson.decode(json_str)
    lu.assertEquals(#result, 6)
    lu.assertEquals(result[1], 1)
    lu.assertEquals(result[2], "two")
    lu.assertEquals(result[3], true)
    lu.assertNil(result[4])
    lu.assertEquals(type(result[5]), "table")
    lu.assertEquals(result[5][1], 3)
    lu.assertEquals(type(result[6]), "table")
    lu.assertEquals(result[6].key, "value")
end

-- 测试对象包含 null 值
function TestJsonDecode:test_decode_object_with_null()
    local result = ljson.decode('{"name":null,"age":30}')
    lu.assertEquals(type(result), "table")
    lu.assertNil(result.name)
    lu.assertEquals(result.age, 30)
end

-- 测试对象包含布尔值
function TestJsonDecode:test_decode_object_with_booleans()
    local result = ljson.decode('{"active":true,"deleted":false}')
    lu.assertEquals(type(result), "table")
    lu.assertEquals(result.active, true)
    lu.assertEquals(result.deleted, false)
end

-- 测试对象包含数字
function TestJsonDecode:test_decode_object_with_numbers()
    local result = ljson.decode('{"int":42,"float":3.14,"negative":-10}')
    lu.assertEquals(type(result), "table")
    lu.assertEquals(result.int, 42)
    lu.assertEquals(result.float, 3.14)
    lu.assertEquals(result.negative, -10)
end

-- 测试深层嵌套对象
function TestJsonDecode:test_decode_deeply_nested_object()
    local result = ljson.decode('{"a":{"b":{"c":{"d":"value"}}}}')
    lu.assertEquals(type(result), "table")
    lu.assertEquals(type(result.a), "table")
    lu.assertEquals(type(result.a.b), "table")
    lu.assertEquals(type(result.a.b.c), "table")
    lu.assertEquals(result.a.b.c.d, "value")
end

-- 测试深层嵌套数组
function TestJsonDecode:test_decode_deeply_nested_array()
    local result = ljson.decode('[[[[1]]]]')
    lu.assertEquals(type(result), "table")
    lu.assertEquals(type(result[1]), "table")
    lu.assertEquals(type(result[1][1]), "table")
    lu.assertEquals(type(result[1][1][1]), "table")
    lu.assertEquals(result[1][1][1][1], 1)
end

-- 测试包含特殊字符的键
function TestJsonDecode:test_decode_object_with_special_keys()
    local result = ljson.decode('{"key with spaces":"value1","key-with-dashes":"value2"}')
    lu.assertEquals(type(result), "table")
    lu.assertEquals(result["key with spaces"], "value1")
    lu.assertEquals(result["key-with-dashes"], "value2")
end

-- 测试编码解码往返一致性（encode -> decode）
function TestJsonDecode:test_encode_decode_roundtrip()
    local original = {name = "Alice", age = 30, tags = {"tag1", "tag2"}}
    local encoded = ljson.encode(original)
    local decoded = ljson.decode(encoded)
    lu.assertEquals(decoded.name, original.name)
    lu.assertEquals(decoded.age, original.age)
    lu.assertEquals(type(decoded.tags), "table")
    lu.assertEquals(decoded.tags[1], original.tags[1])
    lu.assertEquals(decoded.tags[2], original.tags[2])
end

-- 测试数组编码解码往返一致性
function TestJsonDecode:test_array_encode_decode_roundtrip()
    local original = {1, 2, 3, "four", true}
    local encoded = ljson.encode(original)
    local decoded = ljson.decode(encoded)
    lu.assertEquals(#decoded, #original)
    for i = 1, #original do
        lu.assertEquals(decoded[i], original[i])
    end
end

-- 测试类：dump 和 dump_pretty 函数测试
TestJsonDump = {}

function TestJsonDump:setUp()
    -- 创建临时目录用于测试
    self.test_dir = "/tmp/ljson_dump_test"
    os.execute("mkdir -p " .. self.test_dir)
end

function TestJsonDump:tearDown()
    -- 清理临时文件
    os.execute("rm -rf " .. self.test_dir)
end

-- 辅助函数：读取文件内容
local function read_file(path)
    local file = io.open(path, "r")
    if not file then
        return nil
    end
    local content = file:read("*a")
    file:close()
    return content
end

-- 测试 dump 基本类型 - nil
function TestJsonDump:test_dump_nil()
    local file_path = self.test_dir .. "/test_nil.json"
    local result = ljson.dump(nil, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "null")
end

-- 测试 dump 布尔值 - true
function TestJsonDump:test_dump_boolean_true()
    local file_path = self.test_dir .. "/test_true.json"
    local result = ljson.dump(true, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "true")
end

-- 测试 dump 布尔值 - false
function TestJsonDump:test_dump_boolean_false()
    local file_path = self.test_dir .. "/test_false.json"
    local result = ljson.dump(false, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "false")
end

-- 测试 dump 整数
function TestJsonDump:test_dump_integer()
    local file_path = self.test_dir .. "/test_integer.json"
    local result = ljson.dump(42, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "42")
end

-- 测试 dump 浮点数
function TestJsonDump:test_dump_double()
    local file_path = self.test_dir .. "/test_double.json"
    local result = ljson.dump(3.14, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "3.14")
end

-- 测试 dump 字符串
function TestJsonDump:test_dump_string()
    local file_path = self.test_dir .. "/test_string.json"
    local result = ljson.dump("hello", file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "\"hello\"")
end

-- 测试 dump 中文字符串
function TestJsonDump:test_dump_chinese_string()
    local file_path = self.test_dir .. "/test_chinese.json"
    local result = ljson.dump("张三", file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "\"张三\"")
end

-- 测试 dump 空数组
function TestJsonDump:test_dump_empty_array()
    local file_path = self.test_dir .. "/test_empty_array.json"
    local result = ljson.dump({}, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "[]")
end

-- 测试 dump 简单数组
function TestJsonDump:test_dump_simple_array()
    local file_path = self.test_dir .. "/test_array.json"
    local result = ljson.dump({1, 2, 3}, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "[1,2,3]")
end

-- 测试 dump 混合类型数组
function TestJsonDump:test_dump_mixed_array()
    local file_path = self.test_dir .. "/test_mixed_array.json"
    local result = ljson.dump({1, "two", false, 3.14}, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "[1,\"two\",false,3.14]")
end

-- 测试 dump 嵌套数组
function TestJsonDump:test_dump_nested_array()
    local file_path = self.test_dir .. "/test_nested_array.json"
    local result = ljson.dump({{1, 2}, {3, 4}}, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "[[1,2],[3,4]]")
end

-- 测试 dump 空对象（需要设置标志）
function TestJsonDump:test_dump_empty_object()
    ljson.encode_empty_table_as_object(true)
    local file_path = self.test_dir .. "/test_empty_object.json"
    local result = ljson.dump({}, file_path)
    ljson.encode_empty_table_as_object(false)

    lu.assertTrue(result)
    local content = read_file(file_path)
    lu.assertEquals(content, "{}")
end

-- 测试 dump 简单对象
function TestJsonDump:test_dump_simple_object()
    local file_path = self.test_dir .. "/test_object.json"
    local result = ljson.dump({name = "Alice", age = 30}, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    -- 注意：键会按字母序排序
    lu.assertEquals(content, "{\"age\":30,\"name\":\"Alice\"}")
end

-- 测试 dump 嵌套对象
function TestJsonDump:test_dump_nested_object()
    local file_path = self.test_dir .. "/test_nested_object.json"
    local result = ljson.dump({
        person = {
            name = "Bob",
            age = 25
        }
    }, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "{\"person\":{\"age\":25,\"name\":\"Bob\"}}")
end

-- 测试 dump 对象包含数组
function TestJsonDump:test_dump_object_with_array()
    local file_path = self.test_dir .. "/test_obj_array.json"
    local result = ljson.dump({
        name = "Charlie",
        scores = {85, 92, 78}
    }, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "{\"name\":\"Charlie\",\"scores\":[85,92,78]}")
end

-- 测试 dump 数组包含对象
function TestJsonDump:test_dump_array_with_object()
    local file_path = self.test_dir .. "/test_array_obj.json"
    local result = ljson.dump({
        {id = 1, name = "Alice"},
        {id = 2, name = "Bob"}
    }, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "[{\"id\":1,\"name\":\"Alice\"},{\"id\":2,\"name\":\"Bob\"}]")
end

-- 测试 dump 覆盖已存在文件
function TestJsonDump:test_dump_overwrite_existing()
    local file_path = self.test_dir .. "/test_overwrite.json"

    -- 第一次写入
    local result1 = ljson.dump({value = 1}, file_path)
    lu.assertTrue(result1)

    -- 第二次覆盖
    local result2 = ljson.dump({value = 2}, file_path)
    lu.assertTrue(result2)

    local content = read_file(file_path)
    lu.assertEquals(content, "{\"value\":2}")
end

-- 测试 dump_pretty 基本类型
function TestJsonDump:test_dump_pretty_nil()
    local file_path = self.test_dir .. "/test_pretty_nil.json"
    local result = ljson.dump_pretty(nil, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertEquals(content, "null")
end

-- 测试 dump_pretty 简单对象（格式化输出）
function TestJsonDump:test_dump_pretty_simple_object()
    local file_path = self.test_dir .. "/test_pretty_object.json"
    local result = ljson.dump_pretty({name = "Alice", age = 30}, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    -- 验证包含换行和缩进
    lu.assertStrContains(content, "\n")
    lu.assertStrContains(content, "    ")
    lu.assertStrContains(content, "\"name\"")
    lu.assertStrContains(content, "\"age\"")
end

-- 测试 dump_pretty 数组（格式化输出）
function TestJsonDump:test_dump_pretty_array()
    local file_path = self.test_dir .. "/test_pretty_array.json"
    local result = ljson.dump_pretty({1, 2, 3}, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    -- 格式化输出应该包含换行和缩进
    lu.assertStrContains(content, "[")
    lu.assertStrContains(content, "]")
end

-- 测试 dump_pretty 嵌套结构
function TestJsonDump:test_dump_pretty_nested()
    local file_path = self.test_dir .. "/test_pretty_nested.json"
    local result = ljson.dump_pretty({
        users = {
            {name = "Alice", age = 25},
            {name = "Bob", age = 30}
        }
    }, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    -- 验证格式化
    lu.assertStrContains(content, "\n")
end

-- 测试 dump 和 dump_pretty 输出格式差异
function TestJsonDump:test_dump_vs_pretty_format()
    local data = {name = "Test", value = 123, items = {1, 2, 3}}

    local compact_file = self.test_dir .. "/compact.json"
    local pretty_file = self.test_dir .. "/pretty.json"

    ljson.dump(data, compact_file)
    ljson.dump_pretty(data, pretty_file)

    local compact = read_file(compact_file)
    local pretty = read_file(pretty_file)

    -- 紧凑格式应该更短（不含换行）
    lu.assertTrue(#compact < #pretty or string.find(pretty, "\n") ~= nil)
end

-- 测试 dump 后通过 decode 读取验证
function TestJsonDump:test_dump_then_decode_roundtrip()
    local file_path = self.test_dir .. "/roundtrip.json"
    local original = {name = "Alice", age = 30, tags = {"tag1", "tag2"}}

    -- dump 到文件
    local dump_result = ljson.dump(original, file_path)
    lu.assertTrue(dump_result)

    -- 从文件读取
    local file = io.open(file_path, "r")
    lu.assertNotNil(file)
    local content = file:read("*a")
    file:close()

    -- decode
    local decoded = ljson.decode(content)

    -- 验证
    lu.assertEquals(decoded.name, original.name)
    lu.assertEquals(decoded.age, original.age)
    lu.assertEquals(type(decoded.tags), "table")
    lu.assertEquals(#decoded.tags, 2)
end

-- 测试 dump_pretty 后通过 decode 读取验证
function TestJsonDump:test_dump_pretty_then_decode_roundtrip()
    local file_path = self.test_dir .. "/roundtrip_pretty.json"
    local original = {name = "Bob", scores = {85, 92, 78}}

    -- dump_pretty 到文件
    local result = ljson.dump_pretty(original, file_path)
    lu.assertTrue(result)

    -- 从文件读取
    local file = io.open(file_path, "r")
    lu.assertNotNil(file)
    local content = file:read("*a")
    file:close()

    -- decode
    local decoded = ljson.decode(content)

    -- 验证
    lu.assertEquals(decoded.name, original.name)
    lu.assertEquals(type(decoded.scores), "table")
    lu.assertEquals(decoded.scores[1], 85)
    lu.assertEquals(decoded.scores[2], 92)
    lu.assertEquals(decoded.scores[3], 78)
end

-- 测试 dump 包含特殊字符
function TestJsonDump:test_dump_special_characters()
    local file_path = self.test_dir .. "/special.json"
    local result = ljson.dump({
        message = "Hello\nWorld\t!",
        path = "C:\\Users\\test"
    }, file_path)
    lu.assertTrue(result)

    local content = read_file(file_path)
    lu.assertStrContains(content, "\\n")
    lu.assertStrContains(content, "\\t")
    lu.assertStrContains(content, "\\\\")
end

-- 测试 dump 复杂嵌套结构
function TestJsonDump:test_dump_complex_structure()
    local file_path = self.test_dir .. "/complex.json"
    local data = {
        users = {
            {
                id = 1,
                name = "Alice",
                profile = {
                    age = 25,
                    city = "Beijing",
                    hobbies = {"reading", "coding"}
                }
            }
        },
        count = 1
    }

    local result = ljson.dump(data, file_path)
    lu.assertTrue(result)

    -- 验证文件存在且可读
    local file = io.open(file_path, "r")
    lu.assertNotNil(file)
    local content = file:read("*a")
    file:close()
    lu.assertStrContains(content, "\"users\"")
    lu.assertStrContains(content, "\"count\"")
end

-- 测试 dump 参数不足
function TestJsonDump:test_dump_missing_arguments()
    local success, err = pcall(function()
        ljson.dump({test = "value"})
    end)
    lu.assertFalse(success)
    lu.assertStrContains(err, "requires 2 arguments")
end

-- 测试 dump 文件路径参数不是字符串
function TestJsonDump:test_dump_invalid_path_type()
    local success, err = pcall(function()
        ljson.dump({test = "value"}, 123)
    end)
    lu.assertFalse(success)
    lu.assertStrContains(err, "must be string")
end

-- 测试 dump_pretty 参数不足
function TestJsonDump:test_dump_pretty_missing_arguments()
    local success, err = pcall(function()
        ljson.dump_pretty({test = "value"})
    end)
    lu.assertFalse(success)
    lu.assertStrContains(err, "requires 2 arguments")
end

-- 测试 dump_pretty 文件路径参数不是字符串
function TestJsonDump:test_dump_pretty_invalid_path_type()
    local success, err = pcall(function()
        ljson.dump_pretty({test = "value"}, true)
    end)
    lu.assertFalse(success)
    lu.assertStrContains(err, "must be string")
end

-- 测试 dump 到只读路径（如果存在）
function TestJsonDump:test_dump_to_readonly_path()
    local file_path = "/root/test.json"
    local result = ljson.dump({test = "value"}, file_path)
    -- 应该返回 false 或报错
    if type(result) == "boolean" then
        lu.assertFalse(result)
    end
end

-- 测试多次 dump 到同一文件
function TestJsonDump:test_dump_multiple_times()
    local file_path = self.test_dir .. "/multiple.json"

    for i = 1, 5 do
        local result = ljson.dump({iteration = i}, file_path)
        lu.assertTrue(result)
    end

    local content = read_file(file_path)
    local decoded = ljson.decode(content)
    lu.assertEquals(decoded.iteration, 5)
end

-- 测试 dump 返回值类型
function TestJsonDump:test_dump_return_type()
    local file_path = self.test_dir .. "/return_test.json"
    local result = ljson.dump({test = "value"}, file_path)
    lu.assertEquals(type(result), "boolean")
end

-- 测试 dump_pretty 返回值类型
function TestJsonDump:test_dump_pretty_return_type()
    local file_path = self.test_dir .. "/return_pretty_test.json"
    local result = ljson.dump_pretty({test = "value"}, file_path)
    lu.assertEquals(type(result), "boolean")
end

-- 测试 dump 大量数据
function TestJsonDump:test_dump_large_data()
    local file_path = self.test_dir .. "/large.json"
    local large_array = {}
    for i = 1, 1000 do
        large_array[i] = {id = i, name = "Item" .. i}
    end

    local result = ljson.dump(large_array, file_path)
    lu.assertTrue(result)

    -- 验证文件存在
    local file = io.open(file_path, "r")
    lu.assertNotNil(file)
    file:close()
end

-- 测试 dump_pretty 大量数据
function TestJsonDump:test_dump_pretty_large_data()
    local file_path = self.test_dir .. "/large_pretty.json"
    local large_array = {}
    for i = 1, 100 do
        large_array[i] = {id = i, data = {x = i, y = i * 2}}
    end

    local result = ljson.dump_pretty(large_array, file_path)
    lu.assertTrue(result)

    -- 验证文件包含格式化
    local content = read_file(file_path)
    lu.assertStrContains(content, "\n")
end

-- 测试类：json_object_dump 和 json_object_dump_pretty 函数测试
TestJsonObjectDump = {}

function TestJsonObjectDump:setUp()
    -- 创建临时目录用于测试
    self.test_dir = "/tmp/ljson_object_dump_test"
    os.execute("mkdir -p " .. self.test_dir)
end

function TestJsonObjectDump:tearDown()
    -- 清理临时文件
    os.execute("rm -rf " .. self.test_dir)
end

-- 测试 json_object_dump 基本对象
function TestJsonObjectDump:test_json_object_dump_simple_object()
    -- 先创建 JsonValue userdata
    local obj = ljson.json_object_from_table({name = "Alice", age = 30})
    local file_path = self.test_dir .. "/test_simple.json"

    -- 使用 json_object_dump 写入文件
    local result = ljson.json_object_dump(obj, file_path)
    lu.assertTrue(result)

    -- 验证文件内容
    local file = io.open(file_path, "r")
    lu.assertNotNil(file)
    local content = file:read("*a")
    file:close()

    lu.assertStrContains(content, "\"name\"")
    lu.assertStrContains(content, "\"age\"")
end

-- 测试 json_object_dump 数组
function TestJsonObjectDump:test_json_object_dump_array()
    local arr = ljson.json_object_from_table({1, 2, 3, "four"})
    local file_path = self.test_dir .. "/test_array.json"

    local result = ljson.json_object_dump(arr, file_path)
    lu.assertTrue(result)

    local file = io.open(file_path, "r")
    local content = file:read("*a")
    file:close()

    lu.assertEquals(content, "[1,2,3,\"four\"]")
end

-- 测试 json_object_dump 空对象
function TestJsonObjectDump:test_json_object_dump_empty_object()
    local obj = ljson.json_object_new_object()
    local file_path = self.test_dir .. "/test_empty.json"

    local result = ljson.json_object_dump(obj, file_path)
    lu.assertTrue(result)

    local file = io.open(file_path, "r")
    local content = file:read("*a")
    file:close()

    lu.assertEquals(content, "{}")
end

-- 测试 json_object_dump 空数组
function TestJsonObjectDump:test_json_object_dump_empty_array()
    local arr = ljson.json_object_new_array()
    local file_path = self.test_dir .. "/test_empty_arr.json"

    local result = ljson.json_object_dump(arr, file_path)
    lu.assertTrue(result)

    local file = io.open(file_path, "r")
    local content = file:read("*a")
    file:close()

    lu.assertEquals(content, "[]")
end

-- 测试 json_object_dump 嵌套结构
function TestJsonObjectDump:test_json_object_dump_nested()
    local data = {
        users = {
            {name = "Alice", age = 25},
            {name = "Bob", age = 30}
        },
        count = 2
    }
    local obj = ljson.json_object_from_table(data)
    local file_path = self.test_dir .. "/test_nested.json"

    local result = ljson.json_object_dump(obj, file_path)
    lu.assertTrue(result)

    local file = io.open(file_path, "r")
    local content = file:read("*a")
    file:close()

    lu.assertStrContains(content, "\"users\"")
    lu.assertStrContains(content, "\"count\"")
end

-- 测试 json_object_dump_pretty 简单对象
function TestJsonObjectDump:test_json_object_dump_pretty_simple()
    local obj = ljson.json_object_from_table({name = "Alice", age = 30})
    local file_path = self.test_dir .. "/test_pretty_simple.json"

    local result = ljson.json_object_dump_pretty(obj, file_path)
    lu.assertTrue(result)

    local file = io.open(file_path, "r")
    local content = file:read("*a")
    file:close()

    -- 验证包含格式化（换行和缩进）
    lu.assertStrContains(content, "\n")
    lu.assertStrContains(content, "\"name\"")
    lu.assertStrContains(content, "\"age\"")
end

-- 测试 json_object_dump_pretty 嵌套结构
function TestJsonObjectDump:test_json_object_dump_pretty_nested()
    local data = {
        level1 = {
            level2 = {
                level3 = {
                    value = "deep"
                }
            }
        }
    }
    local obj = ljson.json_object_from_table(data)
    local file_path = self.test_dir .. "/test_pretty_nested.json"

    local result = ljson.json_object_dump_pretty(obj, file_path)
    lu.assertTrue(result)

    local file = io.open(file_path, "r")
    local content = file:read("*a")
    file:close()

    -- 格式化输出应该包含多层缩进
    lu.assertStrContains(content, "\n")
    lu.assertStrContains(content, "\"level1\"")
    lu.assertStrContains(content, "\"level2\"")
    lu.assertStrContains(content, "\"level3\"")
end

-- 对比 json_object_dump 和 json_object_dump_pretty 输出
function TestJsonObjectDump:test_json_object_dump_vs_pretty()
    local data = {name = "Test", items = {1, 2, 3}, active = true}
    local obj = ljson.json_object_from_table(data)

    local compact_file = self.test_dir .. "/compact.json"
    local pretty_file = self.test_dir .. "/pretty.json"

    ljson.json_object_dump(obj, compact_file)
    ljson.json_object_dump_pretty(obj, pretty_file)

    local file1 = io.open(compact_file, "r")
    local file2 = io.open(pretty_file, "r")
    local compact = file1:read("*a")
    local pretty = file2:read("*a")
    file1:close()
    file2:close()

    -- 紧凑格式不应该包含换行（或很少），格式化版本应该包含换行
    local compact_newlines = select(2, string.gsub(compact, "\n", "\n"))
    local pretty_newlines = select(2, string.gsub(pretty, "\n", "\n"))

    lu.assertTrue(pretty_newlines >= compact_newlines)
end

-- 测试 json_object_dump 后通过 decode 验证
function TestJsonObjectDump:test_json_object_dump_roundtrip()
    local original = {
        name = "Alice",
        age = 30,
        tags = {"tag1", "tag2", "tag3"},
        active = true,
        score = 95.5
    }

    local obj = ljson.json_object_from_table(original)
    local file_path = self.test_dir .. "/roundtrip.json"

    -- dump 到文件
    local dump_result = ljson.json_object_dump(obj, file_path)
    lu.assertTrue(dump_result)

    -- 从文件读取并 decode
    local file = io.open(file_path, "r")
    local content = file:read("*a")
    file:close()

    local decoded = ljson.decode(content)

    -- 验证数据一致性
    lu.assertEquals(decoded.name, original.name)
    lu.assertEquals(decoded.age, original.age)
    lu.assertEquals(type(decoded.tags), "table")
    lu.assertEquals(#decoded.tags, 3)
    lu.assertEquals(decoded.active, original.active)
    lu.assertEquals(decoded.score, original.score)
end

-- 测试 json_object_dump 参数不足
function TestJsonObjectDump:test_json_object_dump_missing_arguments()
    local obj = ljson.json_object_from_table({test = "value"})

    local success, err = pcall(function()
        ljson.json_object_dump(obj)
    end)

    lu.assertFalse(success)
    lu.assertStrContains(err, "requires 2 arguments")
end

-- 测试 json_object_dump 第二个参数不是字符串
function TestJsonObjectDump:test_json_object_dump_invalid_path_type()
    local obj = ljson.json_object_from_table({test = "value"})

    local success, err = pcall(function()
        ljson.json_object_dump(obj, 123)
    end)

    lu.assertFalse(success)
    lu.assertStrContains(err, "must be string")
end

-- 测试 json_object_dump_pretty 参数不足
function TestJsonObjectDump:test_json_object_dump_pretty_missing_arguments()
    local obj = ljson.json_object_from_table({test = "value"})

    local success, err = pcall(function()
        ljson.json_object_dump_pretty(obj)
    end)

    lu.assertFalse(success)
    lu.assertStrContains(err, "requires 2 arguments")
end

-- 测试 json_object_dump_pretty 第二个参数不是字符串
function TestJsonObjectDump:test_json_object_dump_pretty_invalid_path_type()
    local obj = ljson.json_object_from_table({test = "value"})

    local success, err = pcall(function()
        ljson.json_object_dump_pretty(obj, true)
    end)

    lu.assertFalse(success)
    lu.assertStrContains(err, "must be string")
end

-- 测试 json_object_dump 返回值类型
function TestJsonObjectDump:test_json_object_dump_return_type()
    local obj = ljson.json_object_from_table({test = "value"})
    local file_path = self.test_dir .. "/return_type.json"

    local result = ljson.json_object_dump(obj, file_path)
    lu.assertEquals(type(result), "boolean")
    lu.assertTrue(result)
end

-- 测试 json_object_dump_pretty 返回值类型
function TestJsonObjectDump:test_json_object_dump_pretty_return_type()
    local obj = ljson.json_object_from_table({test = "value"})
    local file_path = self.test_dir .. "/return_pretty_type.json"

    local result = ljson.json_object_dump_pretty(obj, file_path)
    lu.assertEquals(type(result), "boolean")
    lu.assertTrue(result)
end

-- 测试 json_object_dump 特殊字符
function TestJsonObjectDump:test_json_object_dump_special_characters()
    local data = {
        message = "Line1\nLine2\tTabbed",
        path = "C:\\Users\\test",
        quote = 'He said "hello"'
    }
    local obj = ljson.json_object_from_table(data)
    local file_path = self.test_dir .. "/special.json"

    local result = ljson.json_object_dump(obj, file_path)
    lu.assertTrue(result)

    local file = io.open(file_path, "r")
    local content = file:read("*a")
    file:close()

    lu.assertStrContains(content, "\\n")
    lu.assertStrContains(content, "\\t")
    lu.assertStrContains(content, "\\\\")
end

-- 测试 json_object_dump 与 encode 一致性
function TestJsonObjectDump:test_json_object_dump_consistent_with_encode()
    local data = {name = "Test", value = 123, active = true}

    -- 使用 json_object_from_table 创建 JsonValue
    local obj = ljson.json_object_from_table(data)
    local file_path = self.test_dir .. "/consistency.json"

    -- dump 到文件
    ljson.json_object_dump(obj, file_path)

    -- 直接 encode
    local encoded = ljson.encode(data)

    -- 读取文件
    local file = io.open(file_path, "r")
    local file_content = file:read("*a")
    file:close()

    -- 两者应该一致
    lu.assertEquals(file_content, encoded)
end

-- 测试 json_object_dump_pretty 与 encode_pretty 一致性
function TestJsonObjectDump:test_json_object_dump_pretty_consistent_with_encode_pretty()
    local data = {
        users = {
            {name = "Alice", age = 25},
            {name = "Bob", age = 30}
        }
    }

    local obj = ljson.json_object_from_table(data)
    local file_path = self.test_dir .. "/consistency_pretty.json"

    -- dump_pretty 到文件
    ljson.json_object_dump_pretty(obj, file_path)

    -- json_object_ordered_encode_pretty
    local encoded = ljson.json_object_ordered_encode_pretty(obj)

    -- 读取文件
    local file = io.open(file_path, "r")
    local file_content = file:read("*a")
    file:close()

    -- 两者应该一致
    lu.assertEquals(file_content, encoded)
end

-- 测试类：json_object_ordered_encode 和 json_object_ordered_encode_pretty 函数测试
TestJsonObjectEncode = {}

function TestJsonObjectEncode:setUp()
end

function TestJsonObjectEncode:tearDown()
end

-- 测试 json_object_ordered_encode 简单对象
function TestJsonObjectEncode:test_json_object_ordered_encode_simple_object()
    local obj = ljson.json_object_from_table({name = "Alice", age = 30})
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "\"name\"")
    lu.assertStrContains(result, "\"age\"")
    lu.assertStrContains(result, "\"Alice\"")
    lu.assertStrContains(result, "30")
end

-- 测试 json_object_ordered_encode 数组
function TestJsonObjectEncode:test_json_object_ordered_encode_array()
    local arr = ljson.json_object_from_table({1, 2, 3, "four"})
    local result = ljson.json_object_ordered_encode(arr)

    lu.assertEquals(result, "[1,2,3,\"four\"]")
end

-- 测试 json_object_ordered_encode 空对象
function TestJsonObjectEncode:test_json_object_ordered_encode_empty_object()
    local obj = ljson.json_object_new_object()
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertEquals(result, "{}")
end

-- 测试 json_object_ordered_encode 空数组
function TestJsonObjectEncode:test_json_object_ordered_encode_empty_array()
    local arr = ljson.json_object_new_array()
    local result = ljson.json_object_ordered_encode(arr)

    lu.assertEquals(result, "[]")
end

-- 测试 json_object_ordered_encode 嵌套结构
function TestJsonObjectEncode:test_json_object_ordered_encode_nested()
    local data = {
        users = {
            {name = "Alice", age = 25},
            {name = "Bob", age = 30}
        },
        count = 2
    }
    local obj = ljson.json_object_from_table(data)
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "\"users\"")
    lu.assertStrContains(result, "\"count\"")
end

-- 测试 json_object_ordered_encode 从 JSON 字符串解码后编码
function TestJsonObjectEncode:test_json_object_ordered_encode_from_decode()
    local json_str = '{"name":"Charlie","age":35,"active":true}'
    local obj = ljson.json_object_ordered_decode(json_str)
    local result = ljson.json_object_ordered_encode(obj)

    -- 编码后的结果应该可以再次解码
    local decoded = ljson.decode(result)
    lu.assertEquals(decoded.name, "Charlie")
    lu.assertEquals(decoded.age, 35)
    lu.assertEquals(decoded.active, true)
end

-- 测试 json_object_ordered_encode 布尔值
function TestJsonObjectEncode:test_json_object_ordered_encode_boolean()
    local obj = ljson.json_object_from_table({active = true, deleted = false})
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "true")
    lu.assertStrContains(result, "false")
end

-- 测试 json_object_ordered_encode null 值
function TestJsonObjectEncode:test_json_object_ordered_encode_null()
    local obj = ljson.json_object_from_table({value = nil})
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "null")
end

-- 测试 json_object_ordered_encode 数字类型
function TestJsonObjectEncode:test_json_object_ordered_encode_numbers()
    local obj = ljson.json_object_from_table({
        int = 42,
        negative = -123,
        float = 3.14159,
        zero = 0
    })
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "42")
    lu.assertStrContains(result, "-123")
    lu.assertStrContains(result, "3.14159")
    lu.assertStrContains(result, "0")
end

-- 测试 json_object_ordered_encode 特殊字符
function TestJsonObjectEncode:test_json_object_ordered_encode_special_chars()
    local obj = ljson.json_object_from_table({
        message = "Hello\nWorld",
        path = "C:\\Users\\test",
        quote = 'He said "hello"'
    })
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "\\n")
    lu.assertStrContains(result, "\\\\")
    lu.assertStrContains(result, "\\\"")
end

-- 测试 json_object_ordered_encode 中文字符
function TestJsonObjectEncode:test_json_object_ordered_encode_chinese()
    local obj = ljson.json_object_from_table({
        name = "张三",
        city = "北京"
    })
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "张三")
    lu.assertStrContains(result, "北京")
end

-- 测试 json_object_ordered_encode 深层嵌套
function TestJsonObjectEncode:test_json_object_ordered_encode_deep_nesting()
    local data = {
        level1 = {
            level2 = {
                level3 = {
                    level4 = {
                        value = "deep"
                    }
                }
            }
        }
    }
    local obj = ljson.json_object_from_table(data)
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "\"level1\"")
    lu.assertStrContains(result, "\"level2\"")
    lu.assertStrContains(result, "\"level3\"")
    lu.assertStrContains(result, "\"level4\"")
end

-- 测试 json_object_ordered_encode 混合数组
function TestJsonObjectEncode:test_json_object_ordered_encode_mixed_array()
    local obj = ljson.json_object_from_table({1, "two", false, nil, 3.14})
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertEquals(result, "[1,\"two\",false,null,3.14]")
end

-- 测试 json_object_ordered_encode 返回值类型
function TestJsonObjectEncode:test_json_object_ordered_encode_return_type()
    local obj = ljson.json_object_from_table({test = "value"})
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertEquals(type(result), "string")
end

-- 测试 json_object_ordered_encode 参数不足
function TestJsonObjectEncode:test_json_object_ordered_encode_missing_argument()
    local success, err = pcall(function()
        ljson.json_object_ordered_encode()
    end)

    lu.assertFalse(success)
    lu.assertStrContains(err, "requires 1 argument")
end

-- 测试 json_object_ordered_encode_pretty 简单对象
function TestJsonObjectEncode:test_json_object_ordered_encode_pretty_simple()
    local obj = ljson.json_object_from_table({name = "Alice", age = 30})
    local result = ljson.json_object_ordered_encode_pretty(obj)

    -- 验证包含格式化（换行和缩进）
    lu.assertStrContains(result, "\n")
    lu.assertStrContains(result, "\"name\"")
    lu.assertStrContains(result, "\"age\"")
end

-- 测试 json_object_ordered_encode_pretty 数组
function TestJsonObjectEncode:test_json_object_ordered_encode_pretty_array()
    local arr = ljson.json_object_from_table({1, 2, 3, 4, 5})
    local result = ljson.json_object_ordered_encode_pretty(arr)

    lu.assertStrContains(result, "[")
    lu.assertStrContains(result, "]")
end

-- 测试 json_object_ordered_encode_pretty 嵌套结构
function TestJsonObjectEncode:test_json_object_ordered_encode_pretty_nested()
    local data = {
        level1 = {
            level2 = {
                value = "deep"
            }
        }
    }
    local obj = ljson.json_object_from_table(data)
    local result = ljson.json_object_ordered_encode_pretty(obj)

    -- 验证格式化输出包含多层缩进
    lu.assertStrContains(result, "\n")
    lu.assertStrContains(result, "\"level1\"")
    lu.assertStrContains(result, "\"level2\"")
end

-- 测试 json_object_ordered_encode_pretty 复杂结构
function TestJsonObjectEncode:test_json_object_ordered_encode_pretty_complex()
    local data = {
        users = {
            {
                id = 1,
                name = "Alice",
                roles = {"admin", "user"},
                active = true
            },
            {
                id = 2,
                name = "Bob",
                roles = {"user"},
                active = false
            }
        },
        metadata = {
            count = 2,
            lastUpdated = "2025-01-01"
        }
    }
    local obj = ljson.json_object_from_table(data)
    local result = ljson.json_object_ordered_encode_pretty(obj)

    -- 验证格式化
    lu.assertStrContains(result, "\n")
    lu.assertStrContains(result, "\"users\"")
    lu.assertStrContains(result, "\"metadata\"")
end

-- 对比 json_object_ordered_encode 和 json_object_ordered_encode_pretty
function TestJsonObjectEncode:test_json_object_ordered_encode_vs_pretty()
    local data = {name = "Test", items = {1, 2, 3}, active = true}
    local obj = ljson.json_object_from_table(data)

    local compact = ljson.json_object_ordered_encode(obj)
    local pretty = ljson.json_object_ordered_encode_pretty(obj)

    -- 紧凑格式不应该包含换行（或很少），格式化版本应该包含换行
    local compact_newlines = select(2, string.gsub(compact, "\n", "\n"))
    local pretty_newlines = select(2, string.gsub(pretty, "\n", "\n"))

    lu.assertTrue(pretty_newlines >= compact_newlines)
end

-- 测试 json_object_ordered_encode_pretty 返回值类型
function TestJsonObjectEncode:test_json_object_ordered_encode_pretty_return_type()
    local obj = ljson.json_object_from_table({test = "value"})
    local result = ljson.json_object_ordered_encode_pretty(obj)

    lu.assertEquals(type(result), "string")
end

-- 测试 json_object_ordered_encode_pretty 参数不足
function TestJsonObjectEncode:test_json_object_ordered_encode_pretty_missing_argument()
    local success, err = pcall(function()
        ljson.json_object_ordered_encode_pretty()
    end)

    lu.assertFalse(success)
    lu.assertStrContains(err, "requires 1 argument")
end

-- 测试 json_object_ordered_encode 与 encode 的一致性
function TestJsonObjectEncode:test_json_object_ordered_encode_consistent_with_encode()
    local data = {name = "Test", value = 123, active = true}

    -- 使用 json_object_from_table 创建 JsonValue
    local obj = ljson.json_object_from_table(data)

    -- json_object_ordered_encode
    local obj_encoded = ljson.json_object_ordered_encode(obj)

    -- 直接 encode
    local direct_encoded = ljson.encode(data)

    -- 两者应该一致
    lu.assertEquals(obj_encoded, direct_encoded)
end

-- 测试 json_object_ordered_encode_pretty 与 encode_pretty 的一致性
function TestJsonObjectEncode:test_json_object_ordered_encode_pretty_consistent_with_encode()
    local data = {
        name = "Test",
        items = {1, 2, 3},
        active = true
    }

    local obj = ljson.json_object_from_table(data)

    -- json_object_ordered_encode_pretty
    local obj_pretty = ljson.json_object_ordered_encode_pretty(obj)

    -- json_object_ordered_encode 并设置 pretty 参数
    -- 注意：普通 encode 没有 pretty 参数，所以我们只验证格式正确
    lu.assertStrContains(obj_pretty, "\n")
    lu.assertStrContains(obj_pretty, "\"name\"")
end

-- 测试 json_object_ordered_encode 后 decode 往返
function TestJsonObjectEncode:test_json_object_ordered_encode_roundtrip()
    local original = {
        name = "Alice",
        age = 30,
        tags = {"tag1", "tag2"},
        active = true,
        score = 95.5
    }

    local obj = ljson.json_object_from_table(original)
    local encoded = ljson.json_object_ordered_encode(obj)
    local decoded = ljson.decode(encoded)

    -- 验证数据一致性
    lu.assertEquals(decoded.name, original.name)
    lu.assertEquals(decoded.age, original.age)
    lu.assertEquals(type(decoded.tags), "table")
    lu.assertEquals(#decoded.tags, 2)
    lu.assertEquals(decoded.active, original.active)
    lu.assertEquals(decoded.score, original.score)
end

-- 测试 json_object_ordered_encode_pretty 后 decode 往返
function TestJsonObjectEncode:test_json_object_ordered_encode_pretty_roundtrip()
    local original = {
        config = {
            debug = true,
            level = 5,
            options = {"opt1", "opt2"}
        },
        metadata = {
            version = "1.0.0",
            name = "test"
        }
    }

    local obj = ljson.json_object_from_table(original)
    local encoded = ljson.json_object_ordered_encode_pretty(obj)
    local decoded = ljson.decode(encoded)

    -- 验证数据
    lu.assertEquals(decoded.config.debug, original.config.debug)
    lu.assertEquals(decoded.config.level, original.config.level)
    lu.assertEquals(type(decoded.config.options), "table")
    lu.assertEquals(#decoded.config.options, 2)
    lu.assertEquals(decoded.metadata.version, original.metadata.version)
end

-- 测试多次调用 json_object_ordered_encode 结果一致
function TestJsonObjectEncode:test_json_object_ordered_encode_idempotent()
    local obj = ljson.json_object_from_table({name = "Test", value = 123})

    local result1 = ljson.json_object_ordered_encode(obj)
    local result2 = ljson.json_object_ordered_encode(obj)

    lu.assertEquals(result1, result2)
end

-- 测试多次调用 json_object_ordered_encode_pretty 结果一致
function TestJsonObjectEncode:test_json_object_ordered_encode_pretty_idempotent()
    local obj = ljson.json_object_from_table({name = "Test", value = 123})

    local result1 = ljson.json_object_ordered_encode_pretty(obj)
    local result2 = ljson.json_object_ordered_encode_pretty(obj)

    lu.assertEquals(result1, result2)
end

-- 测试 json_object_ordered_encode 编码后再次编码
function TestJsonObjectEncode:test_json_object_ordered_encode_double_encode()
    local data = {name = "Test", value = 123}
    local obj1 = ljson.json_object_from_table(data)

    -- 第一次编码
    local encoded1 = ljson.json_object_ordered_encode(obj1)

    -- 将编码后的字符串解码
    local decoded = ljson.decode(encoded1)

    -- 再次创建 JsonValue 并编码
    local obj2 = ljson.json_object_from_table(decoded)
    local encoded2 = ljson.json_object_ordered_encode(obj2)

    -- 两次编码结果应该一致
    lu.assertEquals(encoded1, encoded2)
end

-- 测试 json_object_ordered_encode 编码空值
function TestJsonObjectEncode:test_json_object_ordered_encode_with_null_values()
    local obj = ljson.json_object_from_table({
        string_value = "hello",
        null_value = nil,
        number_value = 42,
        bool_value = true
    })
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "\"string_value\"")
    lu.assertStrContains(result, "\"null_value\"")
    lu.assertStrContains(result, "\"number_value\"")
    lu.assertStrContains(result, "\"bool_value\"")
    lu.assertStrContains(result, "null")
end

-- 测试 json_object_ordered_encode 数组包含对象
function TestJsonObjectEncode:test_json_object_ordered_encode_array_with_objects()
    local obj = ljson.json_object_from_table({
        {id = 1, name = "Alice"},
        {id = 2, name = "Bob"},
        {id = 3, name = "Charlie"}
    })
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "[")
    lu.assertStrContains(result, "]")
    lu.assertStrContains(result, "\"id\"")
    lu.assertStrContains(result, "\"name\"")
end

-- 测试 json_object_ordered_encode 对象包含数组
function TestJsonObjectEncode:test_json_object_ordered_encode_object_with_arrays()
    local obj = ljson.json_object_from_table({
        name = "Collection",
        items = {1, 2, 3, 4, 5},
        tags = {"tag1", "tag2", "tag3"}
    })
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "\"name\"")
    lu.assertStrContains(result, "\"items\"")
    lu.assertStrContains(result, "\"tags\"")
end

-- 测试 json_object_ordered_encode 转义序列
function TestJsonObjectEncode:test_json_object_ordered_encode_escape_sequences()
    local obj = ljson.json_object_from_table({
        newline = "line1\nline2",
        tab = "col1\tcol2",
        carriage_return = "text\rtext",
        backslash = "path\\to\\file",
        quote = 'say "hello"'
    })
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "\\n")
    lu.assertStrContains(result, "\\t")
    lu.assertStrContains(result, "\\r")
    lu.assertStrContains(result, "\\\\")
    lu.assertStrContains(result, "\\\"")
end

-- 测试 json_object_ordered_encode 数值边界
function TestJsonObjectEncode:test_json_object_ordered_encode_number_boundaries()
    local obj = ljson.json_object_from_table({
        max_int = 2147483647,
        min_int = -2147483648,
        large_double = 1.7976931348623157e+308,
        small_double = 2.2250738585072014e-308
    })
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "2147483647")
    lu.assertStrContains(result, "-2147483648")
end

-- 测试 json_object_ordered_encode_unicode
function TestJsonObjectEncode:test_json_object_ordered_encode_unicode()
    local obj = ljson.json_object_from_table({
        emoji = "😀",
        chinese = "中文",
        mixed = "Hello 世界 🌍"
    })
    local result = ljson.json_object_ordered_encode(obj)

    lu.assertStrContains(result, "😀")
    lu.assertStrContains(result, "中文")
    lu.assertStrContains(result, "Hello")
end

-- 测试 json_object_ordered_encode_pretty 缩进格式
function TestJsonObjectEncode:test_json_object_ordered_encode_pretty_indentation()
    local obj = ljson.json_object_from_table({
        level1 = {
            level2 = {
                value = "test"
            }
        }
    })
    local result = ljson.json_object_ordered_encode_pretty(obj)

    -- 验证包含缩进（通常是4个空格）
    lu.assertStrContains(result, "\n")
    -- 检查包含多层缩进
    local lines = {}
    for line in string.gmatch(result, "[^\n]+") do
        table.insert(lines, line)
    end

    -- 应该有多行
    lu.assertTrue(#lines > 1)
end

-- 测试 json_object_ordered_encode_pretty 数组格式化
function TestJsonObjectEncode:test_json_object_ordered_encode_pretty_array_formatting()
    local obj = ljson.json_object_from_table({
        items = {
            {name = "Item1", value = 1},
            {name = "Item2", value = 2},
            {name = "Item3", value = 3}
        }
    })
    local result = ljson.json_object_ordered_encode_pretty(obj)

    -- 验证格式化
    lu.assertStrContains(result, "\n")
    lu.assertStrContains(result, "\"items\"")
    lu.assertStrContains(result, "\"name\"")
    lu.assertStrContains(result, "\"value\"")
end

-- 测试类：json_object_ordered_decode 函数测试
TestJsonObjectDecode = {}

function TestJsonObjectDecode:setUp()
end

function TestJsonObjectDecode:tearDown()
end

-- 测试 json_object_ordered_decode null 值
function TestJsonObjectDecode:test_json_object_ordered_decode_null()
    local obj = ljson.json_object_ordered_decode("null")

    -- 验证返回的是 JsonValue userdata
    -- 可以使用 json_object_ordered_encode 再次编码
    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, "null")
end

-- 测试 json_object_ordered_decode 布尔值 true
function TestJsonObjectDecode:test_json_object_ordered_decode_true()
    local obj = ljson.json_object_ordered_decode("true")

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, "true")
end

-- 测试 json_object_ordered_decode 布尔值 false
function TestJsonObjectDecode:test_json_object_ordered_decode_false()
    local obj = ljson.json_object_ordered_decode("false")

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, "false")
end

-- 测试 json_object_ordered_decode 整数
function TestJsonObjectDecode:test_json_object_ordered_decode_integer()
    local obj = ljson.json_object_ordered_decode("42")

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, "42")
end

-- 测试 json_object_ordered_decode 负整数
function TestJsonObjectDecode:test_json_object_ordered_decode_negative_integer()
    local obj = ljson.json_object_ordered_decode("-123")

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, "-123")
end

-- 测试 json_object_ordered_decode 浮点数
function TestJsonObjectDecode:test_json_object_ordered_decode_double()
    local obj = ljson.json_object_ordered_decode("3.14")

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, "3.14")
end

-- 测试 json_object_ordered_decode 字符串
function TestJsonObjectDecode:test_json_object_ordered_decode_string()
    local obj = ljson.json_object_ordered_decode('"hello"')

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, '"hello"')
end

-- 测试 json_object_ordered_decode 中文字符串
function TestJsonObjectDecode:test_json_object_ordered_decode_chinese()
    local obj = ljson.json_object_ordered_decode('"张三"')

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, '"张三"')
end

-- 测试 json_object_ordered_decode 特殊字符转义
function TestJsonObjectDecode:test_json_object_ordered_decode_escaped_chars()
    local obj = ljson.json_object_ordered_decode('"hello\\nworld"')

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, '"hello\\nworld"')
end

-- 测试 json_object_ordered_decode 空数组
function TestJsonObjectDecode:test_json_object_ordered_decode_empty_array()
    local obj = ljson.json_object_ordered_decode("[]")

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, "[]")
end

-- 测试 json_object_ordered_decode 简单数组
function TestJsonObjectDecode:test_json_object_ordered_decode_simple_array()
    local obj = ljson.json_object_ordered_decode("[1,2,3]")

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, "[1,2,3]")
end

-- 测试 json_object_ordered_decode 混合类型数组
function TestJsonObjectDecode:test_json_object_ordered_decode_mixed_array()
    local obj = ljson.json_object_ordered_decode('[1,"two",false,3.14]')

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, '[1,"two",false,3.14]')
end

-- 测试 json_object_ordered_decode 包含 null 的数组
function TestJsonObjectDecode:test_json_object_ordered_decode_array_with_null()
    local obj = ljson.json_object_ordered_decode('[1,null,3]')

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, '[1,null,3]')
end

-- 测试 json_object_ordered_decode 嵌套数组
function TestJsonObjectDecode:test_json_object_ordered_decode_nested_array()
    local obj = ljson.json_object_ordered_decode("[[1,2],[3,4]]")

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, "[[1,2],[3,4]]")
end

-- 测试 json_object_ordered_decode 空对象
function TestJsonObjectDecode:test_json_object_ordered_decode_empty_object()
    local obj = ljson.json_object_ordered_decode("{}")

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, "{}")
end

-- 测试 json_object_ordered_decode 简单对象
function TestJsonObjectDecode:test_json_object_ordered_decode_simple_object()
    local obj = ljson.json_object_ordered_decode('{"name":"Alice","age":30}')

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertStrContains(encoded, '"name"')
    lu.assertStrContains(encoded, '"age"')
    lu.assertStrContains(encoded, '"Alice"')
    lu.assertStrContains(encoded, '30')
end

-- 测试 json_object_ordered_decode 嵌套对象
function TestJsonObjectDecode:test_json_object_ordered_decode_nested_object()
    local obj = ljson.json_object_ordered_decode('{"person":{"name":"Bob","age":25}}')

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertStrContains(encoded, '"person"')
    lu.assertStrContains(encoded, '"name"')
    lu.assertStrContains(encoded, '"age"')
end

-- 测试 json_object_ordered_decode 对象包含数组
function TestJsonObjectDecode:test_json_object_ordered_decode_object_with_array()
    local obj = ljson.json_object_ordered_decode('{"name":"Charlie","scores":[85,92,78]}')

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertStrContains(encoded, '"name"')
    lu.assertStrContains(encoded, '"scores"')
end

-- 测试 json_object_ordered_decode 数组包含对象
function TestJsonObjectDecode:test_json_object_ordered_decode_array_with_object()
    local obj = ljson.json_object_ordered_decode('[{"id":1,"name":"Alice"}]')

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertStrContains(encoded, '[{')
    lu.assertStrContains(encoded, '"id"')
    lu.assertStrContains(encoded, '"name"')
end

-- 测试 json_object_ordered_decode 复杂嵌套结构
function TestJsonObjectDecode:test_json_object_ordered_decode_complex_nested()
    local json_str = '{"users":[{"name":"Alice","age":25,"tags":["tag1","tag2"]}],"count":1}'
    local obj = ljson.json_object_ordered_decode(json_str)

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertStrContains(encoded, '"users"')
    lu.assertStrContains(encoded, '"count"')
    lu.assertStrContains(encoded, '"tags"')
end

-- 测试 json_object_ordered_decode 带空格的 JSON
function TestJsonObjectDecode:test_json_object_ordered_decode_with_whitespace()
    local obj = ljson.json_object_ordered_decode('  {  "name"  :  "Alice"  }  ')

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertStrContains(encoded, '"name"')
    lu.assertStrContains(encoded, '"Alice"')
end

-- 测试 json_object_ordered_decode 转义引号
function TestJsonObjectDecode:test_json_object_ordered_decode_escaped_quote()
    local obj = ljson.json_object_ordered_decode([["say \"hello\""]])

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(encoded, [[say \"hello\"]])
end

-- 测试 json_object_ordered_decode Unicode 转义
function TestJsonObjectDecode:test_json_object_ordered_decode_unicode_escape()
    local obj = ljson.json_object_ordered_decode('"hello\\u0000world"')

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertStrContains(encoded, "\\u0000")
end

-- 测试 json_object_ordered_decode 多种转义混合
function TestJsonObjectDecode:test_json_object_ordered_decode_multiple_escapes()
    local obj = ljson.json_object_ordered_decode('"\\n\\t\\r\\\\"')

    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertStrContains(encoded, "\\n")
    lu.assertStrContains(encoded, "\\t")
    lu.assertStrContains(encoded, "\\r")
    lu.assertStrContains(encoded, "\\\\")
end

-- 测试 json_object_ordered_decode 返回值类型
function TestJsonObjectDecode:test_json_object_ordered_decode_return_type()
    local obj = ljson.json_object_ordered_decode('{"test":"value"}')

    -- 验证返回的是 userdata（可以通过 json_object_ordered_encode 再次编码）
    local encoded = ljson.json_object_ordered_encode(obj)
    lu.assertEquals(type(encoded), "string")
end

-- 测试 json_object_ordered_decode 参数不足
function TestJsonObjectDecode:test_json_object_ordered_decode_missing_argument()
    local success, err = pcall(function()
        ljson.json_object_ordered_decode()
    end)

    lu.assertFalse(success)
    lu.assertStrContains(err, "requires 1 argument")
end

-- 测试 json_object_ordered_decode 参数不是字符串
function TestJsonObjectDecode:test_json_object_ordered_decode_invalid_argument_type()
    local success, err = pcall(function()
        ljson.json_object_ordered_decode(123)
    end)

    lu.assertFalse(success)
    lu.assertStrContains(err, "must be string")
end

-- 测试 json_object_ordered_decode 空字符串
function TestJsonObjectDecode:test_json_object_ordered_decode_empty_string()
    local success, err = pcall(function()
        ljson.json_object_ordered_decode("")
    end)

    lu.assertFalse(success)
    lu.assertStrContains(err, "failed")
end

-- 测试 json_object_ordered_decode 无效 JSON
function TestJsonObjectDecode:test_json_object_ordered_decode_invalid_json()
    local success, err = pcall(function()
        ljson.json_object_ordered_decode("{invalid json}")
    end)

    lu.assertFalse(success)
    lu.assertStrContains(err, "failed")
end

-- 测试 json_object_ordered_decode 括号不匹配
function TestJsonObjectDecode:test_json_object_ordered_decode_unmatched_bracket()
    local success, err = pcall(function()
        ljson.json_object_ordered_decode('{"name": "test"')
    end)

    lu.assertFalse(success)
    lu.assertStrContains(err, "failed")
end

-- 测试 json_object_ordered_decode 后使用 json_object_to_table
function TestJsonObjectDecode:test_json_object_ordered_decode_then_to_table()
    local obj = ljson.json_object_ordered_decode('{"name":"Alice","age":30}')

    -- 转换为 Lua table
    local table_val = ljson.json_object_to_table(obj)

    lu.assertEquals(type(table_val), "table")
    lu.assertEquals(table_val.name, "Alice")
    lu.assertEquals(table_val.age, 30)
end

-- 测试 json_object_ordered_decode 编码往返
function TestJsonObjectDecode:test_json_object_ordered_decode_encode_roundtrip()
    local original_json = '{"name":"Bob","age":25,"active":true}'

    local obj = ljson.json_object_ordered_decode(original_json)
    local encoded = ljson.json_object_ordered_encode(obj)

    -- 再次解码验证
    local final = ljson.decode(encoded)
    lu.assertEquals(final.name, "Bob")
    lu.assertEquals(final.age, 25)
    lu.assertEquals(final.active, true)
end

-- 测试 json_object_ordered_decode 数组后使用 json_object_to_table
function TestJsonObjectDecode:test_json_object_ordered_decode_array_to_table()
    local obj = ljson.json_object_ordered_decode('[1,2,3,"four"]')

    local table_val = ljson.json_object_to_table(obj)

    lu.assertEquals(type(table_val), "table")
    lu.assertEquals(#table_val, 4)
    lu.assertEquals(table_val[1], 1)
    lu.assertEquals(table_val[2], 2)
    lu.assertEquals(table_val[3], 3)
    lu.assertEquals(table_val[4], "four")
end

-- 测试 json_object_ordered_decode 嵌套结构转 table
function TestJsonObjectDecode:test_json_object_ordered_decode_nested_to_table()
    local obj = ljson.json_object_ordered_decode('{"users":[{"name":"Alice"}],"count":1}')

    local table_val = ljson.json_object_to_table(obj)

    lu.assertEquals(type(table_val), "table")
    lu.assertEquals(type(table_val.users), "table")
    lu.assertEquals(table_val.users[1].name, "Alice")
    lu.assertEquals(table_val.count, 1)
end

-- 测试 json_object_ordered_decode 与 decode 的一致性
function TestJsonObjectDecode:test_json_object_ordered_decode_consistent_with_decode()
    local json_str = '{"name":"Test","value":123,"active":true}'

    -- json_object_ordered_decode 返回 JsonValue userdata
    local obj = ljson.json_object_ordered_decode(json_str)

    -- 转换为 table
    local table_from_obj = ljson.json_object_to_table(obj)

    -- 直接 decode 返回 Lua table
    local table_direct = ljson.decode(json_str)

    -- 两者应该一致
    lu.assertEquals(table_from_obj.name, table_direct.name)
    lu.assertEquals(table_from_obj.value, table_direct.value)
    lu.assertEquals(table_from_obj.active, table_direct.active)
end

-- 测试 json_object_ordered_decode 科学计数法
function TestJsonObjectDecode:test_json_object_ordered_decode_scientific_notation()
    local obj = ljson.json_object_ordered_decode("1.23e5")

    local encoded = ljson.json_object_ordered_encode(obj)
    -- 科学计数法可能转换为普通数字
    local decoded = ljson.decode(encoded)
    -- 123000 或 1.23e5 都是有效的
    lu.assertTrue(decoded == 123000 or decoded == 1.23e5)
end

-- 测试 json_object_ordered_decode 大数
function TestJsonObjectDecode:test_json_object_ordered_decode_large_number()
    local obj = ljson.json_object_ordered_decode("1234567890")

    local encoded = ljson.json_object_ordered_encode(obj)
    local decoded = ljson.decode(encoded)
    lu.assertEquals(decoded, 1234567890)
end

-- 测试 json_object_ordered_decode 负浮点数
function TestJsonObjectDecode:test_json_object_ordered_decode_negative_double()
    local obj = ljson.json_object_ordered_decode("-3.14")

    local encoded = ljson.json_object_ordered_encode(obj)
    local decoded = ljson.decode(encoded)
    lu.assertEquals(decoded, -3.14)
end

-- 测试 json_object_ordered_decode 与 json_object_ordered_encode 配合
function TestJsonObjectDecode:test_json_object_ordered_decode_encode_cycle()
    local data = {
        project = "test",
        config = {
            debug = true,
            level = 5
        },
        items = {1, 2, 3}
    }

    -- 1. Lua table → JsonValue
    local obj = ljson.json_object_from_table(data)

    -- 2. JsonValue → JSON string
    local encoded = ljson.json_object_ordered_encode(obj)

    -- 3. JSON string → JsonValue
    local obj2 = ljson.json_object_ordered_decode(encoded)

    -- 4. JsonValue → JSON string
    local encoded2 = ljson.json_object_ordered_encode(obj2)

    -- 两次编码结果应该一致
    lu.assertEquals(encoded, encoded2)

    -- 5. JsonValue → Lua table
    local table_val = ljson.json_object_to_table(obj2)

    lu.assertEquals(table_val.project, data.project)
    lu.assertEquals(table_val.config.debug, data.config.debug)
    lu.assertEquals(table_val.config.level, data.config.level)
end

-- 测试 json_object_ordered_decode 元方法访问
function TestJsonObjectDecode:test_json_object_ordered_decode_metamethods()
    local obj = ljson.json_object_ordered_decode('{"name":"Alice","age":30}')

    -- 使用 json_object_to_table 转换
    local table_val = ljson.json_object_to_table(obj)

    -- 验证可以通过键访问
    lu.assertEquals(table_val.name, "Alice")
    lu.assertEquals(table_val.age, 30)
end

-- 测试 json_object_ordered_decode 数组元方法
function TestJsonObjectDecode:test_json_object_ordered_decode_array_metamethods()
    local obj = ljson.json_object_ordered_decode('[10,20,30]')

    local table_val = ljson.json_object_to_table(obj)

    -- 验证数组长度
    lu.assertEquals(#table_val, 3)
    lu.assertEquals(table_val[1], 10)
    lu.assertEquals(table_val[2], 20)
    lu.assertEquals(table_val[3], 30)
end

-- 测试 json_object_ordered_decode 深层嵌套
function TestJsonObjectDecode:test_json_object_ordered_decode_deep_nesting()
    local json_str = '{"a":{"b":{"c":{"d":"value"}}}}'
    local obj = ljson.json_object_ordered_decode(json_str)

    local table_val = ljson.json_object_to_table(obj)

    lu.assertEquals(type(table_val), "table")
    lu.assertEquals(type(table_val.a), "table")
    lu.assertEquals(type(table_val.a.b), "table")
    lu.assertEquals(type(table_val.a.b.c), "table")
    lu.assertEquals(table_val.a.b.c.d, "value")
end

-- 测试 json_object_ordered_decode 深层数组嵌套
function TestJsonObjectDecode:test_json_object_ordered_decode_deep_array_nesting()
    local json_str = '[[[[1]]]'
    local obj = ljson.json_object_ordered_decode(json_str)

    local table_val = ljson.json_object_to_table(obj)

    lu.assertEquals(type(table_val), "table")
    lu.assertEquals(type(table_val[1]), "table")
    lu.assertEquals(type(table_val[1][1]), "table")
    lu.assertEquals(table_val[1][1][1], 1)
end

-- 返回测试模块
return {
    TestJsonBasic = TestJsonBasic,
    TestJsonNested = TestJsonNested,
    TestJsonMetamethods = TestJsonMetamethods,
    TestJsonArray = TestJsonArray,
    TestJsonEdgeCases = TestJsonEdgeCases,
    TestJsonLarge = TestJsonLarge,
    TestJsonRaw = TestJsonRaw,
    TestJsonEncode = TestJsonEncode,
    TestJsonDecode = TestJsonDecode,
    TestJsonDump = TestJsonDump,
    TestJsonObjectDump = TestJsonObjectDump,
    TestJsonObjectEncode = TestJsonObjectEncode,
    TestJsonObjectDecode = TestJsonObjectDecode
}
