# DBus Lua API 文档

## 概述

`ldbus` 是 DBus 的 Lua 绑定库，提供了在 Lua 中使用 DBus 进行进程间通信的能力。支持阻塞和非阻塞两种模式。

## 模块加载

```lua
local ldbus = require('ldbus')
```

## 主要特性

- **阻塞模式**：简单的同步 DBus 调用，适合脚本和简单应用
- **非阻塞模式**：异步 DBus 调用，适合需要事件循环的应用
- **消息创建**：创建方法调用、信号等消息
- **连接管理**：连接系统总线、会话总线
- **错误处理**：完善的错误对象和错误检查

## 模块结构

```
ldbus
├── blocking      -- 阻塞模式 DBus 接口
├── nonblock      -- 非阻塞模式 DBus 接口
├── message       -- 消息创建工具
└── setenv()      -- 环境变量设置工具
```

## 快速开始

### 阻塞模式示例

```lua
local ldbus = require('ldbus')

-- 创建阻塞连接
local conn = ldbus.blocking.open_user()

-- 创建方法调用消息
local msg = ldbus.message.new_method_call(
    "org.freedesktop.DBus",       -- 目标服务
    "/org/freedesktop/DBus",      -- 对象路径
    "org.freedesktop.DBus",       -- 接口名称
    "ListNames"                   -- 方法名称
)

-- 获取内部连接对象并发送消息
local inner_conn = conn.conn
local reply = inner_conn:send_with_reply_and_block(msg, 5000)

-- 处理回复...

-- 关闭连接
conn:close()
```

### 非阻塞模式示例

```lua
local ldbus = require('ldbus')

-- 创建非阻塞连接
local conn = ldbus.nonblock.open_user()

-- 请求服务名称
local success, error = conn:request_name("com.example.MyService")
if not success then
    print("请求名称失败: " .. tostring(error))
    return
end

-- 刷新连接
conn:flush()

-- 关闭连接
conn:close()
```

## 模块 API

### ldbus.blocking

阻塞模式 DBus 接口，详见 [blocking.md](dbus/blocking.md)

#### 函数

- `blocking.new(arg)` - 创建阻塞连接
- `blocking.open_user()` - 打开用户会话总线
- `blocking.shutdown()` - 关闭 DBus 库

#### 连接对象方法

- `conn:request_name(name, flags)` - 请求服务名称
- `conn:close()` - 关闭连接
- `conn:flush()` - 刷新连接
- `conn:dispatch()` - 分发消息
- `conn:run_once(timeout)` - 运行一次事件循环
- `conn:start()` - 启动连接

#### 属性

- `conn.conn` - 获取内部连接对象

### ldbus.nonblock

非阻塞模式 DBus 接口，详见 [nonblock.md](dbus/nonblock.md)

#### 函数

- `nonblock.new(arg)` - 创建非阻塞连接
- `nonblock.open_user([start_now])` - 打开用户会话总线
- `nonblock.shutdown()` - 关闭 DBus 库

#### 连接对象方法

- `conn:start()` - 启动连接
- `conn:request_name(name, flags)` - 请求服务名称
- `conn:close()` - 关闭连接
- `conn:flush()` - 刷新连接
- `conn:dispatch()` - 分发消息

#### 属性

- `conn.conn` - 获取内部连接对象

### ldbus.message

消息创建工具，详见 [message.md](dbus/message.md)

#### 函数

- `message.new_method_call(destination, path, interface, method)` - 创建方法调用消息

### ldbus.setenv

设置环境变量的工具函数。

#### 语法

```lua
success, error = ldbus.setenv(name, value, overwrite)
```

#### 参数

- `name` (string) - 环境变量名称
- `value` (string) - 环境变量值
- `overwrite` (boolean, 可选) - 是否覆盖现有值，默认 false

#### 返回值

- `success` (boolean) - 是否成功
- `error` (string, 可选) - 错误消息

#### 示例

```lua
local ldbus = require('ldbus')

-- 设置环境变量
local ok = ldbus.setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/tmp/dbus-test")
if not ok then
    print("设置环境变量失败")
end

-- 覆盖已存在的环境变量
ldbus.setenv("PATH", "/usr/local/bin", true)
```

## 连接对象 (Connection)

内部连接对象通过 `conn.conn` 属性访问，提供底层 DBus 操作。

详见 [connection.md](dbus/connection.md)

### 方法

#### send_with_reply_and_block

发送消息并阻塞等待回复。

```lua
reply = conn:send_with_reply_and_block(message, timeout)
```

**参数：**
- `message` - 消息对象
- `timeout` (number, 可选) - 超时时间（毫秒），nil 表示使用默认超时

**返回值：**
- `reply` - 回复消息（lightuserdata）

**示例：**

```lua
local msg = ldbus.message.new_method_call(...)
local reply = conn:send_with_reply_and_block(msg)
-- 不指定超时
local reply2 = conn:send_with_reply_and_block(msg, nil)
-- 指定 5 秒超时
local reply3 = conn:send_with_reply_and_block(msg, 5000)
```

#### 其他方法

- `conn:close()` - 关闭连接
- `conn:flush()` - 刷新缓冲区
- `conn:dispatch()` - 分发消息
- `conn:request_name(name, flags)` - 请求服务名称
- `conn:is_connected()` - 检查连接状态
- `conn:unique_name()` - 获取唯一名称
- `conn:set_unique_name(name)` - 设置唯一名称
- `conn:to_raw(add_ref)` - 获取原始 DBus 连接指针

