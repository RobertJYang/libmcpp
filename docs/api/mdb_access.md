# MDB Access API 文档

## 概述

`mdb_access` 类提供了 D-Bus 代理对象的管理功能，包括对象创建、缓存管理等。该类使用单例模式，提供全局唯一的对象管理器实例，并通过 LRU 缓存机制提高性能。

## 类定义

```cpp
class mdb_access {
public:
    static mdb_access& instance(size_t max_cache_size = 100);

    std::shared_ptr<proxy_object> get_object_by_short_call(
        mc::dbus::sd_bus*  bus,
        const std::string& path,
        const std::string& interface);

    std::shared_ptr<proxy_object> get_object(
        std::shared_ptr<mc::dbus::sd_bus> bus,
        const std::string&                path,
        const std::string&                interface);

    std::shared_ptr<proxy_object> get_object_with_service(
        std::shared_ptr<mc::dbus::sd_bus> bus,
        const std::string&                service,
        const std::string&                path,
        const std::string&                interface);

    std::map<std::string, std::shared_ptr<proxy_object>> get_sub_objects(
        std::shared_ptr<mc::dbus::sd_bus> bus,
        const std::string&                path,
        const std::string&                interface,
        int32_t                           depth = 1);

    void clear_cache();
    void set_max_cache_size(size_t max_size);
    size_t cache_size() const;
};
```

---

## 单例模式

### instance

获取对象管理器单例。

```cpp
static mdb_access& instance(size_t max_cache_size = 100);
```

**参数：**
- `max_cache_size`: 最大缓存数量，默认值为 100

**返回值：**
- `mdb_access&`: 对象管理器单例引用

**说明：**
- 使用单例模式，确保全局只有一个管理器实例
- 线程安全（使用 `std::once_flag` 和 `std::call_once` 保证）
- 第一次调用时可以指定最大缓存数量，后续调用会忽略该参数

**示例：**
```cpp
// 使用默认缓存大小（100）
auto& mgr = mdb_access::instance();

// 指定缓存大小
auto& mgr = mdb_access::instance(200);
```

---

## 公共方法

### clear_cache

清空缓存。

```cpp
void clear_cache();
```

**说明：**
- 线程安全，会阻塞直到所有并发操作完成
- 清空所有缓存的代理对象

---

### set_max_cache_size

设置最大缓存数量。

```cpp
void set_max_cache_size(size_t max_size);
```

**参数：**
- `max_size`: 最大缓存数量

**说明：**
- 线程安全
- 如果当前缓存大小超过新的最大值，会淘汰最久未使用的对象

---

### cache_size

获取当前缓存大小。

```cpp
size_t cache_size() const;
```

**返回值：**
- `size_t`: 当前缓存的条目数量

**说明：**
- 线程安全

---

### get_object_by_short_call

通过短调用方式获取指定路径和接口的代理对象。

```cpp
proxy_object* get_object_by_short_call(
    mc::dbus::sd_bus*  bus,
    const std::string& path,
    const std::string& interface
);
```

**参数：**
- `bus`: D-Bus 连接对象指针
- `path`: 对象路径
- `interface`: 接口名称

**返回值：**
- `std::shared_ptr<proxy_object>`: 代理对象的智能指针

**说明：**
- 使用短调用方式获取对象
- 不进行缓存检查，每次都会创建新对象
- 不缓存对象，对象生命周期由返回的 `shared_ptr` 管理
- 适用于某些特殊场景，或者需要独立对象实例的情况

**异常：**
- 如果服务不存在，抛出 `mc::system_exception`
- 如果接口不存在，抛出 `std::runtime_error`

**示例：**
```cpp
#include "mc/mdb/mdb_access.h"
#include <mc/dbus/sd_bus.h>

mc::dbus::sd_bus* bus = ...;
auto& mgr = mdb_access::instance();

auto obj = mgr.get_object_by_short_call(
    bus,
    "/org/example/Object",
    "org.example.Interface"
);

// 使用代理对象
mc::variant value = obj->get_property("Name");
```

---

### get_object

获取指定路径和接口的代理对象。

