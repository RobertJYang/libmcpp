# mdb_access Lua API 文档

## 概述

`mdb_access` 模块提供了访问 D-Bus 对象的 Lua 接口，支持通过代理对象访问属性、调用方法等操作。该模块位于 `lmdb` 模块下，作为子模块使用。

## 模块加载

```lua
local lmdb = require("lmdb")
local mdb_access = lmdb.mdb_access
```

## 模块函数

### mdb_access.get_object(bus, path, interface)

获取指定路径和接口的代理对象（自动查找 service）。

**参数：**
- `bus` (lightuserdata): D-Bus 连接对象（DBusConnection）
- `path` (string): 对象路径，例如 `"/bmc/kepler/AccountService/Roles"`
- `interface` (string): 接口名称，例如 `"bmc.kepler.AccountService.Role"`

**返回值：**
- 返回一个代理对象（userdata），支持属性访问和方法调用

**示例：**
```lua
local bus = dbus.get_session_bus()
local obj = mdb_access.get_object(bus, "/bmc/kepler/AccountService/Roles", "bmc.kepler.AccountService.Role")
```

**说明：**
- 此方法会自动调用 `mdb::service::get_object` 查找 service 名称
- 适用于不知道 service 名称的场景
- 其他行为与 `get_object_with_service` 相同，包括缓存机制

**错误：**
- 如果参数类型不正确，会抛出 Lua 错误
- 如果无法获取对象，会抛出异常

---

### mdb_access.get_object_with_service(bus, service, path, interface)

使用指定的 service 名称获取代理对象。

**参数：**
- `bus` (lightuserdata): D-Bus 连接对象（DBusConnection）
- `service` (string): 服务名称，例如 `"org.example.Service"`
- `path` (string): 对象路径，例如 `"/org/example/Object"`
- `interface` (string): 接口名称，例如 `"org.example.Interface"`

**返回值：**
- 返回一个代理对象（userdata），支持属性访问和方法调用

**说明：**
- 此方法不会查找 service，直接使用提供的 service 名称
- 会对 service 名称进行判空检查，如果为空则抛出异常
- 适用于已知 service 名称的场景，可以避免额外的服务查找开销
- 其他行为与 `get_object` 相同，包括缓存机制

**示例：**
```lua
local bus = dbus.get_session_bus()
-- 已知 service 名称，直接使用
local obj = mdb_access.get_object_with_service(
    bus,
    "org.example.Service",  -- 直接指定 service 名称
    "/org/example/Object",
    "org.example.Interface"
)
```

**错误：**
- 如果 service 名称为空，会抛出 Lua 错误
- 如果参数类型不正确，会抛出 Lua 错误
- 如果无法获取对象，会抛出异常

---

### mdb_access.get_object_by_short_call(bus, path, interface)

通过短调用方式获取指定路径和接口的代理对象。

**参数：**
- `bus` (lightuserdata): D-Bus 连接对象（DBusConnection）
- `path` (string): 对象路径
- `interface` (string): 接口名称

**返回值：**
- 返回一个代理对象（userdata）

**说明：**
- 与 `get_object` 功能相同，但使用短调用方式，适用于某些特殊场景

---

### mdb_access.get_all(bus, path, interface)

获取指定对象的所有属性。

**参数：**
- `bus` (lightuserdata): D-Bus 连接对象（DBusConnection）
- `path` (string): 对象路径
- `interface` (string): 接口名称

**返回值：**
- 返回一个 Lua table，包含所有属性的键值对

**示例：**
```lua
local bus = dbus.get_session_bus()
local props = mdb_access.get_all(bus, "/bmc/kepler/AccountService/Roles", "bmc.kepler.AccountService.Role")
for key, value in pairs(props) do
    print(key, value)
end
```

---

### mdb_access.get_properties(bus, path, interface, props)

批量获取指定对象的属性。

**参数：**
- `bus` (lightuserdata): D-Bus 连接对象（DBusConnection）
- `path` (string): 对象路径
- `interface` (string): 接口名称
- `props` (table): 属性名列表，例如 `{"Id", "Name", "Enabled"}`

