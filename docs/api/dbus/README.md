# DBus C++ API 文档目录

本目录包含了 libmcpp DBus C++ API 的完整文档。

## 快速导航

### 入门文档

从 [../dbus.md](../dbus.md) 开始，了解 DBus 模块的概述、架构设计和快速开始。

## 文档列表

### 核心组件

#### [connection.md](connection.md)
DBus 连接管理，包括：
- `mc::dbus::connection` - 连接对象
- 连接工厂方法（open_system_bus, open_session_bus）
- 消息发送（send, send_with_reply, send_with_reply_and_block, async_send_with_reply）
- 路径注册（register_path, unregister_path）
- 名称管理（request_name, get_unique_name, set_unique_name）
- 匹配规则（add_match, remove_match, get_match）
- 连接状态管理（is_connected, disconnect, flush, start, dispatch）

#### [message.md](message.md)
DBus 消息处理，包括：
- `mc::dbus::message` - 消息对象
- `mc::dbus::message_reader` - 消息读取器
- `mc::dbus::message_writer` - 消息写入器
- 消息创建（new_method_call, new_signal, new_method_return, new_error）
- 参数序列化和反序列化
- 消息属性访问

#### [error.md](error.md)
DBus 错误处理，包括：
- `mc::dbus::error` - 错误对象
- `mc::dbus::error_names` - 标准错误名称常量
- 错误检查和处理
- 异常转换

#### [match.md](match.md)
DBus 匹配规则，包括：
- `mc::dbus::match_rule` - 匹配规则构建器
- `mc::dbus::match` - 匹配对象
- `mc::engine::service::add_match()` - 服务中的匹配规则（支持 DBus + 共享内存）
- 信号过滤
- 规则字符串生成

#### [signal.md](signal.md)
DBus 信号发射工具，包括：
- `mc::dbus::send_signal()` - 通过共享内存或 DBus 发送信号
- `mc::dbus::emit_interfaces_added()` - 发射 InterfacesAdded 信号
- `mc::dbus::emit_interfaces_removed()` - 发射 InterfacesRemoved 信号
- `mc::dbus::emit_properties_changed()` - 发射 PropertiesChanged 信号
- ObjectManager 接口支持

#### [validator.md](validator.md)
DBus 名称验证器，包括：
- `mc::dbus::validator` - 验证工具类
- 总线名称验证
- 接口名称验证
- 成员名称验证
- 路径验证
- 签名验证

#### [sd_bus.md](sd_bus.md)
DBus 客户端封装，包括：
- `mc::dbus::sd_bus` - sd_bus客户端类
- 同步和异步方法调用
- 自动 Requestor 字段管理
- devmon chip 接口方法映射
- 共享内存调用支持
- 超时控制和错误处理

#### [enums.md](enums.md)
DBus 枚举类型，包括：
- `mc::dbus::bus_type` - 总线类型
- `mc::dbus::message_type` - 消息类型
- `mc::dbus::message_header_field` - 消息头字段
- `mc::dbus::property_access` - 属性访问权限
- `mc::dbus::message_flag` - 消息标志
- `mc::dbus::connect_status` - 连接状态

#### [shm.md](shm.md)
DBus 共享内存通信，包括：
- `mc::dbus::shm::harbor` - 共享内存端口
- `mc::dbus::shm::shm_tree` - 共享内存树
- `mc::dbus::shm::local_msg` - 本地消息
- `mc::dbus::shm::serialize/deserialize` - 序列化工具
- `mc::dbus::shm::gvariant_convert` - GVariant 转换

## 文档结构

```
api/dbus/
├── README.md          # 本文件
├── connection.md      # 连接管理 (~17KB)
├── message.md         # 消息处理 (~19KB)
├── error.md           # 错误处理 (~10KB)
├── match.md           # 匹配规则 (~14KB)
├── signal.md          # 信号发射 (~8KB)
├── validator.md       # 名称验证 (~13KB)
├── sd_bus.md          # 客户端封装 (~15KB)
├── enums.md           # 枚举类型 (~11KB)
└── shm.md             # 共享内存 (~24KB)
```

## 学习路径

### 初学者（基础使用）

1. 阅读 [../dbus.md](../dbus.md) 了解整体架构
2. 学习 [sd_bus.md](sd_bus.md) sd_bus客户端使用
3. 或学习 [connection.md](connection.md) 创建和管理连接
4. 学习 [message.md](message.md) 创建和处理消息
5. 参考 [error.md](error.md) 处理错误
6. 查看示例代码：`examples/dbus/`

### 中级用户（服务开发）

