# DBus Enums API

## 概述

`mc::dbus` 提供了一系列枚举类型，用于表示 DBus 的各种状态和类型。

## 总线类型

### bus_type

```cpp
enum class bus_type {
    session,  // 会话总线
    system,   // 系统总线
    starter   // 启动器总线
};
```

表示 DBus 总线类型。

**值说明：**
- `session` - 会话总线，用于用户会话内的进程通信
- `system` - 系统总线，用于系统服务和守护进程
- `starter` - 启动器总线，继承自启动进程的总线

**使用示例：**

```cpp
// 根据类型打开不同的总线
mc::dbus::bus_type type = mc::dbus::bus_type::system;

switch (type) {
    case mc::dbus::bus_type::session:
        conn = mc::dbus::connection::open_session_bus(executor);
        break;
    case mc::dbus::bus_type::system:
        conn = mc::dbus::connection::open_system_bus(executor);
        break;
    default:
        // 处理其他类型
        break;
}
```

## 消息类型

### message_type

```cpp
enum class message_type {
    invalid           = 0,  // 无效消息类型
    method_call       = 1,  // 方法调用消息
    method_return     = 2,  // 方法返回消息
    error             = 3,  // 错误消息
    signal            = 4,  // 信号消息
    num_message_types = 5
};
```

表示 DBus 消息类型。

**值说明：**
- `invalid` - 无效消息类型
- `method_call` - 方法调用，需要回复
- `method_return` - 方法返回，响应方法调用
- `error` - 错误响应
- `signal` - 信号，广播消息，不需要回复

**使用示例：**

```cpp
mc::dbus::message msg = ...;

// 检查消息类型
switch (msg.get_type()) {
    case mc::dbus::message_type::method_call:
        // 处理方法调用
        handle_method_call(msg);
        break;
        
    case mc::dbus::message_type::method_return:
        // 处理方法返回
        handle_method_return(msg);
        break;
        
    case mc::dbus::message_type::error:
        // 处理错误
        handle_error(msg);
        break;
        
    case mc::dbus::message_type::signal:
        // 处理信号
        handle_signal(msg);
        break;
        
    default:
        // 无效消息
        break;
}
```

## 消息头字段

### message_header_field

```cpp
enum class message_header_field {
    invalid      = 0,  // 无效字段
    path         = 1,  // 对象路径
    interface    = 2,  // 接口名称
    member       = 3,  // 成员名称(方法/信号)
    error_name   = 4,  // 错误名称
    reply_serial = 5,  // 回复的序列号
    destination  = 6,  // 目标服务
    sender       = 7,  // 发送者
    signature    = 8,  // 消息体签名
    unix_fds     = 9   // UNIX文件描述符数量
};
```

表示 DBus 消息头字段类型。

**值说明：**
- `invalid` - 无效字段
- `path` - 对象路径（如 `/org/freedesktop/DBus`）
- `interface` - 接口名称（如 `org.freedesktop.DBus`）
- `member` - 方法或信号名称（如 `GetName`）
- `error_name` - 错误名称（如 `org.freedesktop.DBus.Error.Failed`）
- `reply_serial` - 此消息回复的原始消息序列号
- `destination` - 目标服务名称
- `sender` - 发送者唯一名称（通常由总线分配）
- `signature` - 消息参数的类型签名
- `unix_fds` - 消息中传递的文件描述符数量

## 属性访问类型

### property_access

```cpp
enum class property_access {
    read,       // 只读
    write,      // 只写
    read_write  // 读写
};
```

表示属性的访问权限。

**值说明：**
- `read` - 只读属性，只能获取值
- `write` - 只写属性，只能设置值
- `read_write` - 读写属性，可以获取和设置值

**使用示例：**

```cpp
struct property_info {
    std::string name;
    mc::dbus::property_access access;
    std::string type_signature;
};

// 定义属性
property_info status_prop{
    "Status",
    mc::dbus::property_access::read,
    "s"
};

property_info value_prop{
    "Value",
    mc::dbus::property_access::read_write,
    "i"
};

// 检查访问权限
if (status_prop.access == mc::dbus::property_access::read) {
    std::cout << "Status 是只读属性" << std::endl;
}
```

## 消息标志位

### message_flag

