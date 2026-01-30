# JSON Lua API 使用说明

## 概述

`ljson` 模块提供了在 Lua 中操作 JSON 数据的功能。该模块通过 userdata 的方式封装 JSON 对象，避免了 JSON 对象作为 Lua table 存储时键值无序的问题，同时保持了键的插入顺序。

## 加载模块

```lua
local ljson = require("ljson")
```

## 接口说明

### 序列化和反序列化

#### `encode(value)`

将 Lua 值编码为 JSON 字符串（紧凑格式）。

**参数：**
- `value`: Lua 值（nil、boolean、number、string、table）

**返回值：** JSON 字符串（string）

**说明：**
- 将 Lua 值转换为紧凑格式的 JSON 字符串（无缩进和换行）
- 支持所有 Lua 基础类型：nil、boolean、number、string、table
- table 会根据内容自动识别为对象或数组：
  - 如果是连续整数索引从 1 开始，则编码为数组
  - 否则编码为对象
- 空 table 默认编码为空数组 `[]`，可通过 `encode_empty_table_as_object` 修改
- 支持嵌套的 table，会递归转换
- 对象的键会按照字符串顺序排序

**示例：**
```lua
local ljson = require("ljson")

-- 编码基本类型
print(ljson.encode(nil))           -- "null"
print(ljson.encode(true))          -- "true"
print(ljson.encode(42))            -- "42"
print(ljson.encode("hello"))       -- "\"hello\""

-- 编码数组
local arr = {1, 2, 3, "four"}
print(ljson.encode(arr))           -- "[1,2,3,"four"]"

-- 编码对象
local obj = {name = "Alice", age = 30}
print(ljson.encode(obj))           -- "{\"age\":30,\"name\":\"Alice\"}"

-- 编码嵌套结构
local nested = {
    user = "Alice",
    profile = {
        age = 30,
        email = "alice@example.com"
    },
    tags = {"tag1", "tag2"}
}
print(ljson.encode(nested))
-- {"profile":{"age":30,"email":"alice@example.com"},"tags":["tag1","tag2"],"user":"Alice"}
```

#### `decode(json_string)`

将 JSON 字符串解码为 Lua 值。

**参数：**
- `json_string`: JSON 字符串

**返回值：** Lua 值（table、nil、boolean、number、string）

**说明：**
- 将 JSON 字符串解析为 Lua 值
- JSON 对象转换为 Lua table
- JSON 数组转换为 Lua 数组 table
- JSON null 转换为 Lua nil
- 递归转换所有嵌套对象和数组

**示例：**
```lua
local ljson = require("ljson")

-- 解码 JSON 字符串
local json_str = '{"name":"Alice","age":30}'
local obj = ljson.decode(json_str)
print(obj.name)   -- "Alice"
print(obj.age)    -- 30

-- 解码数组
local arr_str = '[1,2,3,"four"]'
local arr = ljson.decode(arr_str)
for i = 1, #arr do
    print(arr[i])
end

-- 解码嵌套结构
local nested_str = '{"user":"Alice","profile":{"age":30}}'
local nested = ljson.decode(nested_str)
print(nested.profile.age)  -- 30
```

#### `json_object_ordered_decode(json_string)`

将 JSON 字符串解码为 JSON 对象（userdata），保持键的插入顺序。

**参数：**
- `json_string`: JSON 字符串

**返回值：** JSON 对象或数组（userdata）

**说明：**
- 将 JSON 字符串解析为 JsonValue 对象（userdata）
- 与 `decode` 不同，本函数返回 userdata 而不是普通 table
- userdata 会保持 JSON 对象中键的插入顺序
- 适合需要保持键顺序或需要使用 JSON 对象元方法的场景
- 返回的 userdata 支持所有 JSON 对象的元方法（__index、__newindex、__len、__pairs 等）