1. 学习 [match.md](match.md) 过滤和监听信号
2. 学习 [signal.md](signal.md) 发射标准信号
3. 学习 [validator.md](validator.md) 验证 DBus 名称
4. 使用 [enums.md](enums.md) 了解所有枚举类型
5. 实现自己的 DBus 服务

### 高级用户（性能优化）

1. 学习 [shm.md](shm.md) 使用共享内存通信
2. 理解高性能消息传递机制
3. 优化序列化和反序列化
4. 实现零拷贝消息传递
5. 性能测试和调优

## 快速参考

### 创建连接

```cpp
// 连接到会话总线
auto conn = mc::dbus::connection::open_session_bus(executor);

// 连接到系统总线
auto conn = mc::dbus::connection::open_system_bus(executor);
```

### 发送方法调用

```cpp
// 创建消息
auto msg = mc::dbus::message::new_method_call(
    "org.freedesktop.DBus",
    "/org/freedesktop/DBus",
    "org.freedesktop.DBus",
    "GetId"
);

// 同步发送
auto reply = conn.send_with_reply_and_block(std::move(msg), 5000ms);

// 异步发送
auto fut = conn.async_send_with_reply(std::move(msg), 5000ms);
```

### 注册对象路径

```cpp
conn.register_path("/com/example/Object", 
    [](mc::dbus::connection& conn, mc::dbus::message& msg) {
        // 处理消息
        return mc::dbus::message::new_method_return(msg);
    }
);
```

### 监听信号

```cpp
auto match = mc::dbus::match_rule()
    .type(mc::dbus::message_type::signal)
    .sender("com.example.Service")
    .interface("com.example.Interface")
    .member("SignalName")
    .build();

conn.add_match(match, [](mc::dbus::message& msg) {
    // 处理信号
});
```

### 发射信号

```cpp
mc::dbus::emit_interfaces_added(
    conn,
    "/com/example/Object",
    {{"com.example.Interface", properties}}
);
```

### 错误处理

```cpp
mc::dbus::error err;

if (!dbus_function_call(..., err.get_ptr())) {
    if (err.is_set()) {
        elog("DBus错误: ${name} - ${message}",
            ("name", err.name())
            ("message", err.message()));
    }
}
```

## 类型系统

DBus 类型与 C++ 类型的映射关系：

| DBus 类型 | C++ 类型 | 说明 |
|-----------|---------|------|
| `y` | `uint8_t` | 字节 |
| `b` | `bool` | 布尔值 |
| `n` | `int16_t` | 16位整数 |
| `q` | `uint16_t` | 16位无符号整数 |
| `i` | `int32_t` | 32位整数 |
| `u` | `uint32_t` | 32位无符号整数 |
| `x` | `int64_t` | 64位整数 |
| `t` | `uint64_t` | 64位无符号整数 |
| `d` | `double` | 双精度浮点数 |
| `s` | `std::string` | 字符串 |
| `o` | `std::string` | 对象路径 |
| `g` | `std::string` | 签名 |
| `a` | `std::vector<T>` | 数组 |
| `(...)` | `std::tuple<...>` | 结构体 |
| `a{...}` | `std::map<K,V>` | 字典 |
| `v` | `mc::variant` | Variant |

详细说明参见 [message.md](message.md)

## 使用模式

### 模式 1：简单客户端

```cpp
// 创建连接
mc::io_context executor(1);
auto conn = mc::dbus::connection::open_session_bus(executor);

// 发送请求
auto msg = mc::dbus::message::new_method_call(...);
auto reply = conn.send_with_reply_and_block(std::move(msg), 5000ms);

// 解析回复
mc::dbus::message_reader reader(reply);
auto result = reader.read_basic<std::string>();
```

### 模式 2：异步客户端

```cpp
mc::io_context executor(1);
auto conn = mc::dbus::connection::open_session_bus(executor);
conn.start();

auto msg = mc::dbus::message::new_method_call(...);
auto fut = conn.async_send_with_reply(std::move(msg), 5000ms);

fut.then([](mc::dbus::message reply) {
    // 处理回复
});

executor.run();
```

### 模式 3：服务端

```cpp
mc::io_context executor(1);
auto conn = mc::dbus::connection::open_session_bus(executor);

// 请求服务名称
conn.request_name("com.example.MyService");

// 注册对象
conn.register_path("/com/example/Object", 
    [](mc::dbus::connection& conn, mc::dbus::message& msg) {
        // 业务逻辑
        auto reply = mc::dbus::message::new_method_return(msg);
        mc::dbus::message_writer writer(reply);
        writer.write_basic("result");
        return reply;
    }
);

// 启动
conn.start();
executor.run();
```

