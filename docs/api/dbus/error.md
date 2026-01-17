# DBus Error API

## 概述

`mc::dbus::error` 是 DBus 错误对象，封装了 DBus 错误信息，用于错误处理和错误传递。

## 类定义

```cpp
namespace mc::dbus {
    struct error : DBusError;
}
```

继承自 `DBusError`，封装 DBus 错误信息。

## 构造与析构

### error()

```cpp
error();
```

默认构造函数，创建未设置的错误对象。

### error(const error&)

```cpp
error(const error& other);
```

拷贝构造函数。

### error(error&&)

```cpp
error(error&& other) noexcept;
```

移动构造函数。

### ~error()

```cpp
~error();
```

析构函数，自动释放错误资源。

## 错误状态

### is_set

```cpp
bool is_set() const;
```

检查错误是否已设置。

**返回值：**
- 已设置返回 true，否则返回 false

**示例：**

```cpp
mc::dbus::error err;
if (!err.is_set()) {
    std::cout << "无错误" << std::endl;
}
```

## 设置错误

### set_error

```cpp
void set_error(std::string_view name, std::string_view message);
void set_error(std::string_view name, std::string_view message, const mc::dict& args);
```

设置错误信息。

**参数：**
- `name` [in] - 错误名称
- `message` [in] - 错误消息（模板）
- `args` [in] - 格式化参数（第二个重载）

**示例：**

```cpp
// 简单错误
error err;
err.set_error("com.example.Error.Failed", "操作失败");

// 带参数的错误
err.set_error(
    "com.example.Error.NotFound",
    "未找到文件: ${path}",
    mc::dict{{"path", "/tmp/test.txt"}}
);
```

### set_error_const

```cpp
void set_error_const(std::string_view name, std::string_view message);
```

设置常量错误信息。

**参数：**
- `name` [in] - 错误名称
- `message` [in] - 错误消息

**约束：**
- 消息字符串必须是常量字符串，不会被复制

**示例：**

```cpp
error err;
err.set_error_const("org.freedesktop.DBus.Error.Failed", "Operation failed");
```

## 错误名称常量

`mc::dbus::error_names` 命名空间提供了标准 DBus 错误名称常量。

### 标准错误名称

```cpp
namespace mc::dbus::error_names {
    constexpr std::string_view failed;              // 一般失败
    constexpr std::string_view no_memory;           // 内存不足
    constexpr std::string_view service_unknown;     // 服务未知
    constexpr std::string_view name_has_no_owner;   // 名称无所有者
    constexpr std::string_view no_reply;            // 无回复
    constexpr std::string_view io_error;            // IO错误
    constexpr std::string_view bad_address;         // 地址错误
    constexpr std::string_view not_supported;       // 不支持
    constexpr std::string_view limits_exceeded;     // 超出限制
    constexpr std::string_view access_denied;       // 访问被拒绝
    constexpr std::string_view auth_failed;         // 认证失败
    constexpr std::string_view no_server;           // 无服务器
    constexpr std::string_view timeout;             // 超时
    constexpr std::string_view no_network;          // 无网络
    constexpr std::string_view address_in_use;      // 地址已被使用
    constexpr std::string_view disconnected;        // 已断开
    constexpr std::string_view invalid_args;        // 参数无效
    constexpr std::string_view file_not_found;      // 文件未找到
    constexpr std::string_view file_exists;         // 文件已存在
    constexpr std::string_view unknown_method;      // 方法未知
    constexpr std::string_view unknown_object;      // 对象未知
    constexpr std::string_view unknown_interface;   // 接口未知
    constexpr std::string_view unknown_property;    // 属性未知
    constexpr std::string_view property_read_only;  // 属性只读
}
```

### 使用标准错误名称

```cpp
error err;
err.set_error(mc::dbus::error_names::invalid_args, "参数不正确");
```

## 访问错误信息

错误对象继承自 `DBusError`，可以直接访问其成员：

```cpp
error err;
err.set_error("com.example.Error", "错误消息");

// 访问错误名称
std::string_view error_name = err.name;

// 访问错误消息
std::string_view error_message = err.message;

std::cout << "错误: " << error_name << " - " << error_message << std::endl;
```

## 使用示例

### 检查操作错误

```cpp
// 请求名称
auto [success, error] = conn.request_name("com.example.Service");

if (!success) {
    if (error.has_value()) {
        const auto& err = error.value();
        std::cerr << "请求名称失败: " 
                  << err.name << " - " << err.message << std::endl;
    }
}
```

### 发送错误响应

```cpp
void handle_method_call(mc::dbus::connection& conn, 
                       const mc::dbus::message& msg) {
    try {
        // 处理方法调用
        process_request(msg);
        
        // 发送正常响应
        auto reply = mc::dbus::message::new_method_return(msg);
        reply.writer() << "success";
        conn.send(std::move(reply));
        
    } catch (const std::invalid_argument& e) {
        // 发送错误响应
        auto error_msg = mc::dbus::message::new_error(
            msg,
            mc::dbus::error_names::invalid_args,
            e.what()
        );
        conn.send(std::move(error_msg));
        
    } catch (const std::exception& e) {
        // 发送一般错误
        auto error_msg = mc::dbus::message::new_error(
            msg,
            mc::dbus::error_names::failed,
            e.what()
        );
        conn.send(std::move(error_msg));
    }
}
```

### 检查消息错误

```cpp
// 发送消息并检查错误
auto reply = conn.send_with_reply_and_block(std::move(msg));

if (reply.is_error()) {
    std::cerr << "错误: " << reply.get_error_name() 
              << " - " << reply.get_error_message() << std::endl;
    return;
}

// 处理正常响应
auto result = reply.as<std::string>();
```

### 自定义错误处理

```cpp
class my_error_handler {
public:
    void handle_dbus_error(const mc::dbus::error& err) {
        if (!err.is_set()) {
            return;
        }
        
        std::string_view name = err.name;
        
        if (name == mc::dbus::error_names::timeout) {
            handle_timeout(err.message);
        } else if (name == mc::dbus::error_names::access_denied) {
            handle_access_denied(err.message);
        } else if (name == mc::dbus::error_names::invalid_args) {
            handle_invalid_args(err.message);
        } else {
            handle_general_error(err.name, err.message);
        }
    }
    
private:
    void handle_timeout(std::string_view msg) {
        std::cerr << "超时: " << msg << std::endl;
    }
    
    void handle_access_denied(std::string_view msg) {
        std::cerr << "访问被拒绝: " << msg << std::endl;
    }
    
    void handle_invalid_args(std::string_view msg) {
        std::cerr << "参数无效: " << msg << std::endl;
    }
    
    void handle_general_error(std::string_view name, std::string_view msg) {
        std::cerr << "错误 [" << name << "]: " << msg << std::endl;
    }
};
```

## 注意事项

1. **资源管理**：error 对象会自动管理内存，析构时释放资源
2. **拷贝语义**：拷贝 error 对象会复制错误信息
3. **移动语义**：移动 error 对象高效且不会复制错误信息
4. **常量字符串**：`set_error_const` 用于常量字符串，避免内存拷贝
5. **线程安全**：error 对象不是线程安全的

## 相关接口

- [connection](connection.md) - 连接对象
- [message](message.md) - 消息对象
- [validator](validator.md) - 名称验证
