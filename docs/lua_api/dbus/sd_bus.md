# sd_bus Lua API 文档

本文档介绍了 sd_bus 模块的 Lua API，提供了高级的 D-Bus 客户端功能，包括对象注册、信号匹配和共享内存属性访问。

## 概述

`sd_bus` 模块是对底层 D-Bus 连接的封装，提供了简化的 API 用于：
- 创建和管理 D-Bus 连接
- 注册动态对象到 D-Bus 总线
- 添加和移除信号匹配规则
- 通过共享内存访问属性
- 访问底层阻塞或非阻塞连接对象

## 创建连接

### `sd_bus.open_user(start_now, is_blocking)`

创建一个新的用户会话总线连接。

**参数：**
- `start_now` (布尔，可选)：是否立即启动连接。默认为 `false`。
- `is_blocking` (布尔，可选)：是否使用阻塞模式。默认为 `false`（非阻塞模式）。

**返回值：**
- `bus` (userdata)：sd_bus 连接对象。

**示例：**
```lua
-- 创建非阻塞连接（默认）
local bus = sd_bus.open_user()

-- 创建阻塞连接并立即启动
local bus = sd_bus.open_user(true, true)

-- 创建非阻塞连接并立即启动
local bus = sd_bus.open_user(true, false)
```

## 连接对象方法

### `bus:request_name(name, flags)`

请求一个服务名称。

**参数：**
- `name` (字符串)：要请求的服务名称（例如："com.example.MyService"）。
- `flags` (数字，可选)：请求标志。默认为 `0`。

**返回值：**
- 成功时：`true`
- 失败时：`false, error_message`

**示例：**
```lua
local ok, err = bus:request_name("com.example.MyService")
if not ok then
    print("请求名称失败: " .. err)
end
```

### `bus:add_match(callback_id, member, interface, is_path_namespace, path, sender)`

添加一个信号匹配规则。

**参数：**
- `callback_id` (数字)：回调 ID，用于标识回调函数（通常由 Skynet 框架使用）。
- `member` (字符串)：信号成员名称。
- `interface` (字符串)：接口名称。
- `is_path_namespace` (布尔，可选)：如果提供，表示 `path` 参数是路径命名空间。
- `path` (字符串，可选)：对象路径或路径命名空间。
- `sender` (字符串，可选)：发送者服务名称。

**返回值：**
- 成功时：`true, rule_id`
- 失败时：`false, error_message`

**注意：** 此方法主要用于 Skynet 框架集成，回调通过 Skynet 消息传递。

**示例：**
```lua
-- 匹配特定接口和成员
local ok, rule_id = bus:add_match(123, "PropertiesChanged", "org.freedesktop.DBus.Properties")
if ok then
    print("匹配规则 ID: " .. rule_id)
end

-- 匹配特定路径的信号
local ok, rule_id = bus:add_match(123, "StateChanged", "com.example.Device", false, "/com/example/Device1")

-- 匹配路径命名空间
local ok, rule_id = bus:add_match(123, "Signal", "com.example.Interface", true, "/com/example/")

-- 匹配特定发送者
local ok, rule_id = bus:add_match(123, "Signal", "com.example.Interface", false, "/com/example/Object", "com.example.Service")
```

### `bus:remove_match(id)`

移除一个信号匹配规则。

**参数：**
- `id` (数字)：由 `add_match` 返回的规则 ID。

**返回值：**
- `true` - 成功移除

**异常：**
- 如果移除失败，会抛出 Lua 错误。

**示例：**
```lua
local ok, rule_id = bus:add_match(123, "PropertiesChanged", "org.freedesktop.DBus.Properties")
-- ... 使用规则 ...
bus:remove_match(rule_id)
```

### `bus:register_object(object)`

向 D-Bus 总线注册一个动态对象。

**参数：**
- `object` (userdata)：由 `ldbus.object.new()` 创建的对象实例。

**异常：**
- 如果注册失败，会抛出 Lua 错误。

**示例：**
```lua
local ldbus = require('ldbus')
local l_object = ldbus.object
local l_interface = ldbus.interface

-- 创建对象和接口
local obj = l_object.new("/com/example/MyObject")
local intf = l_interface.new("com.example.MyInterface")
intf:add_property("Status", "s", "active", false, 0)
obj:register_interface(intf)

-- 注册对象到总线
bus:register_object(obj)
```

### `bus:unregister_object(path)`

从 D-Bus 总线注销一个对象。

**参数：**
- `path` (字符串)：要注销的对象路径。

**异常：**
- 如果注销失败，会抛出 Lua 错误。

**示例：**
```lua
bus:unregister_object("/com/example/MyObject")
```

### `bus:call_shm_get_property(service, path, interface, property)`

通过共享内存获取属性值。

**参数：**
- `service` (字符串)：服务名称。
- `path` (字符串)：对象路径。
- `interface` (字符串)：接口名称。
- `property` (字符串)：属性名称。

**返回值：**
- 成功时：属性值（variant 类型）
- 失败时：`nil`

**异常：**
- 如果调用失败，会抛出 Lua 错误。

**注意：** 此方法需要服务已注册到共享内存 harbor。

**示例：**
```lua
local value = bus:call_shm_get_property("com.example.Service", "/com/example/Object", "com.example.Interface", "Status")
if value then
    print("属性值: " .. tostring(value))
else
    print("属性不存在或无法访问")
end
```

## 属性访问

