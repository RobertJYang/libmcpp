# LibMCPP Lua API 文档

欢迎使用 LibMCPP Lua API 文档。本文档提供了 Lua 绑定的详细说明和使用示例。

## Lua 模块

### 进程间通信

- [dbus](dbus.md) - DBus 通信库 Lua 绑定
- [shm_tree](shm_tree.md) - 共享内存 MDB 查询与 SHM 属性访问 Lua 绑定

## 快速开始

### 安装和加载

```lua
-- 加载 ldbus 模块
local ldbus = require('ldbus')
```

### 基本使用

```lua
-- 创建 DBus 连接
local dbus = ldbus.blocking.open_user()

-- 请求服务名称
local ok, err = dbus:request_name("com.example.MyService")
if not ok then
    print("失败: " .. tostring(err))
    return
end

-- 使用连接...

-- 清理
dbus:close()
```

## 文档结构

```
lua_api/
├── index.md           # 本文件
├── shm_tree.md        # 共享内存 MDB 与 SHM 属性 Lua 绑定
└── dbus/              # DBus Lua API
    ├── README.md      # DBus 文档索引
    ├── blocking.md    # 阻塞模式
    ├── nonblock.md    # 非阻塞模式
    ├── message.md     # 消息对象
    ├── connection.md  # 连接对象
    └── error.md       # 错误对象
```

## 模块概览

### ldbus - DBus 通信

DBus 的 Lua 绑定，支持：
- ✓ 阻塞和非阻塞模式
- ✓ 方法调用
- ✓ 服务名称请求
- ✓ 连接管理
- ✓ 错误处理
- ✗ 信号监听（规划中）
- ✗ 参数序列化（规划中）

详见 [dbus.md](dbus.md)

## 测试示例

测试代码位于 `tests/lua/` 目录，提供了完整的使用示例：

```bash
# 运行 DBus 测试
lua tests/lua/dbus/test_blocking.lua
lua tests/lua/dbus/test_nonblock.lua
lua tests/lua/dbus/test_message.lua
lua tests/lua/dbus/test_error.lua
```

## 最佳实践

### 1. 错误处理

始终检查返回值和错误对象：

```lua
local ok, err = dbus:request_name("com.example.Test")
if not ok then
    if err then
        print("错误: " .. err.name .. " - " .. err.message)
    end
    return
end
```

### 2. 资源清理

确保在脚本结束前关闭连接：

```lua
local dbus = ldbus.blocking.open_user()

-- 使用 pcall 确保异常时也能清理
local ok, result = pcall(function()
    -- 业务逻辑
end)

-- 总是清理
dbus:close()

if not ok then
    error(result)
end
```

### 3. 超时设置

根据操作性质设置合理的超时：

```lua
-- 快速操作：1-2 秒
local reply = conn:send_with_reply_and_block(msg, 1000)

-- 一般操作：5 秒
local reply = conn:send_with_reply_and_block(msg, 5000)

-- 耗时操作：30 秒或更长
local reply = conn:send_with_reply_and_block(msg, 30000)
```

### 4. 模式选择

根据应用类型选择合适的模式：

```lua
-- 脚本工具：使用阻塞模式
local dbus = ldbus.blocking.open_user()

-- 服务守护进程：使用非阻塞模式
local dbus = ldbus.nonblock.open_user()
```

## 示例代码

### Hello World

```lua
#!/usr/bin/env lua

local ldbus = require('ldbus')

-- 创建连接
local dbus = ldbus.blocking.open_user()
print("DBus 连接已创建")

-- 获取唯一名称
local conn = dbus.conn
local name = conn:unique_name()
print("唯一名称: " .. (name or "未知"))

-- 关闭连接
dbus:close()
print("连接已关闭")
```

### 服务注册

```lua
#!/usr/bin/env lua

local ldbus = require('ldbus')

-- 创建非阻塞连接
local dbus = ldbus.nonblock.open_user()

-- 请求服务名称
local ok, err = dbus:request_name("com.example.HelloService")

if ok then
    print("成功注册服务: com.example.HelloService")
else
    print("注册失败")
    if err then
        print("错误: " .. err.name)
        print("消息: " .. err.message)
    end
end

-- 清理
dbus:close()
```

## 常见问题

### Q: 如何选择阻塞还是非阻塞模式？

**A:** 
- **阻塞模式**：适合一次性脚本、简单工具、同步调用场景
- **非阻塞模式**：适合守护进程、需要事件循环的应用

### Q: 如何传递方法参数？

**A:** 当前版本不支持在 Lua 层添加参数。需要：
- 在 C++ 层封装完整的方法调用
- 或扩展 Lua 绑定添加参数序列化支持

### Q: 如何解析回复消息？

**A:** 当前版本返回 lightuserdata，需要：
- 在 C++ 层解析并返回 Lua 表
- 或扩展 Lua 绑定添加消息解析支持

### Q: 如何监听信号？

**A:** 当前版本暂不支持。可以：
- 使用阻塞模式的 `run_once()` 在事件循环中处理
- 或在 C++ 层注册回调

### Q: 为什么要访问 conn.conn？

**A:** 外层对象（dbus）管理连接生命周期和 IO 上下文，内层对象（conn.conn）提供底层 DBus 操作。这种设计分离了资源管理和功能接口。

## 调试技巧

### 启用 DBus 调试

```lua
local ldbus = require('ldbus')

-- 设置 DBus 调试环境变量
ldbus.setenv("DBUS_VERBOSE", "1", true)

-- 创建连接
local dbus = ldbus.blocking.open_user()

-- 后续操作会输出调试信息
```

### 捕获所有错误

```lua
local ldbus = require('ldbus')

-- 使用 xpcall 获取完整的错误栈
local function error_handler(err)
    print("错误: " .. tostring(err))
    print(debug.traceback())
end

local ok = xpcall(function()
    local dbus = ldbus.blocking.open_user()
    -- 业务逻辑...
    dbus:close()
end, error_handler)
```

## 相关资源

### 内部资源
- [C++ DBus API](../api/dbus.md) - C++ DBus API 文档
- [DBus 设计文档](../9.dbus.md) - 详细设计说明

### 外部资源
- [DBus 官方网站](https://www.freedesktop.org/wiki/Software/dbus/)
- [DBus 规范](https://dbus.freedesktop.org/doc/dbus-specification.html)
- [Lua 官方手册](https://www.lua.org/manual/5.4/)

## 贡献

如果发现文档错误或需要补充，请提交 Issue 或 Pull Request。

## 版权

Copyright (c) 2024-2026 Huawei Technologies Co., Ltd.
