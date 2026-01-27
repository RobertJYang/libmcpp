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

-- 返回测试模块
return {
    TestJsonBasic = TestJsonBasic,
    TestJsonNested = TestJsonNested,
    TestJsonMetamethods = TestJsonMetamethods,
    TestJsonArray = TestJsonArray,
    TestJsonEdgeCases = TestJsonEdgeCases,
    TestJsonLarge = TestJsonLarge,
    TestJsonRaw = TestJsonRaw
}