**返回值：**
- 返回两个值：
  1. `props_dict` (table): 属性数据字典，键为属性名，值为属性值
  2. `errs_dict` (table): 错误信息字典，键为属性名，值为错误信息（如果属性获取失败）

**示例：**
```lua
local bus = dbus.get_session_bus()
local props, errs = mdb_access.get_properties(
    bus,
    "/bmc/kepler/AccountService/Roles",
    "bmc.kepler.AccountService.Role",
    {"Id", "Name", "Enabled"}
)

-- 检查属性
if props.Id then
    print("Id:", props.Id)
end

-- 检查错误
if errs.Name then
    print("Error getting Name:", errs.Name)
end
```

---

### mdb_access.get_sub_objects(bus, path, interface, depth)

获取指定路径下的所有子对象。

**参数：**
- `bus` (lightuserdata): D-Bus 连接对象（DBusConnection）
- `path` (string): 对象路径
- `interface` (string): 接口名称
- `depth` (integer, 可选): 搜索深度，默认为 1

**返回值：**
- 返回一个带元表的 Lua table，键为子对象路径，值为对应的代理对象
- 返回的 table 具有 `Objects` 元表，支持 `fold` 方法

**示例：**
```lua
local bus = dbus.get_session_bus()
local sub_objs = mdb_access.get_sub_objects(
    bus,
    "/bmc/kepler/AccountService/Roles",
    "bmc.kepler.AccountService.Role",
    1
)

-- 方式1：使用 pairs 遍历
for path, obj in pairs(sub_objs) do
    print("Path:", path)
    print("Id:", obj.Id)
end

-- 方式2：使用 fold 方法（函数式编程）
local total_count = sub_objs:fold(function(obj, acc)
    return acc + (obj.Count or 0), true  -- 返回 (新累积值, 是否继续)
end, 0)

-- 查找满足条件的对象路径
local active_paths = sub_objs:fold(function(obj, acc)
    if obj.Status == "active" then
        table.insert(acc, obj.Path)
    end
    return acc, true  -- 继续遍历
end, {})

-- 提前退出示例
local found_obj = sub_objs:fold(function(obj, acc)
    if obj.Id == target_id then
        return obj, false  -- 找到后停止遍历
    end
    return acc, true  -- 继续遍历
end, nil)
```

---

## Objects 集合方法

`get_sub_objects` 返回的对象集合具有 `Objects` 元表，提供了函数式编程方法。

### fold 方法

对集合中的每个对象执行累积操作。

**语法：**
```lua
local result = obj_list:fold(f, initial_acc)
-- 或
local result = obj_list.fold(f, initial_acc)
```

**参数：**
- `f` (function): 累积函数，接收两个参数 `(obj, acc_val)`，返回两个值 `(val, continue_flag)`
  - `obj`: 当前对象
  - `acc_val`: 当前的累积值
  - `val`: 新的累积值
  - `continue_flag` (boolean): 是否继续遍历，`false` 表示提前退出
- `initial_acc` (any, 可选): 初始累积值，默认为空表 `{}`

**返回值：**
- 返回最终的累积值

**说明：**
- `fold` 方法遍历集合中的每个对象（值），忽略键（路径）
- 累积函数 `f` 的参数顺序是 `f(obj, acc_val)`，不是 `f(acc_val, obj)`
- 累积函数必须返回两个值：`(val, continue_flag)`
- 如果 `continue_flag` 为 `false`，会提前退出循环
- 如果 `initial_acc` 未提供或为 `nil`，默认使用空表 `{}`

**使用示例：**

```lua
local sub_objs = mdb_access.get_sub_objects(bus, path, interface)

-- 示例1：计算所有对象的 Count 属性之和
local total = sub_objs:fold(function(obj, acc)
    return acc + (obj.Count or 0), true  -- 返回 (新值, 继续)
end, 0)

-- 示例2：收集所有满足条件的路径
local active_paths = sub_objs:fold(function(obj, acc)
    if obj.Status == "active" then
        table.insert(acc, obj.Path)
    end
    return acc, true  -- 继续遍历
end, {})

-- 示例3：查找第一个满足条件的对象（提前退出）
local found = sub_objs:fold(function(obj, acc)
    if obj.Id == target_id then
        return obj, false  -- 找到后停止遍历
    end
    return acc, true  -- 继续遍历
end, nil)

-- 示例4：构建映射表（Id -> Path）
local id_to_path = sub_objs:fold(function(obj, acc)
    acc[obj.Id] = obj.Path
    return acc, true
end, {})

-- 示例5：统计不同状态的对象数量
local status_count = sub_objs:fold(function(obj, acc)
    local status = obj.Status or "unknown"
    acc[status] = (acc[status] or 0) + 1
    return acc, true
end, {})
```