**示例：**
```lua
local ljson = require("ljson")

-- 解码 JSON 字符串为 userdata
local json_str = '{"name":"Alice","age":30,"email":"alice@example.com"}'
local json_obj = ljson.json_object_ordered_decode(json_str)

-- 通过元方法访问元素
print(json_obj.name)   -- "Alice"
print(json_obj.age)    -- 30

-- 获取所有键（保持插入顺序）
local keys = ljson.json_object_get_keys(json_obj)
for i, k in ipairs(keys) do
    print("键", i, ":", k)
end
-- 输出顺序：name, age, email（保持 JSON 中的顺序）

-- 遍历对象（保持键的顺序）
for k, v in pairs(json_obj) do
    local val = ljson.json_object_to_table(v)
    print("键:", k, "值:", val)
end

-- 转换为普通 table
local tbl = ljson.json_object_to_table(json_obj)
print(tbl.name)  -- "Alice"
```

#### `json_object_ordered_encode(json_value)`

将 JSON 对象（userdata）编码为 JSON 字符串（紧凑格式）。

**参数：**
- `json_value`: JSON 对象或数组（userdata）

**返回值：** JSON 字符串（string）

**说明：**
- 将 JsonValue userdata 编码为紧凑格式的 JSON 字符串（无缩进和换行）
- 与 `encode` 不同，本函数接受 userdata 而不是普通 Lua 值
- 适合需要将 `json_object_ordered_decode` 或 `json_object_from_table` 创建的 userdata 转换为 JSON 字符串的场景
- 对象的键会按照字符串顺序排序
- 支持嵌套对象和数组的递归编码

**示例：**
```lua
local ljson = require("ljson")

-- 从 userdata 编码为 JSON 字符串
local json_obj = ljson.json_object_from_table({
    name = "Alice",
    age = 30,
    email = "alice@example.com"
})
local json_str = ljson.json_object_ordered_encode(json_obj)
print(json_str)
-- {"age":30,"email":"alice@example.com","name":"Alice"}

-- 解码后再编码
local json_str2 = '{"name":"Bob","age":25}'
local json_obj2 = ljson.json_object_ordered_decode(json_str2)
local encoded = ljson.json_object_ordered_encode(json_obj2)
print(encoded)
-- {"age":25,"name":"Bob"}

-- 编码数组
local arr = ljson.json_object_from_table({1, 2, 3, "four"})
local arr_str = ljson.json_object_ordered_encode(arr)
print(arr_str)
-- [1,2,3,"four"]

-- 编码嵌套结构
local nested = ljson.json_object_from_table({
    user = "Alice",
    profile = {
        age = 30,
        email = "alice@example.com"
    },
    tags = {"tag1", "tag2"}
})
local nested_str = ljson.json_object_ordered_encode(nested)
print(nested_str)
-- {"profile":{"age":30,"email":"alice@example.com"},"tags":["tag1","tag2"],"user":"Alice"}
```

#### `json_object_ordered_encode_pretty(json_value)`

将 JSON 对象（userdata）编码为格式化的 JSON 字符串（带缩进）。

**参数：**
- `json_value`: JSON 对象或数组（userdata）

**返回值：** JSON 字符串（string）

**说明：**
- 将 JsonValue userdata 编码为格式化的 JSON 字符串（带缩进和换行）
- 与 `json_object_ordered_encode` 不同，本函数输出易读的格式化 JSON
- 与 `encode` 不同，本函数接受 userdata 而不是普通 Lua 值
- 适合需要将 `json_object_ordered_decode` 或 `json_object_from_table` 创建的 userdata 转换为易读的 JSON 字符串的场景
- 对象的键会按照字符串顺序排序
- 支持嵌套对象和数组的递归编码
- 输出的 JSON 格式易读，适合需要人工查看的场景