## 错误对象 (Error)

详见 [error.md](dbus/error.md)

### 方法

- `err:is_set()` - 检查错误是否已设置
- `err:ensure_ok()` - 如果有错误则抛出异常

### 属性

- `err.name` - 错误名称
- `err.message` - 错误消息

### 示例

```lua
local success, error = conn:request_name("com.example.Test")
if not success and error then
    print("错误: " .. error.name .. " - " .. error.message)
end
```

## 使用模式

### 模式 1：简单脚本（阻塞模式）

适用于一次性脚本、简单工具。

```lua
local ldbus = require('ldbus')

-- 打开连接
local dbus = ldbus.blocking.open_user()

-- 创建并发送消息
local msg = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "GetId"
)

local conn = dbus.conn
local reply = conn:send_with_reply_and_block(msg, 5000)

-- 处理回复...

dbus:close()
```

### 模式 2：服务守护进程（阻塞模式 + 事件循环）

适用于需要监听信号和消息的服务。

```lua
local ldbus = require('ldbus')

local dbus = ldbus.blocking.open_user()

-- 请求服务名称
local ok, err = dbus:request_name("com.example.MyService")
if not ok then
    error("无法请求服务名称: " .. tostring(err))
end

-- 启动连接
dbus:start()

-- 事件循环
while true do
    -- 运行一次，超时 100ms
    local has_message = dbus:run_once(100)
    
    -- 处理其他任务...
    
    -- 检查退出条件
    if should_exit then
        break
    end
end

dbus:close()
```

### 模式 3：异步服务（非阻塞模式）

适用于需要与其他异步操作集成的应用。

```lua
local ldbus = require('ldbus')

-- 打开连接（自动启动）
local dbus = ldbus.nonblock.open_user(true)

-- 或者手动启动
-- local dbus = ldbus.nonblock.open_user(false)
-- dbus:start()

-- 请求服务名称
local ok, err = dbus:request_name("com.example.AsyncService")
if not ok then
    error("无法请求服务名称")
end

-- 定期调度
-- 需要与应用的事件循环集成
function on_timer()
    dbus:dispatch()
    dbus:flush()
end

-- 应用退出时
dbus:close()
```

## 环境变量

### DBUS_SESSION_BUS_ADDRESS

设置会话总线地址。

```lua
ldbus.setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/tmp/dbus-test")
```

### DBUS_SYSTEM_BUS_ADDRESS

设置系统总线地址。

```lua
ldbus.setenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/var/run/dbus/system_bus_socket")
```

## 注意事项

1. **连接管理**：确保在脚本结束前关闭连接
2. **错误处理**：始终检查返回值和错误对象
3. **超时设置**：合理设置超时时间避免无限等待
4. **内存管理**：Lua 会自动管理对象生命周期
5. **线程安全**：连接对象不是线程安全的

## 常见问题

### 如何选择阻塞还是非阻塞模式？

- **阻塞模式**：简单脚本、一次性任务、同步调用
- **非阻塞模式**：守护进程、需要与其他 I/O 集成

### 如何处理超时？

```lua
-- 使用 pcall 捕获超时异常
local ok, reply = pcall(function()
    return conn:send_with_reply_and_block(msg, 5000)
end)

if not ok then
    print("调用失败或超时: " .. tostring(reply))
end
```

### 如何监听信号？

目前 Lua 绑定主要用于方法调用，信号监听需要在事件循环中实现。

## 完整示例

### 查询 DBus 服务列表

```lua
#!/usr/bin/env lua

local ldbus = require('ldbus')

-- 创建连接
local dbus = ldbus.blocking.open_user()

-- 创建消息
local msg = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "ListNames"
)

-- 发送并接收
local conn = dbus.conn
local reply = conn:send_with_reply_and_block(msg, 5000)

-- 注意：当前版本返回的是 lightuserdata
-- 实际使用中需要进一步解析

print("成功获取服务列表")

-- 清理
dbus:close()
```

### 注册服务名称

```lua
#!/usr/bin/env lua

local ldbus = require('ldbus')

-- 创建非阻塞连接
local dbus = ldbus.nonblock.open_user()

-- 请求名称
local success, error = dbus:request_name("com.example.TestService", 0)

if success then
    print("成功注册服务名称")
else
    if error then
        print("注册失败: " .. error.name .. " - " .. error.message)
    else
        print("注册失败")
    end
end

-- 清理
dbus:close()
```

## 相关文档

- [blocking.md](dbus/blocking.md) - 阻塞模式详细文档
- [nonblock.md](dbus/nonblock.md) - 非阻塞模式详细文档
- [message.md](dbus/message.md) - 消息对象文档
- [connection.md](dbus/connection.md) - 连接对象文档
- [error.md](dbus/error.md) - 错误对象文档
- [shm_tree.md](shm_tree.md) - 共享内存 MDB 与 SHM 属性 Lua 绑定

## 测试

测试文件位于 `tests/lua/dbus/` 目录：
- `test_blocking.lua` - 阻塞模式测试
- `test_nonblock.lua` - 非阻塞模式测试
- `test_message.lua` - 消息测试
- `test_error.lua` - 错误测试