---

## 代理对象操作

通过 `mdb_access.get_object` 获取的代理对象支持以下操作：

### 属性访问

#### 读取属性

使用点号（`.`）访问属性：

```lua
local obj = mdb_access.get_object(bus, path, interface)
local value = obj.PropertyName
```

**示例：**
```lua
local obj = mdb_access.get_object(bus, "/bmc/kepler/AccountService/Roles", "bmc.kepler.AccountService.Role")
local id = obj.Id
local name = obj.Name
local enabled = obj.Enabled
```

**说明：**
- 属性名区分大小写
- 如果属性不存在，返回 `nil`
- 属性值类型根据 D-Bus 类型自动转换

**注意：**
- `get_object` 会自动查找 service 名称，适用于不知道 service 名称的场景
- `get_object_with_service` 直接使用提供的 service 名称，适用于已知 service 名称的场景，可以避免额外的查找开销

#### 设置属性

使用赋值操作设置属性：

```lua
local obj = mdb_access.get_object(bus, path, interface)
obj.PropertyName = value
```

**示例：**
```lua
local obj = mdb_access.get_object(bus, "/bmc/kepler/AccountService/Roles", "bmc.kepler.AccountService.Role")
obj.Enabled = true
obj.Name = "NewName"
```

**说明：**
- 只有可写属性（`readwrite` 或 `write`）才能设置
- 属性值会自动进行类型校验
- 如果属性不存在或不可写，会抛出错误

---

### 方法调用

代理对象支持两种方法调用方式：

#### 方式一：使用冒号语法（推荐）

```lua
local obj = mdb_access.get_object(bus, path, interface)
local result = obj:MethodName(arg1, arg2, ...)
```

**示例：**
```lua
local obj = mdb_access.get_object(bus, "/bmc/kepler/AccountService/Roles", "bmc.kepler.AccountService.Role")
local ctx = context.new("bmc.kepler.AccountService", "admin", "127.0.0.1")
local data = obj:Read(ctx, 1, 30)
```

#### 方式二：使用点号语法

```lua
local obj = mdb_access.get_object(bus, path, interface)
local result = obj.MethodName(arg1, arg2, ...)
```

**示例：**
```lua
local obj = mdb_access.get_object(bus, "/bmc/kepler/AccountService/Roles", "bmc.kepler.AccountService.Role")
local ctx = context.new("bmc.kepler.AccountService", "admin", "127.0.0.1")
local data = obj.Read(ctx, 1, 30)
```

#### 返回值格式

方法调用返回一个 Lua table（字典），键为输出参数名，值为返回值：

```lua
local result = obj:MethodName(arg1, arg2, ...)
-- result 是一个 table，例如：
-- {
--     Id = 123,
--     Name = "test",
--     Data = {1, 2, 3}  -- 数组类型
-- }
```

**示例：**
```lua
local obj = mdb_access.get_object(bus, "/bmc/kepler/AccountService/Roles", "bmc.kepler.AccountService.Role")
local ctx = context.new("bmc.kepler.AccountService", "admin", "127.0.0.1")
local rsp = obj:Read(ctx, 1, 30)

-- 访问返回值
if rsp.Id then
    print("Id:", rsp.Id)
end

-- 数组类型返回值
if rsp.Data then
    for i, v in ipairs(rsp.Data) do
        print("Data[", i, "] =", v)
    end
end
```

**说明：**
- 如果输出参数有名称，使用参数名作为键
- 如果输出参数没有名称，使用数字索引作为键（如 `"1"`, `"2"`）
- 数组类型（如 `ay`, `au`, `ai`）会自动转换为 Lua 数组
- 如果方法没有输出参数，返回空 table