```cpp
std::shared_ptr<proxy_object> get_object(
    std::shared_ptr<mc::dbus::sd_bus> bus,
    const std::string&                path,
    const std::string&                interface
);
```

**参数：**
- `bus`: D-Bus 连接对象的 `shared_ptr`（共享所有权）
- `path`: 对象路径，例如 `"/org/example/Object"`
- `interface`: 接口名称，例如 `"org.example.Interface"`

**返回值：**
- `std::shared_ptr<proxy_object>`: 代理对象的智能指针

**说明：**
- 接收 `shared_ptr<sd_bus>`，由调用方与代理对象、缓存等共同管理 `sd_bus` 生命周期
- 首先检查缓存，如果缓存中存在且连接有效，则直接返回
- 如果缓存中不存在或连接无效，则：
  1. 调用 `mdb::service::get_object` 获取服务名称
  2. 从 `introspection_cache` 获取接口信息
  3. 创建新的 `proxy_object` 实例
  4. 将对象存入 LRU 缓存（仅在非阻塞模式下）
- 缓存键为 `path + interface`
- 返回的 `shared_ptr` 由缓存和调用者共同管理
- 在阻塞模式下，会跳过 `is_connected()` 判断且不缓存对象；在非阻塞模式下，需要检查连接是否有效并缓存对象

**异常：**
- 如果服务不存在，抛出 `mc::system_exception`
- 如果接口不存在，抛出 `std::runtime_error`

**示例：**
```cpp
#include "mc/mdb/mdb_access.h"
#include <mc/dbus/sd_bus.h>

auto bus = std::make_unique<mc::dbus::sd_bus>(connection, false);
auto& mgr = mdb_access::instance();

auto obj = mgr.get_object(
    std::move(bus),
    "/org/example/Object",
    "org.example.Interface"
);

// 使用代理对象
mc::variant value = obj->get_property("Name");
```

---

### get_object_with_service

使用指定的 service 名称获取代理对象。

```cpp
std::shared_ptr<proxy_object> get_object_with_service(
    std::shared_ptr<mc::dbus::sd_bus> bus,
    const std::string&                service,
    const std::string&                path,
    const std::string&                interface
);
```

**参数：**
- `bus`: D-Bus 连接对象的 `shared_ptr`（共享所有权）
- `service`: 服务名称，例如 `"org.example.Service"`
- `path`: 对象路径，例如 `"/org/example/Object"`
- `interface`: 接口名称，例如 `"org.example.Interface"`

**返回值：**
- `std::shared_ptr<proxy_object>`: 代理对象的智能指针

**说明：**
- 此方法不会查找 service，直接使用提供的 service 名称
- 会对 service 名称进行判空检查，如果为空则抛出异常
- 首先检查缓存，如果缓存中存在且连接有效，则直接返回
- 如果缓存中不存在或连接无效，则：
  1. 直接使用提供的 service 名称（不调用 `mdb::service::get_object`）
  2. 从 `introspection_cache` 获取接口信息
  3. 创建新的 `proxy_object` 实例
  4. 将对象存入 LRU 缓存（仅在非阻塞模式下）
- 缓存键为 `path + interface`
- 返回的 `shared_ptr` 由缓存和调用者共同管理
- 在阻塞模式下，会跳过 `is_connected()` 判断且不缓存对象；在非阻塞模式下，需要检查连接是否有效并缓存对象
- 适用于已知 service 名称的场景，可以避免额外的服务查找开销

**异常：**
- 如果 service 名称为空，抛出 `mc::invalid_arg_exception`
- 如果接口不存在，抛出 `std::runtime_error`

**示例：**
```cpp
#include "mc/mdb/mdb_access.h"
#include <mc/dbus/sd_bus.h>

auto bus = std::make_unique<mc::dbus::sd_bus>(connection, false);
auto& mgr = mdb_access::instance();

// 已知 service 名称，直接使用
auto obj = mgr.get_object_with_service(
    std::move(bus),
    "org.example.Service",  // 直接指定 service 名称
    "/org/example/Object",
    "org.example.Interface"
);

// 使用代理对象
mc::variant value = obj->get_property("Name");
```

---

### get_sub_objects

获取指定路径下所有子对象的代理对象。

