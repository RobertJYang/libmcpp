# DBus Blocking Lua API

## 概述

`ldbus.blocking` 提供阻塞模式的 DBus 接口，适用于同步调用场景。所有操作都会阻塞直到完成或超时。

## 模块函数

### blocking.new

创建新的阻塞连接。

#### 语法

```lua
conn = ldbus.blocking.new(arg)
```

#### 参数

- `arg` (nil | string | userdata, 可选) - 连接参数：
  - `nil` - 连接到会话总线
  - `string` - DBus 地址字符串（如 `"unix:path=/tmp/dbus"`）
  - `userdata` - 已有的 DBusConnection 指针

#### 返回值

- `conn` (userdata) - 连接对象

#### 示例

```lua
local ldbus = require('ldbus')

-- 连接到会话总线
local conn1 = ldbus.blocking.new()
local conn2 = ldbus.blocking.new(nil)

-- 使用自定义地址
local conn3 = ldbus.blocking.new("unix:path=/tmp/my-dbus")

-- 清理
conn1:close()
conn2:close()
conn3:close()
```

### blocking.open_user

打开用户会话总线连接（便捷方法）。

#### 语法

```lua
conn = ldbus.blocking.open_user()
```

#### 返回值

- `conn` (userdata) - 连接对象

#### 示例

```lua
local ldbus = require('ldbus')

-- 打开用户会话总线
local conn = ldbus.blocking.open_user()

-- 使用连接...

conn:close()
```

### blocking.shutdown

关闭 DBus 库，释放全局资源。

#### 语法

```lua
success = ldbus.blocking.shutdown()
```

#### 返回值

- `success` (boolean) - 是否成功

#### 注意

通常不需要手动调用，Lua 会在退出时自动清理。

#### 示例

```lua
local ldbus = require('ldbus')

-- 在程序结束时
ldbus.blocking.shutdown()
```

## 连接对象方法

### conn:request_name

请求 DBus 服务名称。

#### 语法

```lua
success, error = conn:request_name(name, flags)
```

#### 参数

- `name` (string) - 服务名称（如 `"com.example.MyService"`）
- `flags` (number, 可选) - 标志位，默认 0
  - `0x01` - DBUS_NAME_FLAG_ALLOW_REPLACEMENT
  - `0x02` - DBUS_NAME_FLAG_REPLACE_EXISTING
  - `0x04` - DBUS_NAME_FLAG_DO_NOT_QUEUE

#### 返回值

- `success` (boolean) - 是否成功
- `error` (userdata | nil) - 错误对象（失败时）

#### 示例

```lua
local conn = ldbus.blocking.open_user()

-- 请求服务名称
local ok, err = conn:request_name("com.example.TestService")
if not ok then
    if err then
        print("失败: " .. err.name .. " - " .. err.message)
    end
    conn:close()
    return
end

print("成功注册服务名称")

-- 允许被替换的服务
local ok2 = conn:request_name("com.example.TestService2", 0x01)

conn:close()
```

### conn:close

关闭连接。

#### 语法

```lua
success = conn:close()
```

#### 返回值

- `success` (boolean) - 是否成功

#### 示例

```lua
local conn = ldbus.blocking.open_user()

-- 使用连接...

local ok = conn:close()
if not ok then
    print("关闭连接失败")
end
```

### conn:flush

刷新连接缓冲区，将待发送的消息立即发送。

#### 语法

```lua
success = conn:flush()
```

#### 返回值

- `success` (boolean) - 是否成功

#### 示例

```lua
local conn = ldbus.blocking.open_user()

-- 发送一些消息...

-- 确保消息已发送
conn:flush()

conn:close()
```

### conn:dispatch

分发待处理的消息。

#### 语法

```lua
success = conn:dispatch()
```

#### 返回值

- `success` (boolean) - 是否成功

#### 示例

```lua
local conn = ldbus.blocking.open_user()

-- 定期分发消息
conn:dispatch()

conn:close()
```

### conn:run_once

运行一次事件循环，等待并处理一条消息。

#### 语法

```lua
has_message = conn:run_once(timeout)
```

#### 参数

- `timeout` (number) - 超时时间（毫秒）

#### 返回值

- `has_message` (boolean) - 是否处理了消息

#### 示例

```lua
local conn = ldbus.blocking.open_user()

-- 启动连接
conn:start()

-- 事件循环
while true do
    -- 等待最多 100ms
    local got_msg = conn:run_once(100)
    
    if got_msg then
        print("处理了一条消息")
    end
    
    -- 检查退出条件
    if should_exit then
        break
    end
end

conn:close()
```

### conn:start

启动连接，开始消息分发。

#### 语法

```lua
success = conn:start()
```

#### 返回值

- `success` (boolean) - 是否成功

#### 示例

```lua
local conn = ldbus.blocking.open_user()

-- 启动连接
if not conn:start() then
    print("启动连接失败")
    conn:close()
    return
end

-- 现在可以接收消息了

conn:close()
```

### conn:call

同步调用 DBus 方法（使用默认超时时间，10分钟）。

#### 语法

```lua
result1, result2, ... = conn:call(service_name, path, interface, method, [signature], [args...])
```

#### 参数

- `service_name` (string) - 服务名称
- `path` (string) - 对象路径
- `interface` (string) - 接口名称
- `method` (string) - 方法名称
- `signature` (string, 可选) - 参数签名，默认为空字符串
- `args...` (可选) - 方法参数，可以是多个值

#### 返回值

- `result1, result2, ...` - 方法返回的多个值（variants）
- 如果方法没有返回值，不返回任何值
- 如果方法返回单个值，返回该值
- 如果方法返回多个值，返回所有值