---

### get_all 方法

获取代理对象的所有属性。

**语法：**
```lua
local obj = mdb_access.get_object(bus, path, interface)
local props = obj:get_all()
-- 或
local props = obj.get_all()
```

**返回值：**
- `table`: 包含所有属性的字典，键为属性名，值为属性值

**示例：**
```lua
local obj = mdb_access.get_object(bus, "/bmc/kepler/AccountService/Roles", "bmc.kepler.AccountService.Role")
local props = obj:get_all()

for key, value in pairs(props) do
    print(key, "=", value)
end
```

**说明：**
- 与 `mdb_access.get_all(bus, path, interface)` 功能相同
- 使用代理对象调用时，无需重复传入 `bus`、`path`、`interface`
- 返回所有属性的完整字典

---

### get_properties 方法

批量获取代理对象的指定属性。

**语法：**
```lua
local obj = mdb_access.get_object(bus, path, interface)
local props, errs = obj:get_properties(prop_names)
-- 或
local props, errs = obj.get_properties(prop_names)
```

**参数：**
- `prop_names` (table): 属性名列表，例如 `{"Id", "Name", "Enabled"}`

**返回值：**
- 返回两个值：
  1. `props` (table): 属性数据字典，键为属性名，值为属性值
  2. `errs` (table): 错误信息字典，键为属性名，值为错误信息（如果属性获取失败）

**示例：**
```lua
local obj = mdb_access.get_object(bus, "/bmc/kepler/AccountService/Roles", "bmc.kepler.AccountService.Role")
local props, errs = obj:get_properties({"Id", "Name", "Enabled", "Status"})

-- 检查属性
if props.Id then
    print("Id:", props.Id)
end

if props.Name then
    print("Name:", props.Name)
end

-- 检查错误
if errs.Status then
    print("Error getting Status:", errs.Status)
end
```

**说明：**
- 与 `mdb_access.get_properties(bus, path, interface, props)` 功能相同
- 使用代理对象调用时，无需重复传入 `bus`、`path`、`interface`
- 如果某个属性获取失败，会在错误字典中记录，不会抛出异常
- 返回两个值：属性数据字典和错误信息字典

---

### is_volatile 方法

检查属性是否为易变属性（volatile）。

**语法：**
```lua
local obj = mdb_access.get_object(bus, path, interface)
local is_vol = obj:is_volatile(prop_name)
-- 或
local is_vol = obj.is_volatile(prop_name)
```

**参数：**
- `prop_name` (string): 属性名

**返回值：**
- `boolean`: 如果属性是易变的，返回 `true`，否则返回 `false`

**示例：**
```lua
local obj = mdb_access.get_object(bus, "/bmc/kepler/AccountService/Roles", "bmc.kepler.AccountService.Role")

-- 使用冒号语法（推荐）
if obj:is_volatile("Status") then
    print("Status is volatile")
end

-- 使用点号语法
if obj.is_volatile("Status") then
    print("Status is volatile")
end
```

**说明：**
- 易变属性表示该属性的值可能会频繁变化
- 如果属性不存在，会抛出错误
- 支持冒号语法（`obj:is_volatile(prop_name)`）和点号语法（`obj.is_volatile(prop_name)`）

---

### pcall 方法

安全调用方法，返回成功状态和结果，不会抛出异常。

**语法：**
```lua
local obj = mdb_access.get_object(bus, path, interface)
local ok, result = obj:pcall(method_name, arg1, arg2, ...)
-- 或
local ok, result = obj.pcall(method_name, arg1, arg2, ...)
```

**参数：**
- `method_name` (string): 方法名称
- `arg1, arg2, ...`: 方法参数（可选）

**返回值：**
- 返回两个值：
  1. `ok` (boolean): 调用是否成功，`true` 表示成功，`false` 表示失败
  2. `result` (table | string): 
     - 如果成功，返回方法调用的结果（table，格式与普通方法调用相同）
     - 如果失败，返回错误信息字符串

