# DBus sd_bus API

## 概述

`mc::dbus::sd_bus` 是一个 DBus 客户端封装类，提供了简化的方法调用接口。它封装了连接管理、消息发送、错误处理等底层细节，并提供了以下特性：

- 自动管理连接生命周期
- 支持同步和异步调用
- 自动添加 Requestor 字段到上下文
- 支持 devmon chip 接口的方法映射
- 支持共享内存调用（SHM）
- 超时控制和错误处理

## 构造函数

### sd_bus

```cpp
sd_bus(bool start_now, bool is_blocking);
```

创建 sd_bus 对象。

**参数：**
- `start_now` [in] - 是否立即启动连接。如果为 true，连接会在构造后立即启动；如果为 false，需要手动调用 `connection::start()`
- `is_blocking` [in] - 是否为阻塞模式。如果为 true，使用阻塞式 DBus 调用；如果为 false，优先使用共享内存调用，失败时回退到 DBus 调用

**说明：**
- 构造函数会自动创建会话总线连接
- 在非阻塞模式下，会优先尝试通过共享内存进行调用，如果失败则回退到普通 DBus 调用

**示例：**

```cpp
// 创建非阻塞 sd_bus，立即启动
sd_bus bus(true, false);
bus.request_name("org.example.MyClient");

// 创建阻塞 sd_bus，稍后启动
sd_bus blocking_bus(false, true);
// ... 其他初始化 ...
// blocking_bus 的连接需要手动启动
```

## 方法调用

### call

```cpp
variants call(const method_call_params& params);
```

同步调用 DBus 方法（使用默认超时时间，10 分钟）。

**参数：**
- `params` [in] - 方法调用参数结构，包含：
  - `service_name` - 服务名称，例如 "org.freedesktop.DBus"
  - `path` - 对象路径，例如 "/org/freedesktop/DBus"
  - `interface` - 接口名称，例如 "org.freedesktop.DBus"
  - `method` - 方法名称，例如 "GetId"
  - `signature` - 参数签名，例如 "s" 表示一个字符串参数
  - `args` - 参数列表（引用，避免不必要的复制）

**返回值：**
- 返回方法调用的结果（`variants` 类型，可能包含多个返回值）

**异常：**
- 调用失败时抛出 `mc::exception`
- 超时时抛出相应异常

**说明：**
- 自动在第一个参数（如果是字典）中添加 "Requestor" 字段
- 对于 `bmc.kepler.devmon` 服务，支持接口和方法映射（见下文）
- 返回 `variants`，如果方法返回多个值，可以通过索引访问

**示例：**

```cpp
sd_bus bus(true, false);
bus.request_name("org.example.Client");

// 调用方法，设置值（无返回值）
bus.call({"org.example.Service", "/org/example/Object",
          "org.example.Interface", "SetValue", "i", {42}});

// 调用方法，获取值（单个返回值）
auto results = bus.call({"org.example.Service", "/org/example/Object",
                         "org.example.Interface", "GetValue", "", {}});
if (!results.empty()) {
    int32_t value = results[0].as_int32();
}

// 调用方法，获取多个值
auto results = bus.call({"org.example.Service", "/org/example/Object",
                         "org.example.Interface", "GetValues", "", {}});
if (results.size() >= 2) {
    auto value1 = results[0];
    auto value2 = results[1];
}
```

### pcall

```cpp
pcall_result pcall(const method_call_params& params);
```

同步调用 DBus 方法（使用默认超时时间），返回错误信息而不抛出异常。

**参数：**
- `params` [in] - 方法调用参数结构，同 `call` 方法

**返回值：**
- 返回 `std::tuple<std::optional<std::string>, variants>`
  - 第一个元素：错误信息（如果有错误），否则为 `std::nullopt`
  - 第二个元素：方法调用的结果（`variants` 类型，可能包含多个返回值）

**说明：**
- 不会抛出异常，错误信息通过返回值返回
- 适合需要检查错误但不希望使用异常处理的场景
- 返回 `variants`，如果方法返回多个值，可以通过索引访问

**示例：**

```cpp
auto [error, results] = bus.pcall({"org.example.Service", "/org/example/Object",
                                   "org.example.Interface", "GetValue", "", {}});

if (error.has_value()) {
    std::cerr << "调用失败: " << error.value() << std::endl;
} else {
    if (!results.empty()) {
        int32_t value = results[0].as_int32();
        std::cout << "值: " << value << std::endl;
    }
}
```

### timeout_call