**示例：**
```lua
local ljson = require("ljson")

-- 从 userdata 编码为格式化的 JSON 字符串
local json_obj = ljson.json_object_from_table({
    name = "Alice",
    age = 30,
    email = "alice@example.com"
})
local json_str = ljson.json_object_ordered_encode_pretty(json_obj)
print(json_str)
-- 输出：
-- {
--   "age": 30,
--   "email": "alice@example.com",
--   "name": "Alice"
-- }

-- 解码后再编码（格式化输出）
local json_str2 = '{"name":"Bob","age":25}'
local json_obj2 = ljson.json_object_ordered_decode(json_str2)
local encoded = ljson.json_object_ordered_encode_pretty(json_obj2)
print(encoded)
-- 输出：
-- {
--   "age": 25,
--   "name": "Bob"
-- }

-- 编码数组（格式化输出）
local arr = ljson.json_object_from_table({1, 2, 3, "four"})
local arr_str = ljson.json_object_ordered_encode_pretty(arr)
print(arr_str)
-- 输出：
-- [
--   1,
--   2,
--   3,
--   "four"
-- ]

-- 编码嵌套结构（格式化输出）
local nested = ljson.json_object_from_table({
    user = "Alice",
    profile = {
        age = 30,
        email = "alice@example.com"
    },
    tags = {"tag1", "tag2"}
})
local nested_str = ljson.json_object_ordered_encode_pretty(nested)
print(nested_str)
-- 输出：
-- {
--   "profile": {
--     "age": 30,
--     "email": "alice@example.com"
--   },
--   "tags": [
--     "tag1",
--     "tag2"
--   ],
--   "user": "Alice"
-- }
```

#### `dump(value, file_path)`

将 Lua 值编码为 JSON 字符串并写入文件（紧凑格式）。

**参数：**
- `value`: Lua 值（nil、boolean、number、string、table）
- `file_path`: 目标文件路径（string）

**返回值：** boolean（成功返回 true，失败返回 false）

**说明：**
- 将 Lua 值转换为紧凑格式的 JSON 字符串（无缩进和换行）
- 将 JSON 字符串写入指定文件
- 如果文件已存在，会覆盖原文件
- 支持所有 Lua 基础类型和嵌套结构
- 编码规则与 `encode` 相同

**示例：**
```lua
local ljson = require("ljson")

-- 编码基本类型到文件
local success = ljson.dump("hello", "/tmp/output.json")
print(success)  -- true

-- 编码对象到文件
local obj = {name = "Alice", age = 30}
ljson.dump(obj, "/tmp/user.json")

-- 编码嵌套结构到文件
local nested = {
    user = "Alice",
    profile = {
        age = 30,
        email = "alice@example.com"
    },
    tags = {"tag1", "tag2"}
}
ljson.dump(nested, "/tmp/data.json")
```

#### `dump_pretty(value, file_path)`

将 Lua 值编码为格式化的 JSON 字符串并写入文件（带缩进）。

**参数：**
- `value`: Lua 值（nil、boolean、number、string、table）
- `file_path`: 目标文件路径（string）

**返回值：** boolean（成功返回 true，失败返回 false）

**说明：**
- 将 Lua 值转换为格式化的 JSON 字符串（带缩进和换行）
- 将 JSON 字符串写入指定文件
- 如果文件已存在，会覆盖原文件
- 支持所有 Lua 基础类型和嵌套结构
- 编码规则与 `encode` 相同
- 输出的 JSON 格式易读，适合需要人工查看的场景

**示例：**
```lua
local ljson = require("ljson")

-- 编码对象到文件（格式化输出）
local obj = {
    name = "Alice",
    age = 30,
    tags = {"tag1", "tag2"}
}
ljson.dump_pretty(obj, "/tmp/user_pretty.json")
-- 输出文件内容：
-- {
--   "age": 30,
--   "name": "Alice",
--   "tags": [
--     "tag1",
--     "tag2"
--   ]
-- }

-- 编码嵌套结构到文件（格式化输出）
local nested = {
    user = "Alice",
    profile = {
        age = 30,
        email = "alice@example.com"
    }
}
ljson.dump_pretty(nested, "/tmp/data_pretty.json")
```

#### `json_object_dump(json_value, file_path)`

将 JSON 对象（userdata）编码为 JSON 字符串并写入文件（紧凑格式）。

**参数：**
- `json_value`: JSON 对象或数组（userdata）
- `file_path`: 目标文件路径（string）

**返回值：** boolean（成功返回 true，失败返回 false）

