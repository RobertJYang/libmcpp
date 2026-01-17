# DBus Connection API

## 概述

`mc::dbus::connection` 是 DBus 连接对象，用于管理与 DBus 总线的连接，发送和接收消息。

## 类定义

```cpp
namespace mc::dbus {
    class connection;
}
```

## 创建连接

### 静态工厂方法

#### open_system_bus

```cpp
static connection open_system_bus(mc::io_context& executor);
```

打开系统总线连接。

**参数：**
- `executor` [in] - IO上下文执行器

**返回值：**
- 返回DBus系统总线连接对象

**异常：**
- 连接失败时抛出 `mc::system_exception`

**示例：**

```cpp
mc::io_context executor(1);
auto conn = mc::dbus::connection::open_system_bus(executor);
```

#### open_session_bus

```cpp
static connection open_session_bus(mc::io_context& executor);
```

打开会话总线连接。

**参数：**
- `executor` [in] - IO上下文执行器

**返回值：**
- 返回DBus会话总线连接对象

**异常：**
- 连接失败时抛出 `mc::system_exception`

**示例：**

```cpp
mc::io_context executor(1);
auto conn = mc::dbus::connection::open_session_bus(executor);
```

### 构造函数

#### connection()

```cpp
connection();
```

默认构造函数，创建空连接对象。

#### connection(mc::io_context&, DBusConnection*, bool)

```cpp
explicit connection(mc::io_context& executor, DBusConnection* conn, bool add_ref = false);
```

从底层 DBusConnection 构造连接对象。

**参数：**
- `executor` [in] - IO上下文执行器
- `conn` [in] - DBus连接指针
- `add_ref` [in] - 是否增加引用计数，默认为 false

## 连接管理

### start

```cpp
bool start();
```

启动连接并开始消息分发。

**返回值：**
- 成功返回 true，失败返回 false

**异常：**
- 启动失败时可能抛出异常

**示例：**

```cpp
auto conn = mc::dbus::connection::open_system_bus(executor);
if (conn.start()) {
    std::cout << "连接启动成功" << std::endl;
}
```

### disconnect

```cpp
void disconnect();
```

断开连接。

**注意：**
- 连接是共享的，断开连接不会影响其他引用同一连接的对象

### flush

```cpp
void flush();
```

刷新连接，将缓冲的消息发送到底层传输。

### dispatch

```cpp
void dispatch();
```

分发消息，处理队列中的待处理消息。

## 名称管理

### request_name

```cpp
std::tuple<bool, std::optional<error>> request_name(std::string_view name, uint32_t flags = 0);
```

请求总线名称。

**参数：**
- `name` [in] - 要请求的名称
- `flags` [in] - 标志位，默认为 0

**返回值：**
- 返回元组：(是否成功, 可选的错误信息)

**标志位：**
- `DBUS_NAME_FLAG_ALLOW_REPLACEMENT` - 允许被替换
- `DBUS_NAME_FLAG_REPLACE_EXISTING` - 替换现有所有者
- `DBUS_NAME_FLAG_DO_NOT_QUEUE` - 不排队

**示例：**

```cpp
auto [success, error] = conn.request_name("com.example.MyService");
if (!success) {
    if (error.has_value()) {
        std::cerr << "请求名称失败: " << error->message << std::endl;
    }
}
```

## 消息发送

### send

```cpp
bool send(message&& msg);
```

发送消息（不等待回复）。

**参数：**
- `msg` [in] - 要发送的消息（移动语义）

**返回值：**
- 成功返回 true，失败返回 false

**示例：**

```cpp
auto msg = mc::dbus::message::new_signal("/com/example/Object", 
                                         "com.example.Interface",
                                         "StatusChanged");
msg.writer() << "running";
conn.send(std::move(msg));
```

### send_with_reply

```cpp
message send_with_reply(message&& msg, mc::milliseconds timeout = DBUS_TIMEOUT_DEFAULT);
```

发送消息并同步等待回复（阻塞）。

**参数：**
- `msg` [in] - 要发送的消息
- `timeout` [in] - 超时时间，默认为 60 秒

**返回值：**
- 返回响应消息

**异常：**
- 超时时抛出 `mc::timeout_exception`
- 其他错误抛出相应异常

**示例：**

```cpp
auto msg = mc::dbus::message::new_method_call(
    "org.freedesktop.DBus", "/", "org.freedesktop.DBus", "ListNames");

try {
    auto reply = conn.send_with_reply(std::move(msg), mc::seconds(5));
    auto names = reply.as<std::vector<std::string>>();
} catch (const mc::timeout_exception& e) {
    std::cerr << "超时: " << e.what() << std::endl;
}
```

### send_with_reply_and_block

```cpp
message send_with_reply_and_block(message&& msg, mc::milliseconds timeout = DBUS_TIMEOUT_DEFAULT);
```

`send_with_reply` 的别名，功能完全相同。

### async_send_with_reply

```cpp
future<message> async_send_with_reply(message&& msg, mc::milliseconds timeout = DBUS_TIMEOUT_DEFAULT);
```

发送消息并异步等待回复（非阻塞）。

**参数：**
- `msg` [in] - 要发送的消息
- `timeout` [in] - 超时时间

**返回值：**
- 返回 future 对象，包含回复消息

**示例：**