```cpp
variants timeout_call(mc::milliseconds timeout, const method_call_params& params);
```

带超时时间的同步调用 DBus 方法（抛出异常）。

**参数：**
- `timeout` [in] - 超时时间
- `params` [in] - 方法调用参数结构，同 `call` 方法

**返回值：**
- 返回方法调用的结果（`variants` 类型，可能包含多个返回值）

**异常：**
- 调用失败时抛出 `mc::exception`
- 超时时抛出相应异常

**说明：**
- 如果调用时间超过 30 秒，会记录警告日志
- 自动在第一个参数（如果是字典）中添加 "Requestor" 字段
- 返回 `variants`，如果方法返回多个值，可以通过索引访问

**示例：**

```cpp
// 使用 5 秒超时
auto results = bus.timeout_call(mc::seconds(5),
                                {"org.example.Service", "/org/example/Object",
                                 "org.example.Interface", "GetValue", "", {}});
if (!results.empty()) {
    int32_t value = results[0].as_int32();
}

// 使用 3 秒超时调用可能耗时的方法
try {
    auto results = bus.timeout_call(mc::seconds(3),
                                     {"org.example.Service", "/org/example/Object",
                                      "org.example.Interface", "LongRunningMethod", "", {}});
} catch (const mc::exception& e) {
    std::cerr << "调用超时或失败: " << e.what() << std::endl;
}
```

### timeout_pcall

```cpp
pcall_result timeout_pcall(mc::milliseconds timeout, const method_call_params& params);
```

带超时时间的同步调用 DBus 方法（返回错误信息而不抛出异常）。

**参数：**
- `timeout` [in] - 超时时间
- `params` [in] - 方法调用参数结构，同 `call` 方法

**返回值：**
- 同 `pcall` 方法，返回 `std::tuple<std::optional<std::string>, variants>`

**说明：**
- 不会抛出异常，错误信息通过返回值返回
- 如果调用时间超过 30 秒，会记录警告日志
- 返回 `variants`，如果方法返回多个值，可以通过索引访问

**示例：**

```cpp
auto [error, results] = bus.timeout_pcall(mc::seconds(5),
                                          {"org.example.Service", "/org/example/Object",
                                           "org.example.Interface", "GetValue", "", {}});

if (error.has_value()) {
    std::cerr << "调用失败: " << error.value() << std::endl;
} else {
    if (!results.empty()) {
        // 处理结果
        int32_t value = results[0].as_int32();
    }
}
```

### shm_timeout_call

```cpp
std::optional<variants> shm_timeout_call(mc::milliseconds timeout, const method_call_params& params);
```

通过共享内存调用 DBus 方法。

**参数：**
- `timeout` [in] - 超时时间
- `params` [in] - 方法调用参数结构，同 `call` 方法

**返回值：**
- 如果调用成功，返回 `std::optional<variants>`，包含结果（可能包含多个返回值）
- 如果调用失败或共享内存不可用，返回 `std::nullopt`

**说明：**
- 仅通过共享内存进行调用，不会回退到普通 DBus 调用
- 适合需要高性能调用的场景
- 如果服务未注册到共享内存 harbor，调用会失败
- 返回 `variants`，如果方法返回多个值，可以通过索引访问

**示例：**

```cpp
auto result_opt = bus.shm_timeout_call(mc::seconds(30),
                                       {"org.example.Service", "/org/example/Object",
                                        "org.example.Interface", "GetValue", "", {}});

if (result_opt.has_value()) {
    auto& results = result_opt.value();
    if (!results.empty()) {
        int32_t value = results[0].as_int32();
    }
} else {
    // 共享内存调用失败，可能需要使用普通调用
    auto results = bus.timeout_call(mc::seconds(30), {...});
}
```

## 服务名称管理

### request_name

```cpp
void request_name(std::string_view service_name, uint32_t flags = 0);
```

请求 DBus 服务名称。

**参数：**
- `service_name` [in] - 要请求的服务名称
- `flags` [in] - 标志位，默认为 0

**说明：**
- 请求名称后，会自动注册到共享内存 harbor（如果可用）
- 请求的名称会用于自动填充 "Requestor" 字段

**示例：**

```cpp
sd_bus bus(true, false);
bus.request_name("org.example.MyClient");
```

## 配置

### set_enable_local_request

```cpp
void set_enable_local_request(bool enable);
```

设置是否启用本地请求。

**参数：**
- `enable` [in] - 是否启用本地请求

**说明：**
- 当前版本中此功能已注释，保留接口以便将来使用