**说明：**
- 将 JsonValue userdata 转换为紧凑格式的 JSON 字符串（无缩进和换行）
- 将 JSON 字符串写入指定文件
- 如果文件已存在，会覆盖原文件
- 与 `dump` 不同，本函数接受 userdata 而不是普通 Lua 值
- 适合需要将 `json_object_ordered_decode` 或 `json_object_from_table` 创建的 userdata 保存到文件的场景
- 支持嵌套对象和数组的递归编码
- 对象的键会按照字符串顺序排序

**示例：**
```lua
local ljson = require("ljson")

-- 将 userdata 保存到文件（紧凑格式）
local json_obj = ljson.json_object_from_table({
    name = "Alice",
    age = 30,
    email = "alice@example.com"
})
local success = ljson.json_object_dump(json_obj, "/tmp/user.json")
print(success)  -- true

-- 解码 JSON 字符串后保存到文件
local json_str = '{"name":"Bob","age":25}'
local json_obj2 = ljson.json_object_ordered_decode(json_str)
ljson.json_object_dump(json_obj2, "/tmp/user2.json")

-- 将数组保存到文件（紧凑格式）
local arr = ljson.json_object_from_table({1, 2, 3, "four"})
ljson.json_object_dump(arr, "/tmp/array.json")

-- 将嵌套结构保存到文件（紧凑格式）
local nested = ljson.json_object_from_table({
    user = "Alice",
    profile = {
        age = 30,
        email = "alice@example.com"
    },
    tags = {"tag1", "tag2"}
})
ljson.json_object_dump(nested, "/tmp/data.json")
```

#### `json_object_dump_pretty(json_value, file_path)`

将 JSON 对象（userdata）编码为格式化的 JSON 字符串并写入文件（带缩进）。

**参数：**
- `json_value`: JSON 对象或数组（userdata）
- `file_path`: 目标文件路径（string）

**返回值：** boolean（成功返回 true，失败返回 false）

**说明：**
- 将 JsonValue userdata 转换为格式化的 JSON 字符串（带缩进和换行）
- 将 JSON 字符串写入指定文件
- 如果文件已存在，会覆盖原文件
- 与 `dump_pretty` 不同，本函数接受 userdata 而不是普通 Lua 值
- 适合需要将 `json_object_ordered_decode` 或 `json_object_from_table` 创建的 userdata 以易读格式保存到文件的场景
- 支持嵌套对象和数组的递归编码
- 对象的键会按照字符串顺序排序
- 输出的 JSON 格式易读，适合需要人工查看的场景

**示例：**
```lua
local ljson = require("ljson")

-- 将 userdata 保存到文件（格式化输出）
local json_obj = ljson.json_object_from_table({
    name = "Alice",
    age = 30,
    email = "alice@example.com"
})
local success = ljson.json_object_dump_pretty(json_obj, "/tmp/user_pretty.json")
print(success)  -- true
-- 输出文件内容：
-- {
--   "age": 30,
--   "email": "alice@example.com",
--   "name": "Alice"
-- }

-- 解码 JSON 字符串后保存到文件（格式化输出）
local json_str = '{"name":"Bob","age":25}'
local json_obj2 = ljson.json_object_ordered_decode(json_str)
ljson.json_object_dump_pretty(json_obj2, "/tmp/user2_pretty.json")
-- 输出文件内容：
-- {
--   "age": 25,
--   "name": "Bob"
-- }

-- 将数组保存到文件（格式化输出）
local arr = ljson.json_object_from_table({1, 2, 3, "four"})
ljson.json_object_dump_pretty(arr, "/tmp/array_pretty.json")
-- 输出文件内容：
-- [
--   1,
--   2,
--   3,
--   "four"
-- ]

-- 将嵌套结构保存到文件（格式化输出）
local nested = ljson.json_object_from_table({
    user = "Alice",
    profile = {
        age = 30,
        email = "alice@example.com"
    },
    tags = {"tag1", "tag2"}
})
ljson.json_object_dump_pretty(nested, "/tmp/data_pretty.json")
-- 输出文件内容：
-- {
--   "profile": {
--     "age": 30,
--     "email": "alice@example.com"
--   },
--   "tags": [
--     "tag1",
--     "tag2"
--   ],
--   "user": "Alice"
-- }
```