### 模式 4：高性能共享内存通信

```cpp
// 创建 harbor
mc::dbus::shm::harbor harbor(
    conn, 
    "/tmp/shm_harbor", 
    1024 * 1024  // 1MB
);

// 发送消息（零拷贝）
mc::dbus::shm::local_msg msg = create_large_message();
harbor.send(destination, std::move(msg));

// 接收消息
harbor.receive([](mc::dbus::shm::local_msg&& msg) {
    // 处理消息
});
```

## 线程安全

**连接对象**：
- 不是线程安全的
- 必须在创建它的 io_context 线程中使用
- 使用 `post()` 在 io_context 线程中执行操作

**消息对象**：
- 读取是线程安全的
- 写入不是线程安全的

**错误对象**：
- 不是线程安全的

## 性能优化

### 1. 使用异步 API

```cpp
// 好：异步，不阻塞
auto fut = conn.async_send_with_reply(std::move(msg));

// 差：同步，阻塞当前线程
auto reply = conn.send_with_reply_and_block(std::move(msg));
```

### 2. 批量操作

```cpp
// 发送多个消息
for (auto& msg : messages) {
    conn.send(std::move(msg));
}
conn.flush();  // 一次性刷新
```

### 3. 使用共享内存

```cpp
// 对于大消息（>4KB），使用共享内存
if (msg_size > 4096) {
    harbor.send(destination, std::move(msg));
} else {
    conn.send(std::move(msg));
}
```

### 4. 重用消息对象

```cpp
// 避免频繁创建消息对象
auto msg = mc::dbus::message::new_method_call(...);
// 重用 msg 对象...
```

## 调试技巧

### 启用 DBus 调试

```bash
export DBUS_VERBOSE=1
export DBUS_DEBUG=all
```

### 使用 dbus-monitor

```bash
# 监控会话总线
dbus-monitor --session

# 监控系统总线
sudo dbus-monitor --system

# 过滤特定服务
dbus-monitor "sender='com.example.Service'"
```

### 使用 busctl

```bash
# 列出服务
busctl list

# 查看服务对象
busctl tree com.example.Service

# 调用方法
busctl call com.example.Service /com/example/Object \
    com.example.Interface Method s "argument"
```

## 测试

单元测试位于 `tests/dbus/` 目录：
- `test_connection.cpp` - 连接测试
- `test_message.cpp` - 消息测试
- `test_error.cpp` - 错误测试
- `test_match.cpp` - 匹配测试
- `test_signal.cpp` - 信号测试
- `test_validator.cpp` - 验证器测试
- `test_shm.cpp` - 共享内存测试

示例代码位于 `examples/dbus/` 目录。

## 注意事项

1. **生命周期管理**：确保 io_context 和 connection 的生命周期正确
2. **异常处理**：DBus 操作可能抛出异常，注意捕获
3. **超时设置**：合理设置超时避免死锁
4. **内存管理**：使用 RAII 管理 DBus 资源
5. **错误检查**：始终检查错误对象

## 常见问题

### Q: 如何选择同步还是异步 API？

**A:**
- **同步 API**：适合简单工具、脚本、不要求高性能的场景
- **异步 API**：适合服务端、需要高并发的场景

### Q: 如何处理大消息？

**A:** 使用共享内存通信（shm）：
- 消息 < 4KB：使用普通 DBus 消息
- 消息 >= 4KB：使用共享内存

### Q: 如何实现请求-响应模式？

**A:** 参考 [connection.md](connection.md) 中的 `async_send_with_reply` 示例。

### Q: 如何监听属性变化？

**A:** 使用 match_rule 监听 `PropertiesChanged` 信号，参考 [match.md](match.md)。

### Q: 如何验证 DBus 名称？

**A:** 使用 `mc::dbus::validator`，参考 [validator.md](validator.md)。

## 相关文档

### 内部文档
- [../dbus.md](../dbus.md) - DBus 模块概述
- [../../9.dbus.md](../../9.dbus.md) - DBus 详细设计文档
- [../../lua_api/dbus.md](../../lua_api/dbus.md) - DBus Lua API

### 外部资源
- [DBus 规范](https://dbus.freedesktop.org/doc/dbus-specification.html)
- [DBus 教程](https://dbus.freedesktop.org/doc/dbus-tutorial.html)
- [sd-bus API](https://www.freedesktop.org/software/systemd/man/sd-bus.html)

## 贡献

如果发现文档错误或需要补充，请提交 Issue 或 Pull Request。

## 版权

Copyright (c) 2024-2026 Huawei Technologies Co., Ltd.
