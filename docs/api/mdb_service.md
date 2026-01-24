# MDB Service C++ API

## 概述

MDB Service 模块是对 `bmc.kepler.maca` D-Bus 服务的客户端封装，提供了对象查询、路径管理、服务发现等功能。该模块实现了智能缓存机制，支持 LRU 淘汰策略，并通过 D-Bus 信号监听自动维护缓存一致性。

## 头文件

```cpp
#include <mc/mdb_service.h>
```

## 命名空间

```cpp
namespace mc::mdb::service
```

## 核心特性

1. **智能缓存**: 对 `get_path` 接口实现了 LRU 缓存，显著提升查询性能
2. **缓存一致性**: 通过监听 D-Bus 信号自动维护缓存有效性
3. **线程安全**: 所有接口均为线程安全
4. **灵活配置**: 支持动态调整缓存大小和查询行为

---

## 对象查询接口

### get_object

获取指定路径和接口的对象信息。

**函数签名**

```cpp
mc::variant get_object(mc::dbus::sd_bus*   bus,
                       std::string_view    path,
                       const mc::variants& interfaces);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `path`: 对象路径，如 `"/org/freedesktop/DBus"`
- `interfaces`: 接口名称列表

**返回值**

返回对象信息（类型依赖于 MDB 服务的实现）。

**示例**

```cpp
mc::variants interfaces;
interfaces.push_back(mc::variant("org.freedesktop.DBus"));

auto result = mc::mdb::service::get_object(bus, "/org/freedesktop/DBus", interfaces);
```

---

### get_sub_objects

获取指定路径的子对象。

**函数签名**

```cpp
mc::variant get_sub_objects(mc::dbus::sd_bus*   bus,
                            std::string_view    path,
                            int32_t             depth,
                            const mc::variants& interfaces);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `path`: 对象路径
- `depth`: 查询深度（-1 表示无限深度）
- `interfaces`: 接口名称列表

**返回值**

返回子对象信息数组。

**示例**

```cpp
mc::variants interfaces;
auto result = mc::mdb::service::get_sub_objects(bus, "/", 1, interfaces);
```

---

### get_sub_paths

获取指定路径的子路径列表。

**函数签名**

```cpp
mc::variant get_sub_paths(mc::dbus::sd_bus*   bus,
                          std::string_view    path,
                          int32_t             depth,
                          const mc::variants& interfaces);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `path`: 对象路径
- `depth`: 查询深度
- `interfaces`: 接口名称列表

**返回值**

返回子路径列表（数组）。

**示例**

```cpp
mc::variants interfaces;
auto result = mc::mdb::service::get_sub_paths(bus, "/", 1, interfaces);
```

---

### get_sub_paths_paging

分页获取子路径列表。

**函数签名**

```cpp
mc::variant get_sub_paths_paging(mc::dbus::sd_bus*   bus,
                                std::string_view    path,
                                int32_t             depth,
                                const mc::variants& interfaces,
                                int32_t             skip,
                                int32_t             top);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `path`: 对象路径
- `depth`: 查询深度
- `interfaces`: 接口名称列表
- `skip`: 跳过的数量
- `top`: 获取的数量

**返回值**

返回分页后的子路径列表。

**示例**

```cpp
mc::variants interfaces;
// 跳过前 10 个，获取 20 个
auto result = mc::mdb::service::get_sub_paths_paging(bus, "/", 1, interfaces, 10, 20);
```

---

### get_parent_objects

获取指定路径的父对象。

**函数签名**

```cpp
mc::variant get_parent_objects(mc::dbus::sd_bus*   bus,
                               std::string_view    path,
                               const mc::variants& interfaces);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `path`: 对象路径
- `interfaces`: 接口名称列表

**返回值**

返回父对象信息。

---

## 服务发现接口

### get_service_names

获取所有已注册的服务名称列表。

**函数签名**

```cpp
mc::variant get_service_names(mc::dbus::sd_bus* bus);
```

**参数**

- `bus`: D-Bus 连接对象指针

**返回值**

返回服务名称数组。

**示例**

```cpp
auto result = mc::mdb::service::get_service_names(bus);
if (result.is_array()) {
    auto services = result.as<mc::variants>();
    for (const auto& service : services) {
        std::cout << service.as_string() << std::endl;
    }
}
```

---

### get_service_name

根据发送者名称获取服务名称。

**函数签名**

```cpp
mc::variant get_service_name(mc::dbus::sd_bus* bus, std::string_view sender);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `sender`: 发送者名称