#### `encode_empty_table_as_object(boolean)`

设置空 table 编码为 JSON 对象还是数组。

**参数：**
- `boolean`: 布尔值（true 表示空 table 编码为对象 `{}`，false 表示空 table 编码为数组 `[]`）

**返回值：** 无

**说明：**
- 控制空 table 的 JSON 编码方式
- 默认情况下（false），空 table 编码为空数组 `[]`
- 设置为 true 时，空 table 编码为空对象 `{}`
- 此设置是全局的，会影响后续所有 `encode`、`dump`、`json_object_from_table` 等操作

**示例：**
```lua
local ljson = require("ljson")

-- 默认行为：空 table 编码为数组
print(ljson.encode({}))  -- "[]"

-- 修改为空 table 编码为对象
ljson.encode_empty_table_as_object(true)
print(ljson.encode({}))  -- "{}"

-- 恢复默认行为
ljson.encode_empty_table_as_object(false)
print(ljson.encode({}))  -- "[]"
```

### 创建 JSON 对象和数组

#### `json_object_new_object()`

创建一个新的空 JSON 对象。

**返回值：** JSON 对象（userdata）

**示例：**
```lua
local obj = ljson.json_object_new_object()
obj.name = "Alice"
obj.age = 30
```

#### `json_object_new_array()`

创建一个新的空 JSON 数组。

**返回值：** JSON 数组（userdata）

**示例：**
```lua
local arr = ljson.json_object_new_array()
arr[1] = 100
arr[2] = 200
```

### 类型转换

#### `json_object_from_table(table)`

从 Lua table 创建 JSON 对象或数组。

**参数：**
- `table`: Lua table，可以是对象（键值对）或数组（连续整数索引）

**返回值：** JSON 对象或数组（userdata）

**说明：**
- 如果 table 是数组（连续整数索引从 1 开始），则创建 JSON 数组
- 如果 table 是对象（键值对），则创建 JSON 对象
- 对象中的键会按照字符串顺序排序，以保证稳定的顺序
- 支持嵌套的 table，会递归转换

**示例：**
```lua
-- 创建对象
local obj = ljson.json_object_from_table({
    name = "Alice",
    age = 30,
    tags = {"tag1", "tag2"}
})

-- 创建数组
local arr = ljson.json_object_from_table({1, 2, 3, "four", true})
```

#### `json_object_to_table(json_value)`

将 JSON 对象或数组转换为 Lua table。

**参数：**
- `json_value`: JSON 对象或数组（userdata）

**返回值：** Lua table（普通 table，不包含 userdata）

**说明：**
- 只接受 userdata（json_object）作为参数，如果传入普通的 Lua table 会报错
- 递归转换所有嵌套对象和数组
- 返回的 table 中所有值都是普通的 Lua 类型（nil、boolean、number、string、table），不包含 userdata
- 返回的是转换时的快照，不会自动反映 userdata 的后续变化

**示例：**
```lua
local json_obj = ljson.json_object_from_table({
    name = "Alice",
    age = 30,
    profile = {
        email = "alice@example.com"
    }
})

-- 转换为 table
local tbl = ljson.json_object_to_table(json_obj)
print(tbl.name)  -- "Alice"
print(tbl.profile.email)  -- "alice@example.com"

-- 注意：tbl.profile 已经是普通 table，不是 userdata
assert(type(tbl.profile) == "table")
assert(type(tbl.profile) ~= "userdata")
```

### 类型检查

#### `json_object_is_object(json_value)`

检查 JSON 值是否为对象。

**参数：**
- `json_value`: JSON 值（userdata）

**返回值：** boolean

**示例：**
```lua
local obj = ljson.json_object_new_object()
assert(ljson.json_object_is_object(obj) == true)
```

#### `json_object_is_array(json_value)`