## 特殊功能

### 自动添加 Requestor 字段

`sd_bus` 会自动在方法调用的第一个参数（如果是字典类型）中添加 "Requestor" 字段，值为通过 `request_name()` 设置的服务名称。

**示例：**

```cpp
sd_bus bus(true, false);
bus.request_name("org.example.Client");

// 第一个参数是字典，会自动添加 Requestor 字段
mc::dict context;
context["Key"] = "Value";
auto result = bus.call({"org.example.Service", "/org/example/Object",
                       "org.example.Interface", "Method", "a{ss}", {context}});

// context 现在包含 "Requestor": "org.example.Client"
```

**注意：**
- 如果第一个参数不是字典，不会添加 Requestor 字段
- 如果第一个参数已经是字典且包含 "Requestor" 字段，不会覆盖现有值

### devmon Chip 接口方法映射

对于 `bmc.kepler.devmon` 服务，`sd_bus` 支持将 `bmc.kepler.Chip.BitIO` 和 `bmc.kepler.Chip.BlockIO` 接口的方法映射到 `bmc.dev.Chip` 接口。

**支持的映射：**

| 原接口 | 原方法 | 目标接口 | 目标方法 | 签名 |
|--------|--------|----------|----------|------|
| `bmc.kepler.Chip.BitIO` | `Read` | `bmc.dev.Chip` | `BitIORead` | `uyu` |
| `bmc.kepler.Chip.BitIO` | `Write` | `bmc.dev.Chip` | `BitIOWrite` | `uyuay` |
| `bmc.kepler.Chip.BlockIO` | `Read` | `bmc.dev.Chip` | `BlockIORead` | `uu` |
| `bmc.kepler.Chip.BlockIO` | `Write` | `bmc.dev.Chip` | `BlockIOWrite` | `uay` |
| `bmc.kepler.Chip.BlockIO` | `WriteRead` | `bmc.dev.Chip` | `BlockIOWriteRead` | `ayu` |
| `bmc.kepler.Chip.BlockIO` | `ComboWriteRead` | `bmc.dev.Chip` | `BlockIOComboWriteRead` | `uayuu` |

**说明：**
- 映射时会自动移除第一个参数（上下文字典）
- 如果方法不在映射表中，会使用原始接口和方法进行调用

**示例：**

```cpp
// 调用 bmc.kepler.Chip.BitIO.Read，会自动映射到 bmc.dev.Chip.BitIORead
auto results = bus.call({"bmc.kepler.devmon", "/bmc/dev/Chip",
                         "bmc.kepler.Chip.BitIO", "Read", "a{ss}uyu",
                         {mc::dict(), 0x1234, 0x5678, 0x9ABC}});

// 实际调用的是 bmc.dev.Chip.BitIORead，参数为 (0x1234, 0x5678, 0x9ABC)
// 结果在 results 中
```

## 调用流程

### 非阻塞模式（is_blocking = false）

1. 首先尝试通过共享内存调用（`shm_timeout_call`）
2. 如果共享内存调用失败，回退到普通 DBus 调用（`dbus_call`）

### 阻塞模式（is_blocking = true）

1. 直接使用普通 DBus 调用（`dbus_call`）

## 超时和日志

- 默认超时时间：10 分钟（`DEFAULT_DBUS_CALL_TIMEOUT`）
- 慢调用阈值：30 秒（`CALL_TIMEOUT_SECONDS`）
- 如果调用时间超过 30 秒，会记录警告日志，包含：
  - 调用者（从 Requestor 字段获取）
  - 远程服务名称
  - 对象路径
  - 接口名称
  - 方法名称
  - 耗时（秒）

## 使用示例

### 基本使用

```cpp
#include <mc/dbus/sd_bus.h>

// 创建 sd_bus 对象
mc::dbus::sd_bus bus(true, false);
bus.request_name("org.example.MyClient");

// 调用方法
auto result = bus.call("org.example.Service", "/org/example/Object",
                       "org.example.Interface", "GetValue", "", {});

if (result.is_int32()) {
    int32_t value = result.as_int32();
    std::cout << "值: " << value << std::endl;
}
```

### 错误处理

