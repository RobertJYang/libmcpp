# DBus 共享内存通信 API

## 概述

DBus 共享内存通信是一个高性能的进程间通信方案，通过共享内存传递消息，避免了传统 DBus 的序列化开销和内核复制开销。

## 架构组件

### 核心组件

- **harbor** - 共享内存通信港口，管理消息队列和服务注册
- **shm_tree** - 共享内存对象树，管理对象和接口
- **local_msg** - 本地消息对象，用于共享内存通信
- **serialize** - 消息序列化/反序列化
- **gvariant_convert** - GVariant 与 mc::variant 的转换

### 工作原理

```
┌─────────────┐         ┌─────────────┐
│  Service A  │         │  Service B  │
│             │         │             │
│  shm_tree   │         │  shm_tree   │
└──────┬──────┘         └──────┬──────┘
       │                       │
       │    ┌──────────┐       │
       └────┤  Harbor  ├───────┘
            │          │
            │ 消息队列  │
            │ 对象注册  │
            └──────────┘
                 │
          ┌──────┴──────┐
          │ 共享内存区域 │
          └─────────────┘
```

## Harbor API

### 获取实例

```cpp
#include <mc/dbus/shm/harbor.h>

auto& harbor = mc::dbus::harbor::get_instance();
```

### 配置和启动

```cpp
// 设置港口名称
harbor.set_harbor_name("my_service");

// 启动港口
harbor.start();

// 停止港口
harbor.stop();
```

### 服务注册

```cpp
// 注册唯一名称
harbor.register_unique_name(":1.123", "com.example.MyService");

// 获取唯一名称
std::string unique_name = harbor.get_unique_name("com.example.MyService");

// 注销服务
harbor.unregister_service("com.example.MyService");
```

### 方法处理

```cpp
// 注册方法处理器
harbor.register_method_handler(
    "com.example.MyService",  // 服务名称
    ":1.123",                 // 唯一名称
    [](std::string_view path, 
       std::string_view interface,
       std::string_view method,
       const mc::variants& args) -> invoke_result {
        
        // 处理方法调用
        if (method == "GetStatus") {
            return {nullptr, mc::variants{"running"}};
        }
        
        return {nullptr, mc::variants{}};
    }
);
```

## SHM Tree API

### 创建对象树

```cpp
#include <mc/dbus/shm/shm_tree.h>

mc::dbus::shm_tree tree(
    "my_service",              // 港口名称
    "com.example.MyService",   // 服务名称
    ":1.123"                   // 唯一名称
);
```

### 注册对象

```cpp
// 定义对象
class my_object : public mc::engine::abstract_object {
public:
    // 实现接口...
};

my_object obj;

// 注册到共享内存
tree.register_object(obj);

// 注销对象
tree.unregister_object("/com/example/Object");
```

### 调用方法

```cpp
// 同步调用（带超时）
auto result = tree.timeout_call(
    mc::seconds(5),                 // 超时时间
    "com.example.Service",          // 服务名称
    "/com/example/Object",          // 对象路径
    "com.example.Interface",        // 接口名称
    "DoSomething",                  // 方法名称
    "si",                           // 参数签名
    mc::variants{"hello", 42}       // 参数
);

if (result.has_value()) {
    auto& values = result.value();
    // 处理结果
}
```

### 属性操作

```cpp
// 获取属性
auto value = mc::dbus::shm_tree::get_property(
    "com.example.Service",
    "/com/example/Object",
    "com.example.Interface",
    "Status"
);

// 设置属性
mc::dbus::shm_tree::set_property(
    "com.example.Service",
    "/com/example/Object",
    "com.example.Interface",
    "Status",
    mc::variant("running")
);
```

## Local Message API

### 创建消息

```cpp
#include <mc/dbus/shm/local_msg.h>

// 创建方法调用消息
mc::dbus::local_msg msg(
    "com.example.Service",      // 服务名称
    "/com/example/Object",      // 对象路径
    "com.example.Interface",    // 接口名称
    "GetStatus"                 // 方法名称
);
```

### 写入参数

```cpp
// 追加参数
msg.append("si", "hello", 42);

// 或使用模板方法
msg.append<std::string, int>("si", "hello", 42);
```

### 读取参数

```cpp
// 读取参数
const auto& args = msg.read();
std::string str_arg = args[0].as<std::string>();
int int_arg = args[1].as<int>();
```

### 错误处理

```cpp
// 设置错误
msg.error("com.example.Error.Failed", "操作失败");

// 获取错误
auto [error_name, error_msg] = msg.get_error();
```