```cpp
std::map<std::string, std::shared_ptr<proxy_object>> get_sub_objects(
    std::shared_ptr<mc::dbus::sd_bus> bus,
    const std::string&                path,
    const std::string&                interface,
    int32_t                           depth = 1
);
```

**参数：**
- `bus`: D-Bus 连接对象的 `shared_ptr`（共享所有权）
- `path`: 父路径，例如 `"/org/example"`
- `interface`: 接口名称，例如 `"org.example.Interface"`
- `depth`: 递归深度，默认为 1

**返回值：**
- `std::map<std::string, std::shared_ptr<proxy_object>>`: 子对象映射，键为子路径字符串，值为对应的代理对象智能指针

**说明：**
- 调用 `mdb::service::get_sub_paths` 获取指定路径下的所有子路径
- 为每个子路径创建对应的 `proxy_object` 实例
- 如果某个子路径获取对象失败，会跳过该路径，不影响其他路径的处理
- 返回的 map 中只包含成功获取的对象
- 每个子对象都会通过 `get_object` 方法获取，因此会使用缓存机制
- 如果 `get_object` 失败（例如服务不存在），该子路径会被跳过

**异常：**
- 如果 `get_sub_paths` 返回无效结果，抛出 `mc::system_exception`

**示例：**
```cpp
#include "mc/mdb/mdb_access.h"
#include <mc/dbus/sd_bus.h>

auto bus = std::make_unique<mc::dbus::sd_bus>(connection, false);
auto& mgr = mdb_access::instance();

// 获取 /org/example 路径下所有子对象
auto sub_objects = mgr.get_sub_objects(
    std::move(bus),
    "/org/example",
    "org.example.Interface",
    1  // 深度为 1，只获取直接子对象
);

// 遍历子对象
for (const auto& [sub_path, obj] : sub_objects) {
    std::cout << "Path: " << sub_path << std::endl;
    mc::variant value = obj->get_property("Name");
    std::cout << "Name: " << value.as_string() << std::endl;
}
```

---

## 缓存机制

### LRU 缓存

`mdb_access` 使用 LRU（Least Recently Used）缓存机制：

- **缓存大小**：默认 100 个对象（可通过 `instance(max_cache_size)` 或 `set_max_cache_size()` 设置）
- **缓存键**：`path + interface`
- **缓存值**：`std::shared_ptr<proxy_object>`

### 缓存行为

1. **缓存命中**：
   - 如果缓存中存在对象且连接有效，直接返回
   - 不会创建新对象，提高性能

2. **缓存未命中**：
   - 如果缓存中不存在或连接无效，创建新对象
   - 新对象存入缓存
   - 如果缓存已满，自动淘汰最久未使用的对象

3. **连接检查**：
   - 每次获取对象时检查连接是否有效
   - 如果连接无效，创建新对象替换缓存中的对象

---

## 使用示例

### 完整示例