```cpp
// 方式 1：使用异常处理
try {
    auto results = bus.call({"org.example.Service", "/org/example/Object",
                             "org.example.Interface", "Method", "", {}});
    // 处理结果
    if (!results.empty()) {
        // 使用 results[0], results[1], ...
    }
} catch (const mc::exception& e) {
    std::cerr << "调用失败: " << e.what() << std::endl;
}

// 方式 2：使用 pcall（不抛出异常）
auto [error, results] = bus.pcall({"org.example.Service", "/org/example/Object",
                                    "org.example.Interface", "Method", "", {}});
if (error.has_value()) {
    std::cerr << "调用失败: " << error.value() << std::endl;
} else {
    // 处理结果
    if (!results.empty()) {
        // 使用 results[0], results[1], ...
    }
}
```

### 带超时的调用

```cpp
// 使用 5 秒超时
try {
    auto results = bus.timeout_call(mc::seconds(5),
                                     {"org.example.Service", "/org/example/Object",
                                      "org.example.Interface", "Method", "", {}});
    // 处理结果
    if (!results.empty()) {
        // 使用 results[0], results[1], ...
    }
} catch (const mc::exception& e) {
    std::cerr << "调用超时或失败: " << e.what() << std::endl;
}
```

### 使用上下文参数

```cpp
// 创建上下文
mc::dict context;
context["Key1"] = "Value1";
context["Key2"] = "Value2";

// 调用方法，Requestor 会自动添加
auto results = bus.call({"org.example.Service", "/org/example/Object",
                         "org.example.Interface", "Method", "a{ss}", {context}});

// context 现在包含 "Requestor": "org.example.MyClient"
```

### 共享内存调用

```cpp
// 尝试共享内存调用
auto result_opt = bus.shm_timeout_call(mc::seconds(30),
                                       {"org.example.Service", "/org/example/Object",
                                        "org.example.Interface", "Method", "", {}});

if (result_opt.has_value()) {
    // 共享内存调用成功
    auto& results = result_opt.value();
    if (!results.empty()) {
        // 使用 results[0], results[1], ...
    }
} else {
    // 共享内存调用失败，使用普通调用
    auto results = bus.timeout_call(mc::seconds(30), {"org.example.Service", "/org/example/Object",
                                                       "org.example.Interface", "Method", "", {}});
}
```

## 类型别名

```cpp
using pcall_result = std::tuple<std::optional<std::string>, variants>;
```

`pcall_result` 是 `pcall` 和 `timeout_pcall` 的返回类型：
- 第一个元素：错误信息（如果有），否则为 `std::nullopt`
- 第二个元素：方法调用的结果（`variants` 类型，可能包含多个返回值）

## 结构体

### method_call_params

```cpp
struct method_call_params {
    std::string_view service_name;
    std::string_view path;
    std::string_view interface;
    std::string_view method;
    std::string_view signature;
    const variants& args;
};
```

方法调用参数结构，用于封装所有方法调用参数，简化函数签名。

**字段：**
- `service_name` - 服务名称
- `path` - 对象路径
- `interface` - 接口名称
- `method` - 方法名称
- `signature` - 参数签名
- `args` - 参数列表（引用，避免不必要的复制）

**使用方式：**
- 可以使用聚合初始化：`{service_name, path, interface, method, signature, args}`
- 所有方法调用函数都接受此结构作为参数

## 注意事项

1. **生命周期管理**：确保 `sd_bus` 对象在使用期间保持有效
2. **线程安全**：`sd_bus` 不是线程安全的，应在单线程中使用
3. **异常处理**：`call` 和 `timeout_call` 会抛出异常，注意捕获
4. **超时设置**：合理设置超时时间，避免长时间阻塞
5. **Requestor 字段**：自动添加的 Requestor 字段不会覆盖已存在的值
6. **共享内存**：共享内存调用需要服务注册到 harbor，否则会失败

## 相关文档

- [connection.md](connection.md) - DBus 连接管理
- [message.md](message.md) - DBus 消息处理
- [error.md](error.md) - DBus 错误处理
- [shm.md](shm.md) - 共享内存通信

## 测试

单元测试位于 `tests/dbus/test_sd_bus.cpp`，包含以下测试用例：

- `test_valid_args_call` - 测试有效参数调用
- `test_invalid_args_call` - 测试无效参数调用
- `test_blocking_bus_call` - 测试阻塞式调用
- `test_call_timeout` - 测试超时调用
- `test_devmon_chip_methods` - 测试 devmon chip 方法映射
- `test_context_requestor` - 测试 Requestor 字段自动添加
- `test_protected_call` - 测试异常处理调用
- `test_unprotected_call` - 测试无异常处理调用
- `test_shm_call` - 测试共享内存调用（条件编译）
- `test_shm_call_request_size_over_limit` - 测试共享内存调用大小限制（条件编译）