```cpp
auto msg = mc::dbus::message::new_method_call(
    "org.freedesktop.DBus", "/", "org.freedesktop.DBus", "ListNames");

auto future = conn.async_send_with_reply(std::move(msg));
future.then([](mc::dbus::message reply) {
    auto names = reply.as<std::vector<std::string>>();
    // 处理结果
}).on_error([](std::exception_ptr e) {
    try {
        std::rethrow_exception(e);
    } catch (const std::exception& ex) {
        std::cerr << "错误: " << ex.what() << std::endl;
    }
});
```

## 对象路径注册

### register_path

```cpp
void register_path(std::string_view path, path_handler_type handler);
```

注册对象路径处理器。

**参数：**
- `path` [in] - 对象路径
- `handler` [in] - 消息处理回调函数

**处理器签名：**

```cpp
using path_handler_type = std::function<DBusHandlerResult(message&)>;
```

**示例：**

```cpp
conn.register_path("/com/example/Object", [](mc::dbus::message& msg) {
    if (msg.is_method_call()) {
        auto method = msg.get_member();
        // 处理方法调用
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
});
```

### unregister_path

```cpp
void unregister_path(std::string_view path);
```

注销对象路径。

**参数：**
- `path` [in] - 要注销的对象路径

## 消息匹配

### add_match

```cpp
void add_match(match_rule& rule, match_cb_t&& cb, uint64_t id);
```

添加消息匹配规则。

**参数：**
- `rule` [in] - 匹配规则
- `cb` [in] - 匹配成功时的回调函数
- `id` [in] - 规则的唯一标识符

**异常：**
- 添加失败时抛出异常

**示例：**

```cpp
auto rule = mc::dbus::match_rule::new_signal("NameOwnerChanged", 
                                             "org.freedesktop.DBus");
conn.add_match(rule, [](mc::dbus::message& msg) {
    std::cout << "收到信号: " << msg.get_member() << std::endl;
}, 1);
```

### remove_match

```cpp
void remove_match(uint64_t id);
```

移除消息匹配规则。

**参数：**
- `id` [in] - 规则的唯一标识符

**异常：**
- 移除失败时抛出异常

### get_match

```cpp
match& get_match();
```

获取匹配对象。

**返回值：**
- 返回匹配对象的引用

## 连接状态查询

### is_connected

```cpp
bool is_connected() const;
```

获取当前连接状态（缓存状态）。

**返回值：**
- 已连接返回 true，否则返回 false

### get_is_connected

```cpp
bool get_is_connected() const;
```

获取DBus连接的实际状态（查询底层连接）。

**返回值：**
- 已连接返回 true，否则返回 false

**注意：**
- 此方法会查询底层 DBus 连接的实际状态，比 `is_connected()` 更准确但开销更大

### get_unique_name

```cpp
std::string_view get_unique_name() const;
```

获取DBus连接的唯一名称。

**返回值：**
- 返回唯一名称（如 `:1.123`）

### set_unique_name

```cpp
void set_unique_name(std::string_view name);
```

设置DBus连接的唯一名称。

**参数：**
- `name` [in] - 唯一名称

## 底层访问

### get_connection

```cpp
DBusConnection* get_connection() const;
```

获取底层 DBusConnection 指针。

**返回值：**
- 返回 DBusConnection 指针

**注意：**
- 仅在需要直接访问底层 API 时使用

### get_next_serial

```cpp
uint32_t get_next_serial();
```

获取下一个消息序列号。

**返回值：**
- 返回下一个可用的消息序列号

### filter_message

```cpp
filter_message_signal_type& filter_message() const;
```

获取消息过滤信号对象。

**返回值：**
- 返回消息过滤信号对象的引用

**约束：**
- 用于注册消息过滤回调

**示例：**

```cpp
conn.filter_message().connect([](mc::dbus::message& msg) -> DBusHandlerResult {
    // 过滤所有消息
    std::cout << "收到消息: " << msg.get_member() << std::endl;
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
});
```

## 使用示例

### 完整的服务示例

```cpp
#include <mc/dbus/connection.h>
#include <mc/dbus/message.h>
#include <mc/runtime.h>

class my_dbus_service {
public:
    my_dbus_service() : m_executor(1) {
        // 连接到系统总线
        m_conn = mc::dbus::connection::open_system_bus(m_executor);
        
        // 请求服务名称
        auto [success, error] = m_conn.request_name("com.example.MyService");
        if (!success) {
            throw std::runtime_error("无法请求服务名称");
        }
        
        // 注册对象路径
        m_conn.register_path("/com/example/Object", 
            [this](mc::dbus::message& msg) {
                return handle_message(msg);
            });
        
        // 启动连接
        m_conn.start();
    }
    
    ~my_dbus_service() {
        m_conn.disconnect();
    }
    
private:
    DBusHandlerResult handle_message(mc::dbus::message& msg) {
        if (!msg.is_method_call()) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        
        auto method = msg.get_member();
        if (method == "Hello") {
            auto reply = mc::dbus::message::new_method_return(msg);
            reply.writer() << "Hello, World!";
            m_conn.send(std::move(reply));
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    
    mc::io_context m_executor;
    mc::dbus::connection m_conn;
};
```

## 注意事项

1. **线程安全**：connection 对象不是线程安全的，需要外部同步
2. **生命周期**：确保 io_context 的生命周期长于 connection
3. **消息所有权**：发送消息使用移动语义，发送后原消息对象不可用
4. **错误处理**：始终检查返回值和捕获异常
5. **资源管理**：在析构时调用 disconnect() 断开连接

## 相关接口

- [message](message.md) - 消息对象
- [match](match.md) - 消息匹配
- [error](error.md) - 错误处理