**示例：**
```lua
local obj = mdb_access.get_object(bus, "/bmc/kepler/AccountService/Roles", "bmc.kepler.AccountService.Role")
local ctx = context.new("bmc.kepler.AccountService", "admin", "127.0.0.1")

-- 使用 pcall 安全调用方法
local ok, result = obj:pcall("Read", ctx, 1, 30)

if ok then
    -- 调用成功，result 是返回值字典
    if result.Id then
        print("Read Id:", result.Id)
    end
    if result.Data then
        for i, v in ipairs(result.Data) do
            print("Data[", i, "] =", v)
        end
    end
else
    -- 调用失败，result 是错误信息
    print("Method call failed:", result)
end
```

**说明：**
- `pcall` 方法不会抛出异常，所有错误都通过返回值传递
- 成功时返回 `(true, result_table)`，失败时返回 `(false, error_message)`
- 适用于需要错误处理但不希望使用 `pcall` 包装的场景
- 支持冒号语法（`obj:pcall(...)`）和点号语法（`obj.pcall(...)`）
- 返回值格式与普通方法调用相同，都是字典格式

**对比普通方法调用：**
```lua
-- 普通方法调用：失败时抛出异常
local result = obj:Read(ctx, 1, 30)  -- 如果失败，会抛出异常

-- pcall 方法调用：失败时返回错误信息
local ok, result = obj:pcall("Read", ctx, 1, 30)  -- 不会抛出异常
if not ok then
    print("Error:", result)  -- result 是错误信息字符串
end
```

---

## 类型转换

代理对象自动处理 D-Bus 类型到 Lua 类型的转换：

| D-Bus 类型 | Lua 类型 | 说明 |
|-----------|---------|------|
| `b` (boolean) | `boolean` | 布尔值 |
| `y` (byte) | `number` | 字节（0-255） |
| `n` (int16) | `number` | 16 位整数 |
| `q` (uint16) | `number` | 16 位无符号整数 |
| `i` (int32) | `number` | 32 位整数 |
| `u` (uint32) | `number` | 32 位无符号整数 |
| `x` (int64) | `number` | 64 位整数 |
| `t` (uint64) | `number` | 64 位无符号整数 |
| `d` (double) | `number` | 双精度浮点数 |
| `s` (string) | `string` | 字符串 |
| `o` (object path) | `string` | 对象路径 |
| `g` (signature) | `string` | 签名 |
| `ay` (byte array) | `table` | 字节数组（Lua 数组） |
| `a{ss}` (dict) | `table` | 字典 |
| `as` (string array) | `table` | 字符串数组（Lua 数组） |
| `(ss)` (struct) | `table` | 结构体（Lua 数组） |
| `v` (variant) | 根据内容 | variant 类型根据内容转换 |

---

## 错误处理

### 常见错误

1. **属性不存在**
   ```lua
   local value = obj.NonExistentProperty  -- 返回 nil
   ```

2. **属性不可写**
   ```lua
   obj.ReadOnlyProperty = value  -- 抛出错误
   ```

3. **方法不存在**
   ```lua
   obj:NonExistentMethod()  -- 抛出错误
   ```

4. **参数类型错误**
   ```lua
   obj:Method(123)  -- 如果期望 string，会抛出错误
   ```

5. **连接错误**
   ```lua
   local obj = mdb_access.get_object(bus, path, interface)  -- 如果连接失败，会抛出错误
   ```

### 错误处理示例

```lua
local ok, obj = pcall(function()
    return mdb_access.get_object(bus, path, interface)
end)

if not ok then
    print("Failed to get object:", obj)
    return
end

local ok, value = pcall(function()
    return obj.PropertyName
end)

if not ok then
    print("Failed to get property:", value)
    return
end

print("Property value:", value)
```

---

## 完整示例