检查 JSON 值是否为数组。

**参数：**
- `json_value`: JSON 值（userdata）

**返回值：** boolean

**示例：**
```lua
local arr = ljson.json_object_new_array()
assert(ljson.json_object_is_array(arr) == true)
```

### 对象操作

#### `json_object_get_keys(json_object)`

获取 JSON 对象的所有键，按照插入顺序返回。

**参数：**
- `json_object`: JSON 对象（userdata）

**返回值：** Lua 数组，包含所有键（字符串）

**示例：**
```lua
local obj = ljson.json_object_from_table({first = 1, second = 2, third = 3})
local keys = ljson.json_object_get_keys(obj)
-- keys = {"first", "second", "third"}
for i, k in ipairs(keys) do
    print("键", i, ":", k)
end
```

#### `json_object_is_equal(json1, json2)`

比较两个 JSON 值是否相等。

**参数：**
- `json1`: 第一个 JSON 值（userdata 或 Lua 值）
- `json2`: 第二个 JSON 值（userdata 或 Lua 值）

**返回值：** boolean

**说明：**
- 支持深度比较，包括嵌套对象和数组
- 如果参数不是 userdata，会自动转换为 JSON 值进行比较

**示例：**
```lua
local obj1 = ljson.json_object_from_table({a = 1, b = 2})
local obj2 = ljson.json_object_from_table({a = 1, b = 2})
assert(ljson.json_object_is_equal(obj1, obj2) == true)
```

#### `json_object_copy(json_value)`

复制 JSON 对象或数组，创建独立的副本。

**参数：**
- `json_value`: JSON 对象或数组（userdata）

**返回值：** JSON 对象或数组的副本（userdata）

**说明：**
- 创建深拷贝，修改副本不会影响原始对象
- 支持嵌套对象和数组的递归复制

**示例：**
```lua
local original = ljson.json_object_from_table({
    name = "Alice",
    data = {value = 100}
})

local copied = ljson.json_object_copy(original)
copied.name = "Bob"
copied.data.value = 200

-- 原始对象未被修改
local orig_tbl = ljson.json_object_to_table(original)
assert(orig_tbl.name == "Alice")
assert(orig_tbl.data.value == 100)
```

### 底层操作（高级用法）

#### `to_raw(json_value)`

获取 JSON 值的底层 C 指针（userdata）。

**参数：**
- `json_value`: JSON 值（userdata）

**返回值：** 底层指针（userdata）

**说明：** 仅用于高级场景，一般不需要使用。

#### `new_from_raw(raw_ptr)`

从底层 C 指针创建 JSON 值。

**参数：**
- `raw_ptr`: 底层指针（userdata）

**返回值：** JSON 值（userdata）

**说明：** 仅用于高级场景，一般不需要使用。

## 元方法

JSON 对象和数组支持以下 Lua 元方法，可以通过类似操作普通 table 的方式来操作 JSON 对象。

### `__index` - 访问元素

支持通过 `obj[key]` 或 `arr[index]` 的方式访问 JSON 对象和数组的元素。

**对象访问：**
```lua
local obj = ljson.json_object_from_table({name = "Alice", age = 30})
print(obj.name)  -- "Alice"
print(obj["age"])  -- 30
```

**数组访问：**
```lua
local arr = ljson.json_object_from_table({10, 20, 30})
print(arr[1])  -- 10
print(arr[2])  -- 20
```

### `__newindex` - 设置元素

支持通过 `obj[key] = value` 或 `arr[index] = value` 的方式设置 JSON 对象和数组的元素。

**对象设置：**
```lua
local obj = ljson.json_object_new_object()
obj.name = "Alice"
obj.age = 30
obj.nested = {key = "value"}  -- 支持直接设置 Lua table，会自动转换
```

**数组设置：**
```lua
local arr = ljson.json_object_new_array()
arr[1] = 100
arr[2] = 200
arr[3] = 300

-- 修改已存在的元素
arr[2] = 250

-- 在末尾添加新元素
arr[4] = 400

-- 索引超出范围时会自动扩展数组（用 null 填充）
arr[10] = 1000  -- 索引 5-9 会被填充为 null
```

