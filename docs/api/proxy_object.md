# Proxy Object API 文档

## 概述

`proxy_object` 类提供了 D-Bus 对象的代理封装，允许通过 C++ 接口访问 D-Bus 对象的属性和方法。该类封装了 D-Bus 通信细节，提供了类型安全的属性访问和方法调用接口。

## 类定义

```cpp
class proxy_object {
public:
    // 构造函数
    proxy_object(mc::dbus::sd_bus* bus, ...);
    proxy_object(std::unique_ptr<mc::dbus::sd_bus> bus, ...);

    // 接口信息查询
    const interface_info& get_interface_info() const;
    bool has_property(const std::string& name) const;
    bool has_method(const std::string& name) const;
    const property_info* get_property_info(const std::string& name) const;
    const method_info* get_method_info(const std::string& name) const;

    // 属性操作
    mc::variant get_property(const std::string& name);
    void set_property(const std::string& name, const mc::variant& value);
    mc::dict get_all_properties();
    std::pair<mc::dict, mc::dict> get_properties(const mc::variants& prop_names);

    // 方法调用
    mc::variants call_method(const std::string& name, const mc::variant& args);
    mc::variants call_method_pcall(const std::string& name, const mc::variant& args,
                                   std::optional<std::string>& error);

    // 易变属性检查
    bool is_volatile(const std::string& name) const;

    // 元数据访问
    mc::dbus::connection& bus();
    const std::string& service() const;
    const std::string& path() const;
    const std::string& interface() const;
};
```

---

## 构造函数

### 构造函数 1：接收指针

```cpp
proxy_object(
    mc::dbus::sd_bus*     bus,
    std::string           service,
    std::string           path,
    std::string           interface,
    const interface_info& iface_info
);
```

**参数：**
- `bus`: D-Bus 连接对象指针（生命周期由调用者管理）
- `service`: 服务名称，例如 `"org.example.Service"`
- `path`: 对象路径，例如 `"/org/example/Object"`
- `interface`: 接口名称，例如 `"org.example.Interface"`
- `iface_info`: 接口信息（从 `introspection_cache` 获取）

**说明：**
- 该构造函数会创建 `sd_bus` 的拷贝
- 适用于 `mdb_access::get_object_by_short_call` 场景

---

### 构造函数 2：接收 unique_ptr

```cpp
proxy_object(
    std::unique_ptr<mc::dbus::sd_bus> bus,
    std::string                       service,
    std::string                       path,
    std::string                       interface,
    const interface_info&             iface_info
);
```

**参数：**
- `bus`: D-Bus 连接对象的 `unique_ptr`（所有权转移）
- `service`: 服务名称
- `path`: 对象路径
- `interface`: 接口名称
- `iface_info`: 接口信息

**说明：**
- 该构造函数接收 `unique_ptr`，拥有 `sd_bus` 的所有权
- 适用于 `mdb_access::get_object` 场景

---

## 接口信息查询

### get_interface_info

获取接口信息。

```cpp
const interface_info& get_interface_info() const;
```

**返回值：**
- `const interface_info&`: 接口信息引用

**说明：**
- 返回接口的完整信息，包括所有方法和属性

---

### has_property

检查属性是否存在。

```cpp
bool has_property(const std::string& name) const;
```

**参数：**
- `name`: 属性名称

**返回值：**
- `bool`: 如果属性存在返回 `true`，否则返回 `false`

---

### has_method

检查方法是否存在。

```cpp
bool has_method(const std::string& name) const;
```

**参数：**
- `name`: 方法名称

**返回值：**
- `bool`: 如果方法存在返回 `true`，否则返回 `false`

---

### get_property_info

获取属性信息。

```cpp
const property_info* get_property_info(const std::string& name) const;
```

**参数：**
- `name`: 属性名称

**返回值：**
- `const property_info*`: 属性信息指针，如果属性不存在返回 `nullptr`

---

### get_method_info

获取方法信息。

```cpp
const method_info* get_method_info(const std::string& name) const;
```

**参数：**
- `name`: 方法名称

**返回值：**
- `const method_info*`: 方法信息指针，如果方法不存在返回 `nullptr`

---

## 属性操作

### get_property

获取单个属性值。

```cpp
mc::variant get_property(const std::string& name);
```

**参数：**
- `name`: 属性名称

**返回值：**
- `mc::variant`: 属性值

**说明：**
- 通过 D-Bus 调用 `GetWithContext` 方法获取属性值
- 自动获取当前上下文（context）
- 如果属性不存在，抛出异常

**异常：**
- 如果属性不存在，抛出 `mc::exception`

**示例：**
```cpp
proxy_object* obj = ...;
mc::variant value = obj->get_property("Name");
std::string name = value.as_string();
```

---

### set_property

设置属性值。

```cpp
void set_property(const std::string& name, const mc::variant& value);
```