```lua
local dbus = require("dbus")
local lmdb = require("lmdb")
local mdb_access = lmdb.mdb_access
local context = require("context")

-- 获取 D-Bus 连接
local bus = dbus.get_session_bus()

-- 获取代理对象（方式一：自动查找 service）
local obj = mdb_access.get_object(
    bus,
    "/bmc/kepler/AccountService/Roles",
    "bmc.kepler.AccountService.Role"
)

-- 获取代理对象（方式二：已知 service 名称）
local obj2 = mdb_access.get_object_with_service(
    bus,
    "org.example.Service",  -- 直接指定 service 名称
    "/org/example/Object",
    "org.example.Interface"
)

-- 读取属性
local id = obj.Id
local name = obj.Name
local enabled = obj.Enabled

print("Id:", id)
print("Name:", name)
print("Enabled:", enabled)

-- 设置属性
obj.Enabled = true

-- 检查属性是否易变
if obj:is_volatile("Status") then
    print("Status is volatile")
end

-- 调用方法（普通方式，失败时抛出异常）
local ctx = context.new("bmc.kepler.AccountService", "admin", "127.0.0.1")
local rsp = obj:Read(ctx, 1, 30)

-- 调用方法（安全方式，使用 pcall，不会抛出异常）
local ok, rsp2 = obj:pcall("Read", ctx, 1, 30)
if ok then
    -- 调用成功，使用 rsp2
    if rsp2.Id then
        print("Read Id:", rsp2.Id)
    end
else
    -- 调用失败，rsp2 是错误信息
    print("Read failed:", rsp2)
end

-- 访问方法返回值
if rsp.Id then
    print("Read Id:", rsp.Id)
end

if rsp.Data then
    print("Read Data length:", #rsp.Data)
    for i, v in ipairs(rsp.Data) do
        print("  Data[", i, "] =", v)
    end
end

-- 批量获取属性
local props, errs = mdb_access.get_properties(
    bus,
    "/bmc/kepler/AccountService/Roles",
    "bmc.kepler.AccountService.Role",
    {"Id", "Name", "Enabled", "Status"}
)

for key, value in pairs(props) do
    print(key, "=", value)
end

if next(errs) then
    print("Errors:")
    for key, err in pairs(errs) do
        print("  ", key, ":", err)
    end
end

-- 获取所有属性（使用模块函数）
local all_props = mdb_access.get_all(
    bus,
    "/bmc/kepler/AccountService/Roles",
    "bmc.kepler.AccountService.Role"
)

for key, value in pairs(all_props) do
    print(key, "=", value)
end

-- 使用代理对象的方法（更简洁，无需重复传入 bus、path、interface）
-- 使用代理对象的 get_all 方法
local all_props2 = obj:get_all()
for key, value in pairs(all_props2) do
    print(key, "=", value)
end

-- 使用代理对象的 get_properties 方法
local props2, errs2 = obj:get_properties({"Id", "Name", "Enabled"})
for key, value in pairs(props2) do
    print(key, "=", value)
end

if next(errs2) then
    print("Errors:")
    for key, err in pairs(errs2) do
        print("  ", key, ":", err)
    end
end

-- 获取子对象
local sub_objs = mdb_access.get_sub_objects(
    bus,
    "/bmc/kepler/AccountService/Roles",
    "bmc.kepler.AccountService.Role",
    1
)

for path, sub_obj in pairs(sub_objs) do
    print("Sub object path:", path)
    print("  Id:", sub_obj.Id)
    print("  Name:", sub_obj.Name)
end
```

---

## 注意事项

1. **对象生命周期**：代理对象由 `mdb_access` 管理，具有缓存机制，相同路径和接口的对象会被复用。

2. **线程安全**：代理对象不是线程安全的，应在单线程环境中使用。

3. **错误处理**：建议使用 `pcall` 包装可能出错的操作，以便更好地处理异常。

4. **类型转换**：D-Bus 类型到 Lua 类型的转换是自动的，但需要注意类型兼容性。

5. **方法返回值**：方法返回值是一个字典，键为输出参数名。如果输出参数没有名称，使用数字索引。

6. **数组类型**：数组类型（如 `ay`）会自动转换为 Lua 数组，可以直接使用 `ipairs` 遍历。

7. **易变属性**：易变属性的值可能会频繁变化，建议在需要时重新读取。

---

## 相关文档

- [mdb_service Lua API](./mdb_service.md)
- [mdb_privilege Lua API](./mdb_privilege.md)
- [variant Lua Utils API](../../api/variant_lua_utils.md)