### `__len` - 获取长度

支持通过 `#obj` 或 `#arr` 的方式获取 JSON 对象或数组的长度。

**对象长度：**
```lua
local obj = ljson.json_object_from_table({a = 1, b = 2, c = 3})
print(#obj)  -- 3（键值对的数量）
```

**数组长度：**
```lua
local arr = ljson.json_object_from_table({10, 20, 30})
print(#arr)  -- 3（元素的数量）
```

### `__pairs` - 遍历对象

支持通过 `for k, v in pairs(obj)` 的方式遍历 JSON 对象，键的顺序与插入顺序一致。

```lua
local obj = ljson.json_object_from_table({first = 1, second = 2, third = 3})
for k, v in pairs(obj) do
    local val = ljson.json_object_to_table(v)
    print("键:", k, "值:", val)
end
-- 输出顺序：first, second, third（保持插入顺序）
```

**注意：** 遍历时返回的值 `v` 是 userdata，如果需要获取实际值，需要使用 `json_object_to_table(v)` 转换。

### `__gc` - 垃圾回收

自动管理 JSON 对象的生命周期，当 Lua 对象被垃圾回收时，会自动释放底层的 JSON 资源。

## 使用示例

### 示例 1：创建和操作 JSON 对象

```lua
local ljson = require("ljson")

-- 创建对象
local obj = ljson.json_object_new_object()
obj.name = "Alice"
obj.age = 30
obj.email = "alice@example.com"

-- 访问元素
print(obj.name)  -- "Alice"
print(obj.age)   -- 30

-- 获取所有键
local keys = ljson.json_object_get_keys(obj)
for i, k in ipairs(keys) do
    print("键", i, ":", k)
end

-- 转换为 table
local tbl = ljson.json_object_to_table(obj)
print(tbl.name)  -- "Alice"
```

### 示例 2：创建和操作 JSON 数组

```lua
local ljson = require("ljson")

-- 创建数组
local arr = ljson.json_object_new_array()
arr[1] = 100
arr[2] = 200
arr[3] = 300

-- 访问元素
print(arr[1])  -- 100
print(arr[2])  -- 200

-- 获取长度
print(#arr)  -- 3

-- 修改元素
arr[2] = 250

-- 转换为 table
local tbl = ljson.json_object_to_table(arr)
for i = 1, #tbl do
    print("元素", i, ":", tbl[i])
end
```

### 示例 3：嵌套结构

```lua
local ljson = require("ljson")

-- 创建嵌套对象
local obj = ljson.json_object_from_table({
    name = "Alice",
    profile = {
        age = 30,
        email = "alice@example.com",
        address = {
            city = "Beijing",
            country = "China"
        }
    },
    tags = {"tag1", "tag2", "tag3"}
})

-- 访问嵌套属性
print(obj.profile.email)  -- "alice@example.com"
print(obj.profile.address.city)  -- "Beijing"

-- 转换为 table（递归转换）
local tbl = ljson.json_object_to_table(obj)
print(tbl.profile.email)  -- "alice@example.com"
-- tbl.profile 已经是普通 table，不是 userdata
assert(type(tbl.profile) == "table")
assert(type(tbl.profile) ~= "userdata")
```

### 示例 4：遍历对象（保持键的顺序）

```lua
local ljson = require("ljson")

-- 创建对象
local obj = ljson.json_object_from_table({
    first = 1,
    second = 2,
    third = 3
})

-- 遍历对象（键的顺序与插入顺序一致）
for k, v in pairs(obj) do
    local val = ljson.json_object_to_table(v)
    print("键:", k, "值:", val)
end
-- 输出：
-- 键: first 值: 1
-- 键: second 值: 2
-- 键: third 值: 3
```

### 示例 5：复制对象