**参数：**
- `name`: 属性名称
- `value`: 属性值

**说明：**
- 通过 D-Bus 调用 `SetWithContext` 方法设置属性值
- 自动进行类型验证（使用 `mdb_validator`）
- 自动获取当前上下文（context）
- 如果属性不存在或不可写，抛出异常

**异常：**
- 如果属性不存在，抛出 `mc::exception`
- 如果属性不可写，抛出 `mc::exception`
- 如果类型不匹配，抛出 `mc::exception`

**示例：**
```cpp
proxy_object* obj = ...;
obj->set_property("Enabled", mc::variant(true));
obj->set_property("Name", mc::variant("NewName"));
```

---

### get_all_properties

获取所有属性值。

```cpp
mc::dict get_all_properties();
```

**返回值：**
- `mc::dict`: 包含所有属性的字典，键为属性名，值为属性值

**说明：**
- 通过 D-Bus 调用 `GetAllWithContext` 方法获取所有属性
- 自动获取当前上下文（context）

**示例：**
```cpp
proxy_object* obj = ...;
mc::dict props = obj->get_all_properties();
for (const auto& entry : props) {
    std::cout << entry.key.as_string() << " = " << entry.value << std::endl;
}
```

---

### get_properties

批量获取指定属性值。

```cpp
std::pair<mc::dict, mc::dict> get_properties(const mc::variants& prop_names);
```

**参数：**
- `prop_names`: 属性名列表（`mc::variants`，每个元素是字符串）

**返回值：**
- `std::pair<mc::dict, mc::dict>`: 
  - 第一个字典：属性数据字典，键为属性名，值为属性值
  - 第二个字典：错误信息字典，键为属性名，值为错误信息（如果属性获取失败）

**说明：**
- 通过 D-Bus 调用 `GetPropertiesByNames` 方法批量获取属性
- 自动获取当前上下文（context）
- 如果某个属性获取失败，会在错误字典中记录，不会抛出异常

**示例：**
```cpp
proxy_object* obj = ...;
mc::variants prop_names = {
    mc::variant("Id"),
    mc::variant("Name"),
    mc::variant("Enabled")
};
auto [props, errs] = obj->get_properties(prop_names);

// 检查属性
if (props.contains("Id")) {
    std::cout << "Id: " << props["Id"] << std::endl;
}

// 检查错误
if (errs.contains("Name")) {
    std::cerr << "Error getting Name: " << errs["Name"].as_string() << std::endl;
}
```

---

## 方法调用

### call_method

调用 D-Bus 方法。

```cpp
mc::variants call_method(const std::string& name, const mc::variant& args);
```

**参数：**
- `name`: 方法名称
- `args`: 方法参数（`mc::variant`，必须是数组类型）

**返回值：**
- `mc::variants`: 方法返回值列表

**说明：**
- 自动进行参数类型验证（使用 `mdb_validator`）
- 自动获取当前上下文（context）
- 如果方法不存在或参数类型不匹配，抛出异常

**异常：**
- 如果方法不存在，抛出 `mc::exception`
- 如果参数类型不匹配，抛出 `mc::exception`

**示例：**
```cpp
proxy_object* obj = ...;
mc::variants args = {
    mc::variant(ctx),      // context
    mc::variant(1),        // offset
    mc::variant(30)        // length
};
mc::variants results = obj->call_method("Read", mc::variant(args));
```

---

### call_method_pcall

安全调用 D-Bus 方法（不抛出异常）。

```cpp
mc::variants call_method_pcall(
    const std::string& name,
    const mc::variant& args,
    std::optional<std::string>& error
);
```

**参数：**
- `name`: 方法名称
- `args`: 方法参数（`mc::variant`，必须是数组类型）
- `error`: 错误信息输出参数（如果调用失败，包含错误信息）

**返回值：**
- `mc::variants`: 方法返回值列表（如果调用失败，返回空列表）

**说明：**
- 与 `call_method` 功能相同，但不会抛出异常
- 如果调用失败，错误信息存储在 `error` 参数中

**示例：**
```cpp
proxy_object* obj = ...;
mc::variants args = {mc::variant(ctx), mc::variant(1), mc::variant(30)};
std::optional<std::string> error;
mc::variants results = obj->call_method_pcall("Read", mc::variant(args), error);

if (error.has_value()) {
    std::cerr << "Method call failed: " << error.value() << std::endl;
} else {
    // 处理返回值
    for (const auto& result : results) {
        std::cout << result << std::endl;
    }
}
```

---

## 易变属性检查

### is_volatile

检查属性是否为易变属性。

```cpp
bool is_volatile(const std::string& name) const;
```

**参数：**
- `name`: 属性名称

**返回值：**
- `bool`: 如果属性是易变的返回 `true`，否则返回 `false`

