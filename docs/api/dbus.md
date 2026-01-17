# DBus API文档

## 概述

`mc::dbus` 是一个完整的 DBus（D-Bus）通信库，提供了与系统和会话总线的交互能力。它支持同步和异步消息传递、信号监听、共享内存通信等高级特性。

## 主要特性

- **连接管理**：支持系统总线、会话总线和自定义地址连接
- **消息传递**：同步和异步的方法调用、信号发送
- **消息匹配**：灵活的消息过滤和匹配规则
- **共享内存通信**：高性能的进程间通信方式
- **类型安全**：基于 C++ 模板的类型转换系统
- **异常处理**：完善的错误处理机制

## 核心组件

### 连接与消息

- [connection](dbus/connection.md) - DBus连接对象
- [message](dbus/message.md) - DBus消息对象
- [error](dbus/error.md) - 错误处理

### 消息匹配与信号

- [match](dbus/match.md) - 消息匹配规则
- [signal](dbus/signal.md) - 信号发射

### 验证与枚举

- [validator](dbus/validator.md) - 名称和消息验证
- [enums](dbus/enums.md) - DBus枚举类型

### 共享内存通信

- [shm](dbus/shm.md) - 共享内存通信综合文档

## 快速开始

### 创建连接

```cpp
#include <mc/dbus/connection.h>
#include <mc/runtime.h>

// 创建 IO 上下文
mc::io_context executor(1);

// 连接到系统总线
auto conn = mc::dbus::connection::open_system_bus(executor);

// 启动连接
conn.start();
```

### 发送方法调用

```cpp
#include <mc/dbus/message.h>

// 创建方法调用消息
auto msg = mc::dbus::message::new_method_call(
    "org.freedesktop.DBus",      // 目标服务
    "/org/freedesktop/DBus",     // 对象路径
    "org.freedesktop.DBus",      // 接口名称
    "ListNames"                  // 方法名称
);

// 同步发送并等待回复
auto reply = conn.send_with_reply_and_block(std::move(msg));

// 读取返回值
auto names = reply.as<std::vector<std::string>>();
```

### 异步方法调用

```cpp
// 异步发送消息
auto future = conn.async_send_with_reply(std::move(msg));

// 处理响应
future.then([](mc::dbus::message reply) {
    auto names = reply.as<std::vector<std::string>>();
    // 处理结果
});
```

### 监听信号

```cpp
#include <mc/dbus/match.h>

// 创建信号匹配规则
auto rule = mc::dbus::match_rule::new_signal(
    "NameOwnerChanged",           // 信号名称
    "org.freedesktop.DBus"        // 接口名称
);

// 添加匹配规则和回调
conn.add_match(rule, [](mc::dbus::message& msg) {
    // 处理信号消息
    std::cout << "收到信号: " << msg.get_member() << std::endl;
}, 1);
```

### 写入和读取消息参数

```cpp
// 创建消息
auto msg = mc::dbus::message::new_method_call(
    "com.example.Service",
    "/com/example/Object",
    "com.example.Interface",
    "DoSomething"
);

// 写入参数
auto writer = msg.writer();
writer << "hello" << 42 << true;

// 发送并接收回复
auto reply = conn.send_with_reply_and_block(std::move(msg));

// 读取返回值
auto reader = reply.reader();
std::string result;
int code;
reader >> result >> code;
```

## 类型系统

### 基本类型映射

DBus类型与C++类型的对应关系：

| DBus类型 | C++类型 | 类型码 |
|---------|---------|--------|
| BYTE | uint8_t | 'y' |
| BOOLEAN | bool | 'b' |
| INT16 | int16_t | 'n' |
| UINT16 | uint16_t | 'q' |
| INT32 | int32_t | 'i' |
| UINT32 | uint32_t | 'u' |
| INT64 | int64_t | 'x' |
| UINT64 | uint64_t | 't' |
| DOUBLE | double | 'd' |
| STRING | std::string | 's' |
| OBJECT_PATH | mc::engine::path | 'o' |
| SIGNATURE | mc::reflect::signature | 'g' |

### 容器类型

| DBus类型 | C++类型 | 类型签名 |
|---------|---------|----------|
| ARRAY | std::vector<T> | 'a' + 元素类型 |
| STRUCT | std::tuple<T...> | '(' + 元素类型 + ')' |
| DICT_ENTRY | std::pair<K,V> | '{' + 键类型 + 值类型 + '}' |
| VARIANT | mc::variant | 'v' |

### 使用反射类型