### 序列化

```cpp
// 序列化消息
std::string packed = msg.pack();

// 反序列化消息
mc::dbus::local_msg msg2(unpack_variants(packed));
```

## Serialize API

### 序列化

```cpp
#include <mc/dbus/shm/serialize.h>

mc::variants args = {"hello", 42, true};

// 序列化
std::string packed = mc::dbus::serialize::pack(args);
```

### 反序列化

```cpp
// 反序列化
mc::variants result = mc::dbus::serialize::unpack(packed);

// 或使用 deserialize（功能相同）
result = mc::dbus::serialize::deserialize(packed);
```

## GVariant Convert API

### 转换到 mc::variant

```cpp
#include <mc/dbus/shm/gvariant_convert.h>

GVariant* gv = ...;  // 来自某个 GLib 函数

// 转换为 mc::variant
mc::variant v = mc::dbus::gvariant_convert::to_mc_variant(gv);
```

### 转换到 GVariant

```cpp
mc::variant v = "hello";

// 自动推断类型
GVariant* gv1 = mc::dbus::gvariant_convert::to_gvariant(v);

// 指定类型签名
GVariant* gv2 = mc::dbus::gvariant_convert::to_gvariant(v, "s");
```

## 完整示例

### 服务端

```cpp
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/engine.h>

class my_service_object : public mc::engine::abstract_object {
public:
    // 实现接口方法
    std::string get_status() {
        return "running";
    }
    
    void set_value(int value) {
        m_value = value;
    }
    
private:
    int m_value = 0;
};

int main() {
    // 初始化港口
    auto& harbor = mc::dbus::harbor::get_instance();
    harbor.set_harbor_name("my_service");
    
    // 创建对象树
    mc::dbus::shm_tree tree(
        "my_service",
        "com.example.MyService",
        ":1.100"
    );
    
    // 注册服务名称
    harbor.register_unique_name(":1.100", "com.example.MyService");
    
    // 创建并注册对象
    my_service_object obj;
    tree.register_object(obj);
    
    // 启动服务
    harbor.start();
    
    // 运行事件循环...
    
    // 清理
    tree.unregister_object(obj.get_path());
    harbor.stop();
    
    return 0;
}
```

### 客户端

```cpp
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/shm_tree.h>

int main() {
    // 初始化港口
    auto& harbor = mc::dbus::harbor::get_instance();
    harbor.set_harbor_name("my_client");
    
    // 创建对象树
    mc::dbus::shm_tree tree(
        "my_client",
        "com.example.MyClient",
        ":1.101"
    );
    
    // 调用远程方法
    auto result = tree.timeout_call(
        mc::seconds(5),
        "com.example.MyService",
        "/com/example/Object",
        "com.example.Interface",
        "GetStatus",
        "",  // 无参数
        mc::variants{}
    );
    
    if (result.has_value()) {
        auto& values = result.value();
        std::string status = values[0].as<std::string>();
        std::cout << "状态: " << status << std::endl;
    }
    
    // 获取属性
    auto value = mc::dbus::shm_tree::get_property(
        "com.example.MyService",
        "/com/example/Object",
        "com.example.Interface",
        "Status"
    );
    std::cout << "属性值: " << value.as<std::string>() << std::endl;
    
    return 0;
}
```

## 性能优势

### 与传统 DBus 比较

| 特性 | 传统 DBus | 共享内存 DBus |
|------|-----------|--------------|
| 消息传递 | 内核拷贝 | 共享内存 |
| 序列化 | DBus 格式 | 优化二进制 |
| 延迟 | ~100μs | ~10μs |
| 吞吐量 | ~10MB/s | ~100MB/s |
| CPU 使用 | 较高 | 较低 |

### 适用场景

**推荐使用共享内存：**
- 高频调用（>1000次/秒）
- 大数据传输（>1KB）
- 同机器进程通信
- 性能敏感场景

**使用传统 DBus：**
- 跨机器通信
- 低频调用
- 需要系统总线特性
- 兼容性要求

## 注意事项

1. **内存管理**：共享内存需要合理管理，避免内存泄漏
2. **同步机制**：使用锁保证数据一致性
3. **进程生命周期**：确保港口在所有使用者之前创建
4. **错误处理**：共享内存操作可能失败，需要完善错误处理
5. **性能调优**：根据实际场景调整消息队列大小和超时时间

## 相关接口

- [connection](connection.md) - 传统 DBus 连接
- [message](message.md) - DBus 消息对象
- [dbus](../dbus.md) - DBus 主文档
