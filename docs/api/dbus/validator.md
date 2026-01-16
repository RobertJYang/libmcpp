# DBus Validator API

## 概述

`mc::dbus::validator` 提供了 DBus 名称和消息的验证功能，确保符合 DBus 规范。

## 类定义

```cpp
namespace mc::dbus {
    class validator;
}
```

## 验证方法

所有方法都是静态方法，可以直接调用。

### is_valid_interface_name

```cpp
static bool is_valid_interface_name(std::string_view name);
```

检查接口名称是否有效。

**参数：**
- `name` [in] - 接口名称

**返回值：**
- 有效返回 true，否则返回 false

**约束：**
- 接口名称必须符合DBus规范

**规范要求：**
- 由点号分隔的多个元素组成，至少两个元素
- 每个元素只能包含 `[A-Z][a-z][0-9]_`
- 每个元素必须以字母开头
- 每个元素长度至少为 1 个字符
- 总长度不超过 255 个字符

**示例：**

```cpp
// 有效的接口名称
mc::dbus::validator::is_valid_interface_name("org.freedesktop.DBus");  // true
mc::dbus::validator::is_valid_interface_name("com.example.MyInterface");  // true

// 无效的接口名称
mc::dbus::validator::is_valid_interface_name("com.example");  // false (只有一个点)
mc::dbus::validator::is_valid_interface_name("com.123example.Test");  // false (元素以数字开头)
mc::dbus::validator::is_valid_interface_name("com.example!");  // false (包含非法字符)
```

### is_valid_member_name

```cpp
static bool is_valid_member_name(std::string_view name);
```

检查成员名称（方法名或信号名）是否有效。

**参数：**
- `name` [in] - 成员名称

**返回值：**
- 有效返回 true，否则返回 false

**约束：**
- 成员名称必须符合DBus规范

**规范要求：**
- 只能包含 `[A-Z][a-z][0-9]_`
- 必须以字母开头
- 长度至少为 1 个字符
- 总长度不超过 255 个字符

**示例：**

```cpp
// 有效的成员名称
mc::dbus::validator::is_valid_member_name("GetStatus");  // true
mc::dbus::validator::is_valid_member_name("StatusChanged");  // true
mc::dbus::validator::is_valid_member_name("set_value");  // true

// 无效的成员名称
mc::dbus::validator::is_valid_member_name("123Method");  // false (以数字开头)
mc::dbus::validator::is_valid_member_name("Get-Status");  // false (包含连字符)
mc::dbus::validator::is_valid_member_name("");  // false (空字符串)
```

### is_valid_bus_name

```cpp
static bool is_valid_bus_name(std::string_view name);
```

检查总线名称是否有效。

**参数：**
- `name` [in] - 总线名称

**返回值：**
- 有效返回 true，否则返回 false

**约束：**
- 有效bus name格式：
  - bus name是以点号分段的多段字符串，至少需要一个点的两段名称
  - 如果以冒号开头表示唯一的bus name（如 `:1.123`）
  - 每段名称只能包含字母、数字、下划线
  - 每段不能以数字和点开头

**规范要求：**
- 由点号分隔的多个元素组成（唯一名称除外）
- 每个元素只能包含 `[A-Z][a-z][0-9]_` 和连字符 `-`
- 每个元素必须以字母、数字或下划线开头（不能以连字符开头）
- 总长度不超过 255 个字符
- 唯一名称以冒号开头，格式为 `:数字.数字`

**示例：**

```cpp
// 有效的总线名称
mc::dbus::validator::is_valid_bus_name("org.freedesktop.DBus");  // true
mc::dbus::validator::is_valid_bus_name("com.example.MyService");  // true
mc::dbus::validator::is_valid_bus_name(":1.123");  // true (唯一名称)

// 无效的总线名称
mc::dbus::validator::is_valid_bus_name("example");  // false (没有点)
mc::dbus::validator::is_valid_bus_name("com.123example.Test");  // false (元素以数字开头)
```

### is_valid_error_name

```cpp
static bool is_valid_error_name(std::string_view errorname);
```

检查错误名称是否有效。

**参数：**
- `errorname` [in] - 错误名称

**返回值：**
- 有效返回 true，否则返回 false

**约束：**
- 错误名称必须符合DBus规范

**规范要求：**
- 与接口名称规则相同
- 由点号分隔的多个元素组成
- 每个元素只能包含 `[A-Z][a-z][0-9]_`
- 每个元素必须以字母开头

**示例：**

```cpp
// 有效的错误名称
mc::dbus::validator::is_valid_error_name("org.freedesktop.DBus.Error.Failed");  // true
mc::dbus::validator::is_valid_error_name("com.example.Error.NotFound");  // true

// 无效的错误名称
mc::dbus::validator::is_valid_error_name("Error.Failed");  // false (只有一个点)
mc::dbus::validator::is_valid_error_name("com.example.error!");  // false (包含非法字符)
```

### is_valid_path

```cpp
static bool is_valid_path(std::string_view path);
```

检查对象路径是否有效。

**参数：**
- `path` [in] - 对象路径

**返回值：**
- 有效返回 true，否则返回 false

**约束：**
- 对象路径必须符合DBus规范

**规范要求：**
- 必须以斜杠 `/` 开头
- 由斜杠分隔的多个元素组成
- 每个元素只能包含 `[A-Z][a-z][0-9]_`
- 根路径 `/` 是特殊的有效路径
- 不能以斜杠结尾（除非是根路径）
- 不能包含连续的斜杠