```cpp
// 定义结构体
struct person {
    std::string name;
    int age;
    bool is_student;
};

// 注册反射
MC_REFLECT(person, name, age, is_student);

// 直接发送结构体
person p{"张三", 30, false};
auto msg = mc::dbus::message::new_method_call(...);
msg.writer() << p;

// 接收结构体
auto reply = conn.send_with_reply_and_block(std::move(msg));
person result;
reply.reader() >> result;
```

## 错误处理

### 检查错误

```cpp
// 使用元组返回值
auto [success, error] = conn.request_name("com.example.Service");
if (!success) {
    std::cerr << "请求名称失败: " << error->message << std::endl;
}

// 处理异常
try {
    auto reply = conn.send_with_reply_and_block(std::move(msg));
} catch (const mc::system_exception& e) {
    std::cerr << "发送失败: " << e.what() << std::endl;
}
```

### 发送错误响应

```cpp
// 创建错误消息
auto error_msg = mc::dbus::message::new_error(
    original_msg,
    "com.example.Error.Failed",
    "操作失败"
);

// 发送错误响应
conn.send(std::move(error_msg));
```

## 共享内存通信

对于高性能场景，可以使用共享内存通信方式：

```cpp
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/shm_tree.h>

// 获取港口实例
auto& harbor = mc::dbus::harbor::get_instance();
harbor.set_harbor_name("my_service");
harbor.start();

// 创建共享内存对象树
mc::dbus::shm_tree tree("my_service", "com.example.Service", ":1.123");

// 注册对象
tree.register_object(my_object);

// 调用共享内存方法
auto result = tree.timeout_call(
    mc::seconds(5),
    "com.example.Service",
    "/com/example/Object",
    "com.example.Interface",
    "DoSomething",
    "si",  // 签名: string, int
    mc::variants{"hello", 42}
);
```

## 最佳实践

### 1. 资源管理

- 使用 RAII 模式管理连接生命周期
- 及时移除不需要的匹配规则
- 在析构时断开连接

```cpp
class my_service {
public:
    my_service(mc::io_context& ctx) 
        : m_conn(mc::dbus::connection::open_system_bus(ctx)) {
        m_conn.start();
    }
    
    ~my_service() {
        m_conn.disconnect();
    }
    
private:
    mc::dbus::connection m_conn;
};
```

### 2. 异步处理

- 对于耗时操作，使用异步方法避免阻塞
- 合理设置超时时间
- 使用 future 处理异步结果

```cpp
// 批量异步调用
std::vector<mc::future<mc::dbus::message>> futures;
for (const auto& service : services) {
    auto msg = create_message(service);
    futures.push_back(conn.async_send_with_reply(std::move(msg)));
}

// 等待所有结果
for (auto& f : futures) {
    auto reply = f.get();
    // 处理回复
}
```

### 3. 错误处理

- 始终检查错误返回值
- 使用 try-catch 捕获异常
- 提供清晰的错误消息

```cpp
try {
    auto reply = conn.send_with_reply_and_block(
        std::move(msg), 
        mc::seconds(30)  // 设置合理的超时
    );
    
    if (reply.is_error()) {
        std::cerr << "收到错误响应: " 
                  << reply.get_error_message() << std::endl;
        return;
    }
    
    // 处理正常响应
} catch (const mc::timeout_exception& e) {
    std::cerr << "超时: " << e.what() << std::endl;
} catch (const mc::system_exception& e) {
    std::cerr << "系统错误: " << e.what() << std::endl;
}
```

### 4. 性能优化

- 对于频繁调用的场景，考虑使用共享内存通信
- 批量处理消息而不是逐个处理
- 重用消息对象避免频繁创建销毁
- 合理设置消息队列大小

```cpp
// 使用共享内存加速本地调用
if (is_local_service(service_name)) {
    // 使用共享内存调用
    auto result = shm_tree.timeout_call(...);
} else {
    // 使用 DBus 调用
    auto result = conn.send_with_reply_and_block(...);
}
```

## 注意事项

1. **线程安全**：`connection` 对象不是线程安全的，需要外部同步
2. **消息生命周期**：发送消息后，原消息对象会被移动，不能再使用
3. **匹配规则**：记得在不需要时移除匹配规则，避免内存泄漏
4. **超时设置**：合理设置超时时间，避免无限等待
5. **名称验证**：使用 `validator` 验证 DBus 名称的合法性

## 相关资源

- [DBus 规范](https://dbus.freedesktop.org/doc/dbus-specification.html)
- [DBus 教程](https://dbus.freedesktop.org/doc/dbus-tutorial.html)
- [libmcpp dbus 详细设计](../9.dbus.md)

## 示例代码

完整的示例代码请参考：
- `examples/dbus/` - 基础 DBus 使用示例
- `examples/interprocess/` - 进程间通信示例
- `tests/dbus/` - 单元测试示例