### `bus.bus`

访问底层连接对象。根据创建时指定的模式，返回 `dbus.blocking` 或 `dbus.nonblock` 对象。

**返回值：**
- `dbus.blocking` 或 `dbus.nonblock` 对象，可用于调用底层连接方法。

**示例：**
```lua
local bus = sd_bus.open_user(false, true)  -- 阻塞模式
local blocking_conn = bus.bus
blocking_conn:start()
blocking_conn:request_name("com.example.Service")

local bus2 = sd_bus.open_user(false, false)  -- 非阻塞模式
local nonblock_conn = bus2.bus
nonblock_conn:start()
nonblock_conn:request_name("com.example.Service")
```

## 完整示例

### 示例 1：创建服务并注册对象

```lua
local sd_bus = require('sd_bus')
local ldbus = require('ldbus')
local l_object = ldbus.object
local l_interface = ldbus.interface

-- 创建 sd_bus 连接
local bus = sd_bus.open_user(true, false)  -- 立即启动，非阻塞模式

-- 请求服务名称
local ok, err = bus:request_name("com.example.MyService")
if not ok then
    error("请求名称失败: " .. err)
end

-- 创建动态对象和接口
local obj = l_object.new("/com/example/MyObject")
local intf = l_interface.new("com.example.MyInterface")

-- 添加属性
intf:add_property("Version", "s", "1.0.0", true, 0)
intf:add_property("Status", "s", "inactive", false, 0)

-- 注册接口到对象
obj:register_interface(intf)

-- 注册对象到总线
bus:register_object(obj)

print("服务已启动，对象已注册")
```

### 示例 2：使用共享内存获取属性

```lua
local sd_bus = require('sd_bus')
local bus = sd_bus.open_user()

-- 通过共享内存获取属性
local value = bus:call_shm_get_property(
    "com.example.Service",
    "/com/example/Object",
    "com.example.Interface",
    "Status"
)

if value then
    print("状态: " .. tostring(value))
end
```

### 示例 3：添加信号匹配（Skynet 集成）

```lua
local sd_bus = require('sd_bus')
local bus = sd_bus.open_user()

-- 添加匹配规则（callback_id 由 Skynet 框架提供）
local callback_id = 1001
local ok, rule_id = bus:add_match(
    callback_id,
    "PropertiesChanged",
    "org.freedesktop.DBus.Properties",
    false,
    "/com/example/Object"
)

if ok then
    print("匹配规则已添加，ID: " .. rule_id)
    
    -- 稍后移除规则
    -- bus:remove_match(rule_id)
end
```

### 示例 4：访问底层连接对象

```lua
local sd_bus = require('sd_bus')

-- 创建阻塞模式连接
local bus = sd_bus.open_user(false, true)
local blocking_conn = bus.bus

-- 使用底层连接的方法
blocking_conn:start()
blocking_conn:request_name("com.example.Service")

-- 创建非阻塞模式连接
local bus2 = sd_bus.open_user(false, false)
local nonblock_conn = bus2.bus

nonblock_conn:start()
nonblock_conn:request_name("com.example.Service")

-- 在事件循环中使用
while running do
    nonblock_conn:dispatch()
    nonblock_conn:flush()
    -- 处理其他事件...
end
```

## 与动态对象的关系

`sd_bus` 模块与 `ldbus.object` 和 `ldbus.interface` 模块紧密集成：

1. **创建对象**：使用 `ldbus.object.new()` 创建动态对象
2. **配置接口**：使用 `ldbus.interface.new()` 创建接口并添加属性/方法/信号
3. **注册对象**：使用 `bus:register_object()` 将对象注册到 D-Bus 总线
4. **访问属性**：可以通过 `bus:call_shm_get_property()` 访问已注册对象的共享内存属性值

## 错误处理

大多数方法在失败时会返回错误信息：

```lua
local ok, err = bus:request_name("com.example.Service")
if not ok then
    print("错误: " .. err)
    return
end
```

某些方法在失败时会抛出 Lua 错误：

```lua
-- 这些方法会抛出错误
bus:register_object(obj)      -- 如果注册失败
bus:unregister_object(path)   -- 如果注销失败
bus:remove_match(id)          -- 如果移除失败
bus:call_shm_get_property(...) -- 如果调用失败
```

## 注意事项

1. **生命周期管理**：sd_bus 对象使用引用计数管理，当 Lua 对象被垃圾回收时自动清理
2. **线程安全**：sd_bus 对象不是线程安全的，应在单线程中使用
3. **阻塞 vs 非阻塞**：选择正确的模式很重要
   - 阻塞模式：适合简单脚本，同步操作
   - 非阻塞模式：适合服务端，需要事件循环
4. **共享内存**：`call_shm_get_property` 需要服务已注册到共享内存 harbor
5. **信号匹配**：`add_match` 主要用于 Skynet 框架集成，回调通过 Skynet 消息传递

## 相关文档

- [object.md](object.md) - 动态对象 API
- [interface.md](interface.md) - 动态接口 API
- [blocking.md](blocking.md) - 阻塞模式连接 API
- [nonblock.md](nonblock.md) - 非阻塞模式连接 API
- [../../api/dbus/sd_bus.md](../../api/dbus/sd_bus.md) - C++ sd_bus API 文档
- [../../api/dbus/dynamic_object.md](../../api/dbus/dynamic_object.md) - C++ 动态对象 API 文档