**示例：**

```cpp
// 有效的对象路径
mc::dbus::validator::is_valid_path("/");  // true (根路径)
mc::dbus::validator::is_valid_path("/org/freedesktop/DBus");  // true
mc::dbus::validator::is_valid_path("/com/example/Object123");  // true

// 无效的对象路径
mc::dbus::validator::is_valid_path("org/freedesktop/DBus");  // false (不以/开头)
mc::dbus::validator::is_valid_path("/org/freedesktop/");  // false (以/结尾)
mc::dbus::validator::is_valid_path("/org//freedesktop");  // false (连续斜杠)
mc::dbus::validator::is_valid_path("/org/123example");  // false (元素以数字开头)
```

### is_message_too_large

```cpp
static bool is_message_too_large(std::size_t size);
```

检查消息大小是否过大。

**参数：**
- `size` [in] - 消息大小（字节）

**返回值：**
- 过大返回 true，否则返回 false

**约束：**
- 消息大小不能超过 `maximum_message_size`

**示例：**

```cpp
std::size_t msg_size = calculate_message_size();
if (mc::dbus::validator::is_message_too_large(msg_size)) {
    std::cerr << "消息太大，无法发送" << std::endl;
    return;
}
```

## 常量

### maximum_array_size

```cpp
static constexpr uint32_t maximum_array_size = (0x01 << 26);  // 67,108,864 字节
```

数组的最大大小。

### maximum_message_size

```cpp
static constexpr uint32_t maximum_message_size = (0x01 << 27);  // 134,217,728 字节
```

消息的最大大小。

### maximum_message_depth

```cpp
static constexpr uint32_t maximum_message_depth = 64;
```

消息的最大嵌套深度。

## 使用示例

### 验证服务注册

```cpp
bool register_service(mc::dbus::connection& conn, const std::string& service_name) {
    // 验证服务名称
    if (!mc::dbus::validator::is_valid_bus_name(service_name)) {
        std::cerr << "无效的服务名称: " << service_name << std::endl;
        return false;
    }
    
    // 请求服务名称
    auto [success, error] = conn.request_name(service_name);
    return success;
}
```

### 验证方法调用

```cpp
mc::dbus::message create_method_call(
    const std::string& service,
    const std::string& path,
    const std::string& interface,
    const std::string& method) {
    
    // 验证参数
    if (!mc::dbus::validator::is_valid_bus_name(service)) {
        throw std::invalid_argument("无效的服务名称");
    }
    
    if (!mc::dbus::validator::is_valid_path(path)) {
        throw std::invalid_argument("无效的对象路径");
    }
    
    if (!mc::dbus::validator::is_valid_interface_name(interface)) {
        throw std::invalid_argument("无效的接口名称");
    }
    
    if (!mc::dbus::validator::is_valid_member_name(method)) {
        throw std::invalid_argument("无效的方法名称");
    }
    
    // 创建消息
    return mc::dbus::message::new_method_call(service, path, interface, method);
}
```

### 配置验证

```cpp
struct dbus_config {
    std::string service_name;
    std::string object_path;
    std::string interface_name;
    
    bool validate() const {
        if (!mc::dbus::validator::is_valid_bus_name(service_name)) {
            std::cerr << "无效的服务名称" << std::endl;
            return false;
        }
        
        if (!mc::dbus::validator::is_valid_path(object_path)) {
            std::cerr << "无效的对象路径" << std::endl;
            return false;
        }
        
        if (!mc::dbus::validator::is_valid_interface_name(interface_name)) {
            std::cerr << "无效的接口名称" << std::endl;
            return false;
        }
        
        return true;
    }
};

// 使用
dbus_config config{
    "com.example.MyService",
    "/com/example/Object",
    "com.example.Interface"
};

if (config.validate()) {
    // 使用配置
}
```

### 动态对象路径生成

```cpp
std::string generate_object_path(const std::string& base, 
                                 const std::string& id) {
    // 确保基础路径有效
    if (!mc::dbus::validator::is_valid_path(base)) {
        throw std::invalid_argument("无效的基础路径");
    }
    
    // 清理 ID（移除非法字符）
    std::string clean_id;
    for (char c : id) {
        if (std::isalnum(c) || c == '_') {
            clean_id += c;
        } else {
            clean_id += '_';
        }
    }
    
    // 如果以数字开头，添加前缀
    if (!clean_id.empty() && std::isdigit(clean_id[0])) {
        clean_id = "device_" + clean_id;
    }
    
    // 生成完整路径
    std::string path = base + "/" + clean_id;
    
    // 验证生成的路径
    if (!mc::dbus::validator::is_valid_path(path)) {
        throw std::runtime_error("生成的路径无效: " + path);
    }
    
    return path;
}

// 使用
try {
    auto path = generate_object_path("/com/example/devices", "sensor-001");
    // 使用生成的路径
} catch (const std::exception& e) {
    std::cerr << "路径生成失败: " << e.what() << std::endl;
}
```

## 注意事项

1. **提前验证**：在创建消息或注册对象前验证名称，避免运行时错误
2. **清理输入**：对用户输入进行清理，确保符合规范
3. **错误提示**：验证失败时提供清晰的错误消息
4. **性能考虑**：验证函数较快，但避免在循环中重复验证相同的值
5. **唯一名称**：唯一名称（以冒号开头）有特殊格式，通常由总线分配

## 相关接口

- [connection](connection.md) - 连接对象
- [message](message.md) - 消息对象
- [error](error.md) - 错误处理
