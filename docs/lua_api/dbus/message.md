# DBus Message Lua API

## 概述

`ldbus.message` 提供了创建 DBus 消息的功能。

## 模块函数

### message.new_method_call

创建方法调用消息。

#### 语法

```lua
msg = ldbus.message.new_method_call(destination, path, interface, method)
```

#### 参数

- `destination` (string) - 目标服务名称（如 `"org.freedesktop.DBus"`）
- `path` (string) - 对象路径（如 `"/org/freedesktop/DBus"`）
- `interface` (string) - 接口名称（如 `"org.freedesktop.DBus"`）
- `method` (string) - 方法名称（如 `"ListNames"`）

#### 返回值

- `msg` (userdata) - 消息对象

#### 示例

```lua
local ldbus = require('ldbus')

-- 创建方法调用消息
local msg = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "GetId"
)

-- 使用消息...
```

## 消息对象

消息对象是 userdata 类型，由 `message.new_method_call()` 创建。

### 生命周期

消息对象由 Lua GC 自动管理，无需手动释放。

#### 示例

```lua
local msg = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/",
    "org.freedesktop.DBus",
    "ListNames"
)

-- Lua 会在不再使用时自动释放消息
```

### 字符串表示

```lua
local msg = ldbus.message.new_method_call(...)
print(msg)  -- 输出: dbus.message
```

## 使用示例

### 查询服务列表

```lua
#!/usr/bin/env lua

local ldbus = require('ldbus')

-- 创建连接
local dbus = ldbus.blocking.open_user()

-- 创建消息：调用 ListNames 方法
local msg = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "ListNames"
)

-- 发送消息
local conn = dbus.conn
local reply = conn:send_with_reply_and_block(msg, 5000)

if reply then
    print("成功获取服务列表")
    -- 注意：当前版本返回 lightuserdata
    -- 需要进一步解析才能使用
end

dbus:close()
```

### 调用自定义服务方法

```lua
local ldbus = require('ldbus')

local dbus = ldbus.blocking.open_user()

-- 创建调用自定义服务的消息
local msg = ldbus.message.new_method_call(
    "com.example.MyService",      -- 自定义服务
    "/com/example/Object",        -- 自定义对象
    "com.example.Interface",      -- 自定义接口
    "DoSomething"                 -- 自定义方法
)

local conn = dbus.conn
local ok, reply = pcall(function()
    return conn:send_with_reply_and_block(msg, 5000)
end)

if not ok then
    print("调用失败: " .. tostring(reply))
else
    print("调用成功")
end

dbus:close()
```

### 批量创建消息

```lua
local ldbus = require('ldbus')

-- 消息工厂函数
local function create_dbus_message(service, method)
    return ldbus.message.new_method_call(
        service,
        "/" .. service:gsub("%.", "/"),  -- 根据服务名生成路径
        service,
        method
    )
end

-- 批量创建
local messages = {
    create_dbus_message("org.freedesktop.DBus", "ListNames"),
    create_dbus_message("org.freedesktop.DBus", "GetId"),
    create_dbus_message("org.freedesktop.DBus", "GetNameOwner")
}

-- 使用消息...
```

### 消息复用（注意事项）

```lua
local ldbus = require('ldbus')

local dbus = ldbus.blocking.open_user()
local conn = dbus.conn

-- 创建消息
local msg = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "GetId"
)

-- 第一次使用
local reply1 = conn:send_with_reply_and_block(msg, 5000)

-- 注意：消息在发送后可能已被移动或释放
-- 如果需要再次发送相同的消息，应该重新创建
local msg2 = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "GetId"
)
local reply2 = conn:send_with_reply_and_block(msg2, 5000)

dbus:close()
```

## 常见调用模式

### 标准 DBus 接口调用

```lua
-- 获取 DBus 总线 ID
local msg_get_id = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "GetId"
)

-- 列出所有服务名称
local msg_list_names = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "ListNames"
)

-- 获取服务的所有者
local msg_get_owner = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "GetNameOwner"
)
```

### Properties 接口调用

```lua
-- 获取属性
local msg_get = ldbus.message.new_method_call(
    "com.example.Service",
    "/com/example/Object",
    "org.freedesktop.DBus.Properties",
    "Get"
)

-- 设置属性
local msg_set = ldbus.message.new_method_call(
    "com.example.Service",
    "/com/example/Object",
    "org.freedesktop.DBus.Properties",
    "Set"
)

-- 获取所有属性
local msg_get_all = ldbus.message.new_method_call(
    "com.example.Service",
    "/com/example/Object",
    "org.freedesktop.DBus.Properties",
    "GetAll"
)
```

### 自定义服务调用

```lua
-- 调用自定义方法
local msg = ldbus.message.new_method_call(
    "com.example.Calculator",     -- 计算器服务
    "/com/example/Calculator",    -- 计算器对象
    "com.example.Calculator",     -- 计算器接口
    "Add"                         -- 加法方法
)

-- 注意：当前版本不支持在 Lua 层添加参数
-- 参数需要在 C++ 层或通过其他方式处理
```

## 注意事项

1. **参数添加**：当前版本的 Lua 绑定不支持在 Lua 层添加消息参数
2. **消息生命周期**：消息对象由 Lua GC 管理
3. **消息复用**：发送后的消息不应再次使用，需重新创建
4. **返回值类型**：当前返回 lightuserdata，需要进一步封装才能解析
5. **内存管理**：消息对象的内存由底层 C++ 自动管理

## 限制和改进方向

### 当前限制

1. 不支持在 Lua 层添加消息参数
2. 不支持解析回复消息内容
3. 不支持创建信号消息
4. 不支持创建错误消息

### 解决方案

对于需要传递参数和解析结果的场景，建议：
- 在 C++ 层封装完整的方法调用
- 使用 C++ 实现业务逻辑，Lua 仅用于配置和控制
- 扩展 Lua 绑定，添加参数序列化支持

## 相关文档

- [blocking.md](blocking.md) - 阻塞模式连接
- [nonblock.md](nonblock.md) - 非阻塞模式连接
- [connection.md](connection.md) - 连接对象API
- [../dbus.md](../dbus.md) - DBus Lua API 主文档