**返回值**

返回服务名称字符串。

---

### get_interface_owners

获取实现指定接口的所有对象的拥有者列表。

**函数签名**

```cpp
mc::variant get_interface_owners(mc::dbus::sd_bus* bus, std::string_view interface);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `interface`: 接口名称

**返回值**

返回拥有者列表。

**示例**

```cpp
auto result = mc::mdb::service::get_interface_owners(bus, "org.freedesktop.DBus");
```

---

## 路径查询接口

### get_path

根据接口和过滤条件查找对象路径（支持缓存）。

**函数签名**

```cpp
mc::variant get_path(mc::dbus::sd_bus* bus,
                     std::string_view  interface,
                     std::string_view  filter,
                     bool              ignore_case,
                     bool              enable_cache);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `interface`: 接口名称
- `filter`: 过滤条件（JSON 格式），如 `"{\"Property\":\"Value\"}"`
- `ignore_case`: 是否忽略大小写
- `enable_cache`: 是否启用缓存

**返回值**

返回符合条件的对象路径字符串。

**缓存机制**

- **缓存键**: `interface + ":" + filter`
- **缓存策略**: LRU（最近最少使用）
- **缓存失效**: 监听以下 D-Bus 信号自动失效
  - `NameOwnerChanged`: 服务退出时清理相关缓存
  - `InterfacesRemoved`: 接口移除时清理缓存
  - `PropertiesChanged`: 属性变化时清理匹配的缓存

**精确匹配与模糊匹配**

- `ignore_case=true`: 模糊匹配，可以使用任何缓存结果
- `ignore_case=false`: 精确匹配，只能使用精确匹配的缓存

**二次验证**

为确保缓存有效性，在加入缓存前会进行二次验证：
1. 第一次调用 `GetPath` 获取路径
2. 设置信号监听
3. 第二次调用 `GetPath` 验证路径未变化
4. 验证通过后加入缓存

**示例**

```cpp
// 使用缓存查询
auto result = mc::mdb::service::get_path(
    bus,
    "xyz.openbmc_project.Inventory.Item.Cpu",
    "{\"ProcessorType\":\"CPU\"}",
    false,  // 精确匹配
    true    // 启用缓存
);

// 不使用缓存（实时查询）
auto result2 = mc::mdb::service::get_path(
    bus,
    "xyz.openbmc_project.Inventory.Item.Cpu",
    "{}",
    true,   // 模糊匹配
    false   // 禁用缓存
);
```

---

### is_valid_path

验证指定路径是否有效。

**函数签名**

```cpp
mc::variant is_valid_path(mc::dbus::sd_bus* bus,
                          std::string_view  path,
                          bool              ignore_case);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `path`: 对象路径
- `ignore_case`: 是否忽略大小写

**返回值**

返回布尔值，`true` 表示路径有效。

**示例**

```cpp
auto result = mc::mdb::service::is_valid_path(bus, "/org/freedesktop/DBus", false);
if (result.is_bool() && result.as<bool>()) {
    std::cout << "路径有效" << std::endl;
}
```

---

## 类和对象管理

### get_classes

获取指定服务的类列表。

**函数签名**

```cpp
mc::variant get_classes(mc::dbus::sd_bus* bus, std::string_view service);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `service`: 服务名称

**返回值**

返回类名称数组。

---

### get_object_list

获取指定类的对象列表。

**函数签名**