#### 异常

- 调用失败时抛出 Lua 错误

#### 示例

```lua
local conn = ldbus.blocking.open_user()

-- 调用无返回值的方法
conn:call("org.example.Service", "/org/example/Object",
          "org.example.Interface", "SetValue", "i", {42})

-- 调用返回单个值的方法
local value = conn:call("org.example.Service", "/org/example/Object",
                        "org.example.Interface", "GetValue", "", {})

-- 调用返回多个值的方法
local result1, result2 = conn:call("org.example.Service", "/org/example/Object",
                                   "org.example.Interface", "GetValues", "", {})

conn:close()
```

### conn:timeout_call

带超时时间的同步调用 DBus 方法。

#### 语法

```lua
result1, result2, ... = conn:timeout_call(timeout_ms, service_name, path, interface, method, [signature], [args...])
```

#### 参数

- `timeout_ms` (number) - 超时时间（毫秒）
- `service_name` (string) - 服务名称
- `path` (string) - 对象路径
- `interface` (string) - 接口名称
- `method` (string) - 方法名称
- `signature` (string, 可选) - 参数签名，默认为空字符串
- `args...` (可选) - 方法参数，可以是多个值

#### 返回值

- `result1, result2, ...` - 方法返回的多个值（variants）
- 如果方法没有返回值，不返回任何值
- 如果方法返回单个值，返回该值
- 如果方法返回多个值，返回所有值

#### 异常

- 调用失败或超时时抛出 Lua 错误

#### 示例

```lua
local conn = ldbus.blocking.open_user()

-- 使用 5 秒超时
local result = conn:timeout_call(5000, "org.example.Service", "/org/example/Object",
                                 "org.example.Interface", "GetValue", "", {})

-- 使用 3 秒超时调用可能耗时的方法
local ok, err = pcall(function()
    conn:timeout_call(3000, "org.example.Service", "/org/example/Object",
                      "org.example.Interface", "LongRunningMethod", "", {})
end)

if not ok then
    print("调用超时或失败:", err)
end

conn:close()
```

## 连接对象属性

### conn.conn

获取内部连接对象，用于底层操作。

#### 类型

- (userdata) 内部连接对象

#### 方法

内部连接对象支持以下方法：

- `send_with_reply_and_block(message, timeout)` - 发送消息并阻塞等待回复
- `close()` - 关闭连接
- `flush()` - 刷新缓冲区
- `dispatch()` - 分发消息
- `request_name(name, flags)` - 请求服务名称
- `is_connected()` - 检查连接状态
- `unique_name()` - 获取唯一名称
- `set_unique_name(name)` - 设置唯一名称
- `to_raw(add_ref)` - 获取原始指针

#### 示例

```lua
local conn = ldbus.blocking.open_user()

-- 获取内部连接
local inner_conn = conn.conn

-- 创建消息
local msg = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "GetId"
)

-- 发送消息
local reply = inner_conn:send_with_reply_and_block(msg, 5000)

conn:close()
```

## 使用示例

### 完整的方法调用

```lua
#!/usr/bin/env lua

local ldbus = require('ldbus')

-- 创建连接
local dbus = ldbus.blocking.open_user()

-- 创建方法调用消息
local msg = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "ListNames"
)

-- 发送并等待回复
local conn = dbus.conn
local reply = conn:send_with_reply_and_block(msg, 5000)

if reply then
    print("成功获取回复")
else
    print("未收到回复")
end

-- 清理
dbus:close()
```

### 服务守护进程

```lua
#!/usr/bin/env lua

local ldbus = require('ldbus')

-- 创建连接
local dbus = ldbus.blocking.open_user()

-- 请求服务名称
local ok, err = dbus:request_name("com.example.MyDaemon")
if not ok then
    print("无法请求服务名称")
    if err then
        print("错误: " .. err.name)
    end
    dbus:close()
    os.exit(1)
end

print("服务已启动: com.example.MyDaemon")

-- 启动连接
dbus:start()

-- 事件循环
local running = true

-- 信号处理（简化版）
local function signal_handler()
    running = false
end

-- 主循环
while running do
    -- 处理一条消息，超时 100ms
    local has_msg = dbus:run_once(100)
    
    -- 可以在这里处理其他任务
end

print("服务正在退出...")
dbus:close()
```

### 错误处理

```lua
local ldbus = require('ldbus')

-- 使用 pcall 捕获异常
local ok, conn = pcall(function()
    return ldbus.blocking.open_user()
end)

if not ok then
    print("无法打开连接: " .. tostring(conn))
    return
end

-- 请求名称时检查错误
local success, error = conn:request_name("com.example.Test")
if not success then
    if error then
        print("请求名称失败:")
        print("  名称: " .. (error.name or "unknown"))
        print("  消息: " .. (error.message or "no message"))
        
        -- 检查错误是否已设置
        if error:is_set() then
            error:ensure_ok()  -- 这会抛出异常
        end
    end
end

conn:close()
```

## 注意事项

1. **阻塞操作**：所有操作都是同步的，会阻塞当前线程
2. **超时设置**：合理设置超时避免无限等待
3. **资源清理**：确保调用 `close()` 释放资源
4. **错误检查**：始终检查返回值
5. **单线程**：连接对象不是线程安全的

## 相关文档

- [nonblock.md](nonblock.md) - 非阻塞模式
- [message.md](message.md) - 消息对象
- [connection.md](connection.md) - 连接对象详细API
- [error.md](error.md) - 错误对象
- [../dbus.md](../dbus.md) - DBus Lua API 主文档