```cpp
#include "mc/mdb/mdb_access.h"
#include "mc/mdb/proxy_object.h"
#include <mc/dbus/sd_bus.h>
#include <mc/dbus/connection.h>
#include <iostream>

void example_usage() {
    // 1. 创建 D-Bus 连接
    auto conn = mc::dbus::connection::open_session_bus(mc::get_io_context());
    conn.start();
    mc::dbus::sd_bus bus(std::move(conn), true);

    // 2. 获取对象管理器（可以指定缓存大小）
    auto& mgr = mdb_access::instance(200);  // 设置缓存大小为 200

    // 3. 获取代理对象（使用短调用方式，不缓存）
    auto obj1 = mgr.get_object_by_short_call(
        &bus,
        "/org/example/Object",
        "org.example.Interface"
    );

    // 使用对象
    if (obj1->has_property("Name")) {
        mc::variant name = obj1->get_property("Name");
        std::cout << "Name: " << name.as_string() << std::endl;
    }

    // 4. 获取代理对象（使用 shared_ptr，会缓存）
    auto bus_ptr = std::make_shared<mc::dbus::sd_bus>(bus.get_connection(), false);
    auto obj2 = mgr.get_object(
        bus_ptr,
        "/org/example/Object",
        "org.example.Interface"
    );
    // obj2 会被缓存

    // 5. 再次获取相同对象（会从缓存返回）
    auto bus_ptr2 = std::make_shared<mc::dbus::sd_bus>(bus.get_connection(), false);
    auto obj3 = mgr.get_object(
        bus_ptr2,
        "/org/example/Object",
        "org.example.Interface"
    );
    // obj2 和 obj3 指向同一个对象（如果缓存命中）

    // 6. 使用 get_object_with_service（已知 service 名称）
    auto bus_ptr3 = std::make_shared<mc::dbus::sd_bus>(bus.get_connection(), false);
    auto obj4 = mgr.get_object_with_service(
        bus_ptr3,
        "org.example.Service",  // 直接指定 service 名称
        "/org/example/Object",
        "org.example.Interface"
    );

    // 7. 获取子对象
    auto bus_ptr4 = std::make_shared<mc::dbus::sd_bus>(bus.get_connection(), false);
    auto sub_objects = mgr.get_sub_objects(
        bus_ptr4,
        "/org/example",
        "org.example.Interface",
        1
    );
    for (const auto& [sub_path, sub_obj] : sub_objects) {
        std::cout << "Sub path: " << sub_path << std::endl;
    }

    // 6. 调用方法
    mc::dict ctx = ...; // 获取上下文
    mc::variants args = {
        mc::variant(ctx),
        mc::variant(1),
        mc::variant(30)
    };
    mc::variants results = obj3->call_method("Read", mc::variant(args));

    // 7. 批量获取属性
    mc::variants prop_names = {
        mc::variant("Id"),
        mc::variant("Name"),
        mc::variant("Enabled")
    };
    auto [props, errs] = obj3->get_properties(prop_names);

    for (const auto& entry : props) {
        std::cout << entry.key.as_string() << " = " << entry.value << std::endl;
    }

    // 8. 缓存管理
    std::cout << "Cache size: " << mgr.cache_size() << std::endl;
    mgr.set_max_cache_size(300);  // 调整缓存大小
    mgr.clear_cache();  // 清空缓存
}
```

---

## 注意事项

1. **生命周期管理**：
   - 返回的 `std::shared_ptr<proxy_object>` 由缓存和调用者共同管理
   - 对象在缓存中的引用计数和调用者的引用计数共同保证对象生命周期
   - 当缓存中的对象被淘汰时，如果调用者仍持有引用，对象不会被销毁
   - 建议不要保存 `shared_ptr` 的长期引用，因为对象可能被缓存淘汰

2. **线程安全**：
   - `mdb_access` 的缓存操作是线程安全的（使用 `std::mutex` 保护）
   - 单例初始化是线程安全的（使用 `std::once_flag` 和 `std::call_once`）

3. **缓存一致性**：
   - 缓存不会自动失效
   - 如果 D-Bus 对象的接口发生变化，可以使用 `clear_cache()` 手动清理缓存

4. **连接管理**：
- `get_object_by_short_call` 方法不拥有 `bus` 的所有权，需要确保 `bus` 在对象使用期间有效
- `get_object` 和 `get_object_with_service` 方法通过 `shared_ptr` 共享 `bus` 的所有权，`bus` 的生命周期由代理对象、缓存和调用方共同管理
- `get_sub_objects` 方法会为每个子对象创建新的 `sd_bus` 实例（基于原始连接），同样通过 `shared_ptr` 管理
   - 在阻塞模式下，会跳过 `is_connected()` 判断且不缓存对象；在非阻塞模式下，需要检查连接是否有效并缓存对象

5. **性能考虑**：
   - 缓存机制可以显著提高性能，避免重复创建对象和获取接口信息
   - 缓存大小有限，如果对象数量超过缓存大小，会频繁淘汰对象

6. **错误处理**：
   - 所有方法在失败时会抛出异常
   - 需要适当处理异常

---

## 相关文档

- [proxy_object API](./proxy_object.md) - 代理对象接口
- [introspection API](./introspection.md) - 接口信息获取
- [mdb_service API](./mdb_service.md) - MDB 服务接口
- [mdb_access Lua API](../lua_api/mdb/mdb_access.md) - Lua 绑定

---