**说明：**
- 易变属性表示该属性的值可能会频繁变化
- 如果属性不存在，抛出异常

**异常：**
- 如果属性不存在，抛出 `mc::exception`

**示例：**
```cpp
proxy_object* obj = ...;
if (obj->is_volatile("Status")) {
    // 属性是易变的，可能需要频繁重新读取
}
```

---

## 元数据访问

### bus

获取 D-Bus 连接对象。

```cpp
mc::dbus::connection& bus();
```

**返回值：**
- `mc::dbus::connection&`: D-Bus 连接对象引用

---

### service

获取服务名称。

```cpp
const std::string& service() const;
```

**返回值：**
- `const std::string&`: 服务名称

---

### path

获取对象路径。

```cpp
const std::string& path() const;
```

**返回值：**
- `const std::string&`: 对象路径

---

### interface

获取接口名称。

```cpp
const std::string& interface() const;
```

**返回值：**
- `const std::string&`: 接口名称

---

## 使用示例

### 完整示例

```cpp
#include "mc/mdb/proxy_object.h"
#include "introspection/introspection_cache.h"
#include <mc/dbus/sd_bus.h>
#include <mc/engine/context.h>

void example_usage() {
    // 1. 获取接口信息
    auto& cache = introspection_cache::instance();
    mc::dbus::sd_bus* bus = ...;
    const interface_info& iface_info = cache.get_interface(
        bus,
        "org.example.Service",
        "/org/example/Object",
        "org.example.Interface"
    );

    // 2. 创建代理对象
    auto bus_ptr = std::make_unique<mc::dbus::sd_bus>(*bus);
    proxy_object obj(
        std::move(bus_ptr),
        "org.example.Service",
        "/org/example/Object",
        "org.example.Interface",
        iface_info
    );

    // 3. 检查属性和方法
    if (obj.has_property("Name")) {
        std::cout << "Property 'Name' exists" << std::endl;
    }

    if (obj.has_method("TestMethod")) {
        std::cout << "Method 'TestMethod' exists" << std::endl;
    }

    // 4. 读取属性
    mc::variant name = obj.get_property("Name");
    std::cout << "Name: " << name.as_string() << std::endl;

    // 5. 设置属性
    obj.set_property("Enabled", mc::variant(true));

    // 6. 获取所有属性
    mc::dict all_props = obj.get_all_properties();
    for (const auto& entry : all_props) {
        std::cout << entry.key.as_string() << " = " << entry.value << std::endl;
    }

    // 7. 批量获取属性
    mc::variants prop_names = {
        mc::variant("Id"),
        mc::variant("Name"),
        mc::variant("Enabled")
    };
    auto [props, errs] = obj.get_properties(prop_names);

    // 8. 调用方法
    mc::dict ctx = ...; // 获取上下文
    mc::variants method_args = {
        mc::variant(ctx),
        mc::variant(1),
        mc::variant(30)
    };
    mc::variants results = obj.call_method("Read", mc::variant(method_args));

    // 9. 安全调用方法
    std::optional<std::string> error;
    results = obj.call_method_pcall("Read", mc::variant(method_args), error);
    if (error.has_value()) {
        std::cerr << "Error: " << error.value() << std::endl;
    }

    // 10. 检查易变属性
    if (obj.is_volatile("Status")) {
        std::cout << "Status is volatile" << std::endl;
    }

    // 11. 获取元数据
    std::cout << "Service: " << obj.service() << std::endl;
    std::cout << "Path: " << obj.path() << std::endl;
    std::cout << "Interface: " << obj.interface() << std::endl;
}
```

---

## 注意事项

1. **生命周期管理**：
   - 代理对象持有 `sd_bus` 的所有权（通过 `unique_ptr`）
   - 确保 `sd_bus` 在代理对象生命周期内有效

2. **线程安全**：
   - `proxy_object` 不是线程安全的
   - 如果需要在多线程环境中使用，需要外部同步

3. **异常处理**：
   - 大部分方法在失败时会抛出异常
   - 使用 `call_method_pcall` 可以避免异常

4. **类型验证**：
   - 属性设置和方法调用会自动进行类型验证
   - 确保传入的参数类型正确

5. **上下文（Context）**：
   - 属性操作和方法调用会自动获取当前上下文
   - 上下文包含调用者信息（如用户名、接口等）

6. **性能考虑**：
   - 每次属性访问和方法调用都会进行 D-Bus 通信
   - 考虑使用 `get_all_properties` 或 `get_properties` 批量获取属性

---

## 相关文档

- [mdb_access API](./mdb_access.md) - 获取 proxy_object 实例
- [introspection API](./introspection.md) - 获取接口信息
- [validator API](./validator.md) - 类型验证
- [mdb_access Lua API](../lua_api/mdb/mdb_access.md) - Lua 绑定