```lua
local ljson = require("ljson")

-- 创建原始对象
local original = ljson.json_object_from_table({
    name = "Alice",
    data = {value = 100}
})

-- 复制对象
local copied = ljson.json_object_copy(original)

-- 修改副本
copied.name = "Bob"
copied.data.value = 200

-- 验证原始对象未被修改
local orig_tbl = ljson.json_object_to_table(original)
assert(orig_tbl.name == "Alice")
assert(orig_tbl.data.value == 100)

-- 验证副本已修改
local copy_tbl = ljson.json_object_to_table(copied)
assert(copy_tbl.name == "Bob")
assert(copy_tbl.data.value == 200)
```

### 示例 6：比较对象

```lua
local ljson = require("ljson")

-- 创建两个相同的对象
local obj1 = ljson.json_object_from_table({a = 1, b = 2, c = 3})
local obj2 = ljson.json_object_from_table({a = 1, b = 2, c = 3})

-- 比较对象
assert(ljson.json_object_is_equal(obj1, obj2) == true)

-- 修改其中一个对象
obj2.c = 4
assert(ljson.json_object_is_equal(obj1, obj2) == false)
```

## 注意事项

### 1. 键的顺序

- JSON 对象中的键会按照字符串顺序排序，以保证稳定的顺序
- 使用 `pairs` 遍历时，键的顺序与插入顺序一致（实际上是按字符串顺序）

### 2. `json_object_to_table` 的行为

- `json_object_to_table` 返回的是转换时的快照，不会自动反映 userdata 的后续变化
- 如果需要获取最新状态，需要重新调用 `json_object_to_table`
- 转换后的 table 中所有嵌套对象和数组都是普通 table，不包含 userdata

**示例：**
```lua
local obj = ljson.json_object_new_object()
obj.key1 = "value1"

-- 转换为 table
local tbl = ljson.json_object_to_table(obj)
assert(tbl.key1 == "value1")

-- 修改 userdata
obj.key2 = "value2"

-- 旧的 table 不会自动更新
assert(tbl.key2 == nil)  -- 旧 table 中没有 key2

-- 需要重新转换以获取最新状态
local tbl2 = ljson.json_object_to_table(obj)
assert(tbl2.key2 == "value2")  -- 新 table 中有 key2
```

### 3. 数组中的 `nil` 值

- Lua 的 `#` 操作符在遇到 `nil` 值时会停止计数
- 如果 JSON 数组中包含 `null` 值，转换为 Lua table 后，`#` 操作符可能无法正确计算长度
- 建议直接通过索引访问元素，而不是依赖 `#` 操作符

**示例：**
```lua
local arr = ljson.json_object_from_table({1, 2, 3, nil, 5})
local tbl = ljson.json_object_to_table(arr)

-- #tbl 会返回 3（在 nil 处停止），而不是 5
-- 应该直接通过索引访问
assert(tbl[1] == 1)
assert(tbl[2] == 2)
assert(tbl[3] == 3)
assert(tbl[4] == nil)
assert(tbl[5] == 5)
```

### 4. 类型检查

- `json_object_is_object` 和 `json_object_is_array` 只接受 userdata 参数
- 如果传入已转换的普通 table，这些函数会报错
- 在调用这些函数之前，确保参数是 userdata

**示例：**
```lua
local obj = ljson.json_object_from_table({name = "Alice"})
assert(ljson.json_object_is_object(obj) == true)  -- 正确

local tbl = ljson.json_object_to_table(obj)
-- ljson.json_object_is_object(tbl)  -- 错误！tbl 是普通 table，不是 userdata
```

### 5. 性能考虑

- `json_object_from_table` 和 `json_object_to_table` 会进行深度转换，对于大型嵌套结构可能有性能开销
- 如果需要频繁访问，建议保持 userdata 形式，而不是频繁转换

### 6. 错误处理

- 所有接口在出错时会抛出 Lua 错误
- 建议使用 `pcall` 包装可能出错的操作

**示例：**
```lua
local success, err = pcall(function()
    ljson.json_object_to_table({key = "value"})  -- 错误：只接受 userdata
end)
if not success then
    print("错误:", err)
end
```

## 相关文档

- [Lua API 索引](../index.md)