```cpp
enum class message_flag {
    no_reply_expected               = 0x01,  // 不期望回复
    no_auto_start                   = 0x02,  // 不自动启动服务
    allow_interactive_authorization = 0x04   // 允许交互式授权
};
```

表示消息标志位。

**值说明：**
- `no_reply_expected` - 不期望回复，用于单向消息
- `no_auto_start` - 如果服务未运行，不自动启动它
- `allow_interactive_authorization` - 允许交互式授权对话框

**使用示例：**

```cpp
// 发送不需要回复的信号
auto msg = mc::dbus::message::new_signal(...);
// 设置标志位（通常由底层自动设置）
```

## 连接状态

### connect_status

```cpp
enum class connect_status {
    connected,
    disconnected,
    connecting,
    disconnecting
};
```

表示连接状态（内部使用）。

**值说明：**
- `connected` - 已连接
- `disconnected` - 已断开
- `connecting` - 正在连接
- `disconnecting` - 正在断开

## 使用示例

### 消息类型判断

```cpp
void process_message(mc::dbus::message& msg) {
    // 使用便捷方法检查类型
    if (msg.is_method_call()) {
        std::cout << "这是方法调用" << std::endl;
    }
    
    if (msg.is_signal()) {
        std::cout << "这是信号" << std::endl;
    }
    
    if (msg.is_error()) {
        std::cout << "这是错误消息" << std::endl;
    }
    
    // 或使用 get_type() 获取类型
    auto type = msg.get_type();
    switch (type) {
        case mc::dbus::message_type::method_call:
            handle_method(msg);
            break;
        case mc::dbus::message_type::signal:
            handle_signal(msg);
            break;
        default:
            break;
    }
}
```

### 属性定义

```cpp
class my_interface {
public:
    struct property_definition {
        std::string name;
        std::string signature;
        mc::dbus::property_access access;
        std::function<mc::variant()> getter;
        std::function<void(const mc::variant&)> setter;
    };
    
    std::vector<property_definition> get_properties() {
        return {
            // 只读属性
            {"Name", "s", mc::dbus::property_access::read,
             [this]() { return mc::variant(m_name); },
             nullptr},
            
            // 读写属性
            {"Value", "i", mc::dbus::property_access::read_write,
             [this]() { return mc::variant(m_value); },
             [this](const mc::variant& v) { m_value = v.as<int>(); }},
            
            // 只写属性（较少见）
            {"Password", "s", mc::dbus::property_access::write,
             nullptr,
             [this](const mc::variant& v) { set_password(v.as<std::string>()); }}
        };
    }
    
private:
    std::string m_name;
    int m_value;
    
    void set_password(const std::string& pwd) {
        // 处理密码
    }
};
```

### 总线类型选择

```cpp
class dbus_service {
public:
    dbus_service(mc::io_context& ctx, mc::dbus::bus_type type) 
        : m_executor(ctx) {
        
        // 根据类型连接不同总线
        switch (type) {
            case mc::dbus::bus_type::system:
                m_conn = mc::dbus::connection::open_system_bus(m_executor);
                std::cout << "连接到系统总线" << std::endl;
                break;
                
            case mc::dbus::bus_type::session:
                m_conn = mc::dbus::connection::open_session_bus(m_executor);
                std::cout << "连接到会话总线" << std::endl;
                break;
                
            default:
                throw std::invalid_argument("不支持的总线类型");
        }
    }
    
private:
    mc::io_context& m_executor;
    mc::dbus::connection m_conn;
};

// 使用
mc::io_context executor(1);

// 系统服务使用系统总线
dbus_service system_service(executor, mc::dbus::bus_type::system);

// 用户应用使用会话总线
dbus_service user_app(executor, mc::dbus::bus_type::session);
```

## 注意事项

1. **类型安全**：使用枚举类（enum class）提供类型安全
2. **标志位组合**：message_flag 可以使用位运算组合多个标志
3. **属性访问**：实现属性接口时需要检查访问权限
4. **消息类型**：发送消息前确保类型正确设置
5. **总线选择**：系统服务应使用系统总线，用户应用使用会话总线

## 相关接口

- [connection](connection.md) - 连接对象
- [message](message.md) - 消息对象
- [dbus](../dbus.md) - DBus 主文档