```cpp
mc::variant get_object_list(mc::dbus::sd_bus* bus, std::string_view class_name);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `class_name`: 类名称

**返回值**

返回对象列表数组。

---

### get_object_owner

获取指定对象的拥有者。

**函数签名**

```cpp
mc::variant get_object_owner(mc::dbus::sd_bus* bus, std::string_view object_name);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `object_name`: 对象名称

**返回值**

返回拥有者信息。

---

### get_matched_objects

根据对象名称和接口模式获取匹配的对象列表。

**函数签名**

```cpp
mc::variant get_matched_objects(mc::dbus::sd_bus* bus,
                                std::string_view  object_name,
                                std::string_view  interface_pattern);
```

**参数**

- `bus`: D-Bus 连接对象指针
- `object_name`: 对象名称
- `interface_pattern`: 接口模式（支持通配符）

**返回值**

返回匹配的对象列表。

---

### get_traced_object

获取追踪对象信息（用于调试）。

**函数签名**

```cpp
mc::variant get_traced_object(mc::dbus::sd_bus* bus);
```

**参数**

- `bus`: D-Bus 连接对象指针

**返回值**

返回追踪对象信息。

---

## 缓存管理接口

### clear_cache

清理所有缓存（包括信号订阅）。

**函数签名**

```cpp
void clear_cache();
```

**功能**

- 清理所有缓存条目
- 移除所有索引
- 取消所有信号订阅

**示例**

```cpp
// 清理缓存
mc::mdb::service::clear_cache();
```

---

### set_max_cache_size

设置缓存的最大条目数量。

**函数签名**

```cpp
void set_max_cache_size(size_t max_size);
```

**参数**

- `max_size`: 最大缓存数量
  - `0`: 不限制缓存大小
  - `> 0`: 限制最大条目数，超过时使用 LRU 策略淘汰

**默认值**: 400

**示例**

```cpp
// 限制缓存为 100 条
mc::mdb::service::set_max_cache_size(100);

// 不限制缓存大小
mc::mdb::service::set_max_cache_size(0);
```

**注意事项**

- 调整缓存大小时会触发 LRU 淘汰，移除超出的条目
- 淘汰时会正确清理相关的信号订阅

---

### get_max_cache_size

获取当前设置的最大缓存数量。

**函数签名**

```cpp
size_t get_max_cache_size();
```

**返回值**

返回最大缓存数量，`0` 表示不限制。

**示例**

```cpp
size_t max_size = mc::mdb::service::get_max_cache_size();
std::cout << "最大缓存数量: " << max_size << std::endl;
```

---

### get_cache_size

获取当前缓存的条目数量。

**函数签名**

```cpp
size_t get_cache_size();
```

**返回值**

返回当前缓存的条目数量。

**示例**

```cpp
size_t current_size = mc::mdb::service::get_cache_size();
std::cout << "当前缓存数量: " << current_size << std::endl;
```

---

## 缓存机制详解

### LRU 缓存策略

缓存基于 `mc::algorithm::lru_cache` 实现，特点：

1. **自动淘汰**: 当缓存达到上限时，自动淘汰最久未访问的条目
2. **访问更新**: 每次查询缓存时，会更新条目的访问时间
3. **线程安全**: 所有缓存操作都受互斥锁保护

### 缓存一致性维护

#### 全局 NameOwnerChanged 订阅

- **订阅对象**: `org.freedesktop.DBus.NameOwnerChanged`
- **触发时机**: D-Bus 服务退出
- **处理逻辑**: 清理所有与该服务相关的缓存条目
- **订阅策略**: 全局单例，只订阅一次（优化性能）

#### 按路径订阅 InterfacesRemoved

- **订阅对象**: `org.freedesktop.DBus.ObjectManager.InterfacesRemoved`
- **触发时机**: 对象接口被移除
- **处理逻辑**: 清理指定路径和接口的缓存条目
- **引用计数**: 支持多个缓存条目共享同一个订阅

#### 按路径和接口订阅 PropertiesChanged

- **订阅对象**: `org.freedesktop.DBus.Properties.PropertiesChanged`
- **触发时机**: 对象属性变化
- **处理逻辑**: 检查变化的属性是否在 filter 中，如果匹配则清理缓存
- **精确匹配**: 支持 `ignore_case` 参数的精确匹配

