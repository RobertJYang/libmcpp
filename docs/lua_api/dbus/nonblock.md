# DBus Nonblock Lua API

## 概述

`ldbus.nonblock` 提供非阻塞模式的 DBus 接口，适用于需要异步操作和事件驱动的应用场景。

## 模块函数

### nonblock.new

创建新的非阻塞连接。

#### 语法

```lua
conn = ldbus.nonblock.new(arg)
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
local conn1 = ldbus.nonblock.new()
local conn2 = ldbus.nonblock.new(nil)

-- 使用自定义地址
local conn3 = ldbus.nonblock.new("unix:path=/tmp/my-dbus")

-- 清理
conn1:close()
conn2:close()
conn3:close()
```

### nonblock.open_user

打开用户会话总线连接（便捷方法）。

#### 语法

```lua
conn = ldbus.nonblock.open_user([start_now])
```

#### 参数

- `start_now` (boolean, 可选) - 是否自动启动连接，默认 true

#### 返回值

- `conn` (userdata) - 连接对象

#### 示例

```lua
local ldbus = require('ldbus')

-- 打开并自动启动连接
local conn1 = ldbus.nonblock.open_user()
local conn2 = ldbus.nonblock.open_user(true)

-- 打开但不启动，稍后手动启动
local conn3 = ldbus.nonblock.open_user(false)
conn3:start()

-- 清理
conn1:close()
conn2:close()
conn3:close()
```

### nonblock.shutdown

关闭 DBus 库，释放全局资源。

#### 语法

```lua
success = ldbus.nonblock.shutdown()
```

#### 返回值

- `success` (boolean) - 是否成功

#### 注意

通常不需要手动调用，Lua 会在退出时自动清理。

## 连接对象方法

### conn:start

启动连接，开始消息分发。

#### 语法

```lua
success = conn:start()
```

#### 返回值

- `success` (boolean) - 是否成功启动

#### 示例

```lua
local conn = ldbus.nonblock.open_user(false)

-- 手动启动连接
local ok = conn:start()
if not ok then
    print("启动失败")
    conn:close()
    return
end

print("连接已启动")
```

### conn:request_name

请求 DBus 服务名称。

#### 语法

```lua
success, error = conn:request_name(name, flags)
```

#### 参数

- `name` (string) - 服务名称
- `flags` (number, 可选) - 标志位，默认 0

#### 返回值

- `success` (boolean) - 是否成功
- `error` (userdata | nil) - 错误对象（失败时）

#### 示例

```lua
local conn = ldbus.nonblock.open_user()

local ok, err = conn:request_name("com.example.AsyncService")
if not ok then
    print("请求名称失败")
end
```

### conn:close

关闭连接。

#### 语法

```lua
success = conn:close()
```

#### 返回值

- `success` (boolean) - 是否成功

### conn:flush

刷新连接缓冲区。

#### 语法

```lua
success = conn:flush()
```

#### 返回值

- `success` (boolean) - 是否成功

### conn:dispatch

分发待处理的消息。

#### 语法

```lua
success = conn:dispatch()
```

#### 返回值

- `success` (boolean) - 是否成功

### conn:async_call

异步调用 DBus 方法（使用默认超时时间，10分钟）。

#### 语法

```lua
conn:async_call(callback, service_name, path, interface, method, [signature], [args...])
```

#### 参数

- `callback` (function) - 回调函数，接收 `(ok, ...)` 参数：
  - `ok` (boolean) - 是否成功
  - 如果成功：`ok=true`，后面是方法返回的多个值
  - 如果失败：`ok=false`，后面是错误信息字符串
- `service_name` (string) - 服务名称
- `path` (string) - 对象路径
- `interface` (string) - 接口名称
- `method` (string) - 方法名称
- `signature` (string, 可选) - 参数签名，默认为空字符串
- `args...` (可选) - 方法参数，可以是多个值

#### 返回值

- 无返回值（异步调用）

#### 示例

```lua
local conn = ldbus.nonblock.open_user()

-- 异步调用方法
conn:async_call(function(ok, ...)
    if ok then
        local result1, result2 = ...
        print("成功:", result1, result2)
    else
        local error = ...
        print("失败:", error)
    end
end, "org.example.Service", "/org/example/Object",
    "org.example.Interface", "GetValue", "", {})

-- 需要定期调用 dispatch 来处理回调
conn:dispatch()
```

### conn:async_timeout_call

带超时时间的异步调用 DBus 方法。

#### 语法

```lua
conn:async_timeout_call(timeout_ms, callback, service_name, path, interface, method, [signature], [args...])
```

#### 参数

- `timeout_ms` (number) - 超时时间（毫秒）
- `callback` (function) - 回调函数，接收 `(ok, ...)` 参数：
  - `ok` (boolean) - 是否成功
  - 如果成功：`ok=true`，后面是方法返回的多个值
  - 如果失败：`ok=false`，后面是错误信息字符串
- `service_name` (string) - 服务名称
- `path` (string) - 对象路径
- `interface` (string) - 接口名称
- `method` (string) - 方法名称
- `signature` (string, 可选) - 参数签名，默认为空字符串
- `args...` (可选) - 方法参数，可以是多个值

#### 返回值

- 无返回值（异步调用）

#### 示例

```lua
local conn = ldbus.nonblock.open_user()

-- 使用 5 秒超时
conn:async_timeout_call(5000, function(ok, ...)
    if ok then
        local result = ...
        print("成功:", result)
    else
        local error = ...
        print("失败:", error)
    end
end, "org.example.Service", "/org/example/Object",
    "org.example.Interface", "Method", "s", {"param"})

conn:dispatch()
```

