# DBus Connection Lua API

## 概述

内部连接对象（通过 `conn.conn` 访问）提供了底层的 DBus 连接操作。

## 访问方式

```lua
local dbus = ldbus.blocking.open_user()
local conn = dbus.conn  -- 获取内部连接对象
```

## 方法

### send_with_reply_and_block

发送消息并阻塞等待回复。

#### 语法

```lua
reply = conn:send_with_reply_and_block(message, timeout)
```

#### 参数

- `message` (userdata) - 消息对象
- `timeout` (number | nil, 可选) - 超时时间（毫秒）
  - 数字：指定的超时时间
  - `nil`：使用默认超时（60秒）
  - 不传参数：使用默认超时

#### 返回值

- `reply` (lightuserdata) - 回复消息的原始指针

#### 异常

调用失败时会抛出 Lua 错误。

#### 示例

```lua
local ldbus = require('ldbus')

local dbus = ldbus.blocking.open_user()
local conn = dbus.conn

local msg = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "GetId"
)

-- 三种调用方式

-- 1. 不指定超时（使用默认 60 秒）
local reply1 = conn:send_with_reply_and_block(msg)

-- 2. 明确传 nil（使用默认 60 秒）
local msg2 = ldbus.message.new_method_call(...)
local reply2 = conn:send_with_reply_and_block(msg2, nil)

-- 3. 指定超时时间（5 秒）
local msg3 = ldbus.message.new_method_call(...)
local reply3 = conn:send_with_reply_and_block(msg3, 5000)

dbus:close()
```

### close

关闭连接。

#### 语法

```lua
conn:close()
```

#### 返回值

无返回值，失败时抛出 Lua 错误。

#### 示例

```lua
local conn = dbus.conn
conn:close()
```

### flush

刷新连接缓冲区。

#### 语法

```lua
conn:flush()
```

#### 返回值

无返回值，失败时抛出 Lua 错误。

#### 示例

```lua
local conn = dbus.conn

-- 发送一些消息...

-- 确保消息已发送
conn:flush()
```

### dispatch

分发待处理的消息。

#### 语法

```lua
conn:dispatch()
```

#### 返回值

无返回值，失败时抛出 Lua 错误。

#### 示例

```lua
local conn = dbus.conn

-- 处理待处理的消息
conn:dispatch()
```

### request_name

请求 DBus 服务名称。

#### 语法

```lua
success, error = conn:request_name(name, flags)
```

#### 参数

- `name` (string) - 服务名称
- `flags` (number, 可选) - 请求标志，默认 0

#### 返回值

- `success` (boolean) - 是否成功
- `error` (userdata | nil) - 错误对象（失败时返回）

#### 标志位

- `0x01` - ALLOW_REPLACEMENT（允许被替换）
- `0x02` - REPLACE_EXISTING（替换现有所有者）
- `0x04` - DO_NOT_QUEUE（不排队等待）

#### 示例

```lua
local conn = dbus.conn

-- 基本用法
local ok, err = conn:request_name("com.example.MyService")
if not ok then
    print("失败: " .. tostring(err))
end

-- 带标志位
local ok2 = conn:request_name("com.example.Service2", 0x01)
```

### is_connected

检查连接状态。

#### 语法

```lua
connected = conn:is_connected()
```

#### 返回值

- `connected` (boolean) - 是否已连接

#### 示例

```lua
local conn = dbus.conn

if conn:is_connected() then
    print("连接正常")
else
    print("连接已断开")
end
```

### unique_name

获取连接的唯一名称。

#### 语法

```lua
name = conn:unique_name()
```

#### 返回值

- `name` (string | nil) - 唯一名称（如 `":1.123"`）

#### 示例

```lua
local conn = dbus.conn

local name = conn:unique_name()
if name then
    print("唯一名称: " .. name)
else
    print("尚未分配唯一名称")
end
```

### set_unique_name

设置连接的唯一名称。

#### 语法

```lua
conn:set_unique_name(name)
```

#### 参数

- `name` (string) - 唯一名称

#### 返回值

无返回值，失败时抛出 Lua 错误。

#### 示例