### 订阅引用计数

为避免重复订阅，实现了引用计数机制：

```
订阅键: path + ":" + interface

缓存条目1: path="/cpu0", interface="Cpu" → ref_count++
缓存条目2: path="/cpu0", interface="Cpu" → ref_count++ (共享订阅)
淘汰条目1: ref_count--
淘汰条目2: ref_count-- → 0, 取消订阅
```

### 延迟清理机制

为避免在持有锁时执行 D-Bus 操作（可能导致死锁），实现了延迟清理：

1. **淘汰时**: 将需要清理的订阅信息放入 `thread_local` 队列
2. **解锁后**: 统一处理队列中的订阅清理
3. **线程隔离**: 使用 `thread_local` 避免跨线程竞争

---

## 多线程安全设计

### 锁策略

- **全局缓存锁**: 保护缓存数据结构和索引
- **全局订阅锁**: 保护 `NameOwnerChanged` 订阅状态
- **细粒度锁**: 最小化锁持有时间，避免死锁

### 线程安全保证

1. 所有缓存操作都受互斥锁保护
2. D-Bus 操作（如 `add_match`、`remove_match`）在锁外执行
3. 使用延迟清理避免锁内执行耗时操作

---

## 使用最佳实践

### 1. 合理使用缓存

```cpp
// 频繁查询相同条件 - 使用缓存
for (int i = 0; i < 1000; i++) {
    auto path = mc::mdb::service::get_path(bus, "interface", "{}", false, true);
}

// 需要实时数据 - 禁用缓存
auto path = mc::mdb::service::get_path(bus, "interface", "{}", false, false);
```

### 2. 监控缓存状态

```cpp
// 定期检查缓存使用情况
size_t current = mc::mdb::service::get_cache_size();
size_t max = mc::mdb::service::get_max_cache_size();

if (max > 0 && current > max * 0.9) {
    std::cout << "缓存使用率高: " << current << "/" << max << std::endl;
}
```

### 3. 优雅退出

```cpp
// 程序退出前清理资源
mc::mdb::service::clear_cache();
```

### 4. 调优缓存大小

```cpp
// 根据系统负载调整缓存大小
if (high_query_frequency) {
    mc::mdb::service::set_max_cache_size(1000);
} else {
    mc::mdb::service::set_max_cache_size(100);
}
```

---

## 性能考虑

### 缓存命中率

- **首次查询**: 需要进行 2 次 D-Bus 调用（二次验证）+ 信号订阅
- **缓存命中**: 几乎零开销，直接从内存返回
- **缓存失效**: 由 D-Bus 信号自动触发，无需轮询

### 内存占用

每个缓存条目约占用：
- `cache_entry` 结构体：~200 字节
- 索引数据：~100 字节
- 信号订阅（共享）：~100 字节/订阅

例如：400 条缓存 ≈ 80KB 内存

---

## 错误处理

所有接口都可能抛出以下异常：

- **`mc::dbus_exception`**: D-Bus 调用失败
- **`mc::timeout_exception`**: D-Bus 调用超时
- **`std::exception`**: 其他异常（如 JSON 解析错误）

**示例**

```cpp
try {
    auto result = mc::mdb::service::get_path(bus, "interface", "{}", false, true);
    // 处理结果
} catch (const mc::dbus_exception& e) {
    elog("D-Bus 调用失败: ${error}", ("error", e.what()));
} catch (const mc::timeout_exception& e) {
    elog("D-Bus 调用超时: ${error}", ("error", e.what()));
} catch (const std::exception& e) {
    elog("未知错误: ${error}", ("error", e.what()));
}
```

---

## 相关文档

- [MDB Privilege API](./mdb_privilege.md)
- [MDB Service Lua API](../lua_api/mdb.md#service)
- [LRU Cache 设计](../algorithm/lru_cache.md)
- [D-Bus 信号处理](../dbus/signal_handling.md)