### conn:async_shm_timeout_call

通过共享内存异步调用 DBus 方法。

#### 语法

```lua
success = conn:async_shm_timeout_call(timeout_ms, callback, service_name, path, interface, method, [signature], [args...])
```

#### 参数

- `timeout_ms` (number) - 超时时间（毫秒）
- `callback` (function) - 回调函数，接收 `(ok, ...)` 参数：
  - `ok` (boolean) - 方法调用是否成功
  - 如果成功：`ok=true`，后面是方法返回的多个值
  - 如果失败：`ok=false`，后面是错误信息字符串
- `service_name` (string) - 服务名称
- `path` (string) - 对象路径
- `interface` (string) - 接口名称
- `method` (string) - 方法名称
- `signature` (string, 可选) - 参数签名，默认为空字符串
- `args...` (可选) - 方法参数，可以是多个值

#### 返回值

- `success` (boolean) - 共享内存传输是否成功
  - `true` - 共享内存传输成功（即使方法调用失败，也会调用回调）
  - `false` - 共享内存传输失败（不会调用回调）

#### 说明

- 仅通过共享内存进行调用，不会回退到普通 DBus 调用
- 如果共享内存不可用或传输失败，返回 `false` 且不会调用回调
- 如果共享内存传输成功但方法调用失败，仍会调用回调并返回 `true`
- 适合需要高性能调用的场景

#### 示例

```lua
local conn = ldbus.nonblock.open_user()

-- 尝试共享内存调用
local shm_success = conn:async_shm_timeout_call(30000, function(ok, ...)
    if ok then
        local result = ...
        print("方法调用成功:", result)
    else
        local error = ...
        print("方法调用失败:", error)
    end
end, "org.example.Service", "/org/example/Object",
    "org.example.Interface", "Method", "", {})

if not shm_success then
    -- 共享内存调用失败，使用普通异步调用
    conn:async_timeout_call(30000, function(ok, ...)
        -- 处理结果
    end, "org.example.Service", "/org/example/Object",
        "org.example.Interface", "Method", "", {})
end

conn:dispatch()
```

## 连接对象属性

### conn.conn

获取内部连接对象，用于底层 DBus 操作。

#### 类型

- (userdata) 内部连接对象

#### 方法

内部连接对象支持与阻塞模式相同的方法。详见 [connection.md](connection.md)

## 使用示例

### 基本非阻塞服务

```lua
#!/usr/bin/env lua

local ldbus = require('ldbus')

-- 创建非阻塞连接（自动启动）
local dbus = ldbus.nonblock.open_user()

-- 请求服务名称
local ok, err = dbus:request_name("com.example.AsyncService", 0)
if not ok then
    print("无法请求服务名称")
    dbus:close()
    os.exit(1)
end

print("服务已启动: com.example.AsyncService")

-- 在应用的事件循环中定期调用
function on_idle()
    -- 分发 DBus 消息
    dbus:dispatch()
    
    -- 刷新缓冲区
    dbus:flush()
end

-- 模拟事件循环
for i = 1, 100 do
    on_idle()
    
    -- 模拟其他工作
    os.execute("sleep 0.1")
end

-- 清理
dbus:close()
```

### 与定时器集成

```lua
local ldbus = require('ldbus')
local socket = require('socket')  -- luasocket

local dbus = ldbus.nonblock.open_user()

local ok = dbus:request_name("com.example.TimerService")
if not ok then
    print("服务名称请求失败")
    dbus:close()
    return
end

-- 事件循环
local running = true
local last_time = socket.gettime()

while running do
    local current_time = socket.gettime()
    
    -- 每 100ms 分发一次消息
    if current_time - last_time >= 0.1 then
        dbus:dispatch()
        dbus:flush()
        last_time = current_time
    end
    
    -- 处理其他事件...
    
    -- 短暂休眠避免 CPU 100%
    socket.sleep(0.01)
end

dbus:close()
```

### 手动启动控制

```lua
local ldbus = require('ldbus')

-- 创建连接但不自动启动
local dbus = ldbus.nonblock.open_user(false)

-- 请求服务名称
local ok, err = dbus:request_name("com.example.Service")
if not ok then
    print("请求名称失败")
    dbus:close()
    return
end

-- 准备就绪后再启动
print("准备启动服务...")

-- 启动连接
local started = dbus:start()
if not started then
    print("启动失败")
    dbus:close()
    return
end

print("服务已启动")

-- 运行服务...

dbus:close()
```

## 阻塞模式对比

| 特性 | blocking | nonblock |
|------|----------|----------|
| 调用方式 | 同步阻塞 | 异步非阻塞 |
| run_once | ✓ | ✗ |
| start | ✓ | ✓ |
| 自动启动 | ✗ | ✓ (可配置) |
| 适用场景 | 脚本、工具 | 守护进程、服务 |
| 事件循环 | 内置简单循环 | 需集成到应用循环 |

## 注意事项

1. **非阻塞特性**：需要定期调用 `dispatch()` 处理消息
2. **事件循环集成**：应与应用的主事件循环集成
3. **启动控制**：可以选择自动启动或手动启动
4. **资源管理**：确保在退出时调用 `close()`
5. **性能考虑**：合理设置 dispatch 频率，避免过度占用 CPU

## 相关文档

- [blocking.md](blocking.md) - 阻塞模式
- [message.md](message.md) - 消息对象
- [connection.md](connection.md) - 连接对象详细API
- [error.md](error.md) - 错误对象
- [../dbus.md](../dbus.md) - DBus Lua API 主文档
