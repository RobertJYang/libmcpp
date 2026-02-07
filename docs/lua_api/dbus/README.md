# DBus Lua API 文档目录

本目录包含了 libmcpp DBus Lua 绑定的完整 API 文档。

## 快速导航

### 入门文档

从 [../dbus.md](../dbus.md) 开始，了解 DBus Lua 绑定的概述、模块结构和快速开始。

## 文档列表

### 核心模块

#### [blocking.md](blocking.md)
阻塞模式 DBus 接口，包括：
- `blocking.new()` - 创建连接
- `blocking.open_user()` - 打开用户会话
- `blocking.shutdown()` - 关闭库
- 连接对象方法（start, request_name, close, flush, dispatch, run_once）

#### [nonblock.md](nonblock.md)
非阻塞模式 DBus 接口，包括：
- `nonblock.new()` - 创建连接
- `nonblock.open_user()` - 打开用户会话
- `nonblock.shutdown()` - 关闭库
- 连接对象方法（start, request_name, close, flush, dispatch）
- 共享内存相关方法（notify_signal, add_match）

#### [message.md](message.md)
消息创建工具，包括：
- `message.new_method_call()` - 创建方法调用消息
- 消息对象的生命周期管理

#### [connection.md](connection.md)
内部连接对象 API，包括：
- `send_with_reply_and_block()` - 发送并等待回复
- 连接管理方法（close, flush, dispatch）
- 名称管理（request_name, unique_name）
- 底层访问（to_raw）

#### [error.md](error.md)
错误对象，包括：
- `is_set()` - 检查错误
- `ensure_ok()` - 确保无错误
- 错误属性（name, message）
- 错误处理模式

#### [interface.md](interface.md)
D-Bus接口模块，包括：
- `new()` - 创建接口
- `add_property()` - 添加属性
- `add_method()` - 添加方法
- `add_signal()` - 添加信号
- `set_property()` / `get_property()` - 属性访问
- `get_name()` - 获取接口名称

#### [object.md](object.md)
D-Bus对象模块，包括：
- `new()` - 创建对象
- `register_interface()` - 注册接口
- `get_interface()` - 获取接口
- `set_property()` / `get_property()` - 属性访问（返回错误码）

## 文档结构

```
lua_api/dbus/
├── README.md          # 本文件
├── blocking.md        # 阻塞模式 (~8KB)
├── nonblock.md        # 非阻塞模式 (~7KB)
├── message.md         # 消息对象 (~6KB)
├── connection.md      # 连接对象 (~8KB)
├── error.md           # 错误对象 (~7KB)
├── interface.md       # 接口模块 (~5KB)
└── object.md          # 对象模块 (~5KB)
```

## 学习路径

### 初学者（脚本开发）

1. 阅读 [../dbus.md](../dbus.md) 了解基本概念
2. 学习 [blocking.md](blocking.md) 阻塞模式
3. 学习 [message.md](message.md) 创建消息
4. 参考 [error.md](error.md) 处理错误
5. 查看测试示例：`tests/lua/dbus/test_blocking.lua`

### 进阶用户（服务开发）

1. 学习 [nonblock.md](nonblock.md) 非阻塞模式
2. 学习 [connection.md](connection.md) 连接详细API
3. 实现事件循环和消息处理
4. 查看测试示例：`tests/lua/dbus/test_nonblock.lua`

## 快速参考

### 创建连接

```lua
-- 阻塞模式
local dbus = ldbus.blocking.open_user()

-- 非阻塞模式
local dbus = ldbus.nonblock.open_user()
```

### 请求服务名称

```lua
local ok, err = dbus:request_name("com.example.MyService")
if not ok then
    print("失败: " .. tostring(err))
end
```

### 发送消息

```lua
local msg = ldbus.message.new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "GetId"
)

local conn = dbus.conn
local reply = conn:send_with_reply_and_block(msg, 5000)
```

### 事件循环

```lua
-- 阻塞模式
dbus:start()
while running do
    dbus:run_once(100)
end

-- 非阻塞模式
dbus:start()
while running do
    dbus:dispatch()
    dbus:flush()
    -- 处理其他事件...
end
```

## 测试示例

完整的测试示例位于：
- `tests/lua/dbus/test_blocking.lua` - 阻塞模式测试
- `tests/lua/dbus/test_nonblock.lua` - 非阻塞模式测试
- `tests/lua/dbus/test_message.lua` - 消息测试
- `tests/lua/dbus/test_error.lua` - 错误测试

## 使用限制

当前 Lua 绑定的限制：

1. **参数传递**：不支持在 Lua 层添加消息参数
2. **回复解析**：不支持在 Lua 层解析回复消息
3. **信号创建**：不支持创建信号消息
4. **回调注册**：不支持注册消息回调（C++ 层可以）

这些限制可以通过以下方式解决：
- 在 C++ 层封装完整的方法调用
- 扩展 Lua 绑定添加缺失功能
- 使用 FFI 直接调用 C API

## 相关资源

### 内部文档
- [C++ DBus API](../../api/dbus.md) - C++ DBus API 文档
- [DBus 详细设计](../../9.dbus.md) - 设计文档

### 外部资源
- [DBus 规范](https://dbus.freedesktop.org/doc/dbus-specification.html)
- [DBus 教程](https://dbus.freedesktop.org/doc/dbus-tutorial.html)
- [Lua 5.4 手册](https://www.lua.org/manual/5.4/)

## 贡献

如果发现文档错误或需要补充，请提交 Issue 或 Pull Request。

## 版权

Copyright (c) 2024-2026 Huawei Technologies Co., Ltd.