```lua
local conn = dbus.conn

conn:set_unique_name(":1.123")
```

### to_raw

获取原始 DBusConnection 指针。

#### 语法

```lua
raw_ptr = conn:to_raw(add_ref)
```

#### 参数

- `add_ref` (boolean, 可选) - 是否增加引用计数，默认 false

#### 返回值

- `raw_ptr` (lightuserdata) - DBusConnection 指针

#### 注意

仅在需要与底层 C 库交互时使用。

#### 示例

```lua
local conn = dbus.conn

-- 获取原始指针（不增加引用）
local ptr1 = conn:to_raw()

-- 获取原始指针（增加引用）
local ptr2 = conn:to_raw(true)
```

## 使用示例

### 完整的方法调用流程

```lua
#!/usr/bin/env lua

local ldbus = require('ldbus')

-- 创建连接
local dbus = ldbus.blocking.open_user()
local conn = dbus.conn

-- 检查连接状态
if not conn:is_connected() then
    print("连接失败")
    dbus:close()
    return
end

-- 获取唯一名称
local unique = conn:unique_name()
print("连接唯一名称: " .. (unique or "未知"))

-- 请求服务名称
local ok, err = conn:request_name("com.example.LuaTest")
if not ok then
    print("请求名称失败")
    if err then
        print("错误: " .. err.name)
    end
    dbus:close()
    return
end

-- 创建消息
local msg = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "GetId"
)

-- 发送消息
local reply = conn:send_with_reply_and_block(msg, 5000)
if reply then
    print("成功获取回复")
end

-- 刷新和清理
conn:flush()
dbus:close()
```

### 错误处理

```lua
local ldbus = require('ldbus')

local dbus = ldbus.blocking.open_user()
local conn = dbus.conn

-- 使用 pcall 捕获异常
local ok, result = pcall(function()
    local msg = ldbus.message.new_method_call(
        "com.nonexistent.Service",  -- 不存在的服务
        "/",
        "com.nonexistent.Interface",
        "Method"
    )
    return conn:send_with_reply_and_block(msg, 5000)
end)

if not ok then
    print("调用失败: " .. tostring(result))
end

dbus:close()
```

### 超时控制

```lua
local ldbus = require('ldbus')

local dbus = ldbus.blocking.open_user()
local conn = dbus.conn

local msg = ldbus.message.new_method_call(...)

-- 短超时（1秒）- 适合快速操作
local ok1, reply1 = pcall(function()
    return conn:send_with_reply_and_block(msg, 1000)
end)

-- 中等超时（5秒）- 适合一般操作
local msg2 = ldbus.message.new_method_call(...)
local ok2, reply2 = pcall(function()
    return conn:send_with_reply_and_block(msg2, 5000)
end)

-- 长超时（30秒）- 适合耗时操作
local msg3 = ldbus.message.new_method_call(...)
local ok3, reply3 = pcall(function()
    return conn:send_with_reply_and_block(msg3, 30000)
end)

dbus:close()
```

### 连接状态监控

```lua
local ldbus = require('ldbus')

local dbus = ldbus.blocking.open_user()
local conn = dbus.conn

-- 定期检查连接状态
local function monitor_connection()
    while true do
        if not conn:is_connected() then
            print("连接已断开")
            break
        end
        
        -- 刷新和分发
        conn:flush()
        conn:dispatch()
        
        -- 休眠
        os.execute("sleep 1")
    end
end

-- 启动监控
monitor_connection()

dbus:close()
```

## 注意事项

1. **返回值类型**：`send_with_reply_and_block` 返回 lightuserdata，需要进一步处理
2. **异常处理**：使用 `pcall` 捕获可能的异常
3. **超时设置**：合理设置超时避免长时间阻塞
4. **连接检查**：在发送消息前检查连接状态
5. **资源清理**：确保调用 `close()` 释放连接

## 相关文档

- [blocking.md](blocking.md) - 阻塞模式
- [nonblock.md](nonblock.md) - 非阻塞模式
- [message.md](message.md) - 消息对象
- [error.md](error.md) - 错误对象
- [../dbus.md](../dbus.md) - DBus Lua API 主文档
