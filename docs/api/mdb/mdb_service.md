# MDB Service C++ API

## 概述

MDB Service 模块提供了与 BMC 的 MDB 服务（`bmc.kepler.maca`）进行通信的客户端接口。该模块封装了对 MDB 服务的所有调用，并提供了智能缓存和自动失效机制，以优化性能。

## 头文件

```cpp
#include <mc/mdb_service.h>
```

## 命名空间

```cpp
namespace mc::mdb
```

## 常量

### MDB 服务配置

- **服务名称**: `bmc.kepler.maca`
- **对象路径**: `/bmc/kepler/MdbService`
- **接口名称**: `bmc.kepler.Mdb`
- **超时时间**: 10分钟（600000毫秒）

## 接口函数

### 对象查询接口

#### get_object

获取指定路径的对象信息。

**函数签名**

```cpp
mc::variant get_object(
    mc::dbus::sd_bus* bus,
    std::string_view path,
    const mc::variants& interfaces
);
```

**参数**

- `bus`: DBus 连接对象指针
- `path`: 对象路径
- `interfaces`: 需要查询的接口名称列表

**返回值**

返回对象信息的 variant，格式为 dict，包含对象的所有属性。

**异常**

- 当 DBus 调用失败时抛出异常
- 当对象不存在时抛出异常

**示例**

```cpp
auto bus = mc::dbus::sd_bus::open_system();
auto result = mc::mdb::get_object(
    &bus,
    "/redfish/v1/Systems/1",
    {"xyz.openbmc_project.Inventory.Item.System"}
);
```

---

#### get_sub_objects

获取指定路径下的所有子对象。

**函数签名**

```cpp
mc::variant get_sub_objects(
    mc::dbus::sd_bus* bus,
    std::string_view path,
    int32_t depth,
    const mc::variants& interfaces
);
```

**参数**

- `bus`: DBus 连接对象指针
- `path`: 对象路径
- `depth`: 查询深度（0 表示仅当前层，-1 表示无限深度）
- `interfaces`: 需要查询的接口名称列表

**返回值**

返回子对象信息的 variant，格式为 dict，key 为路径，value 为对象信息。

**示例**

```cpp
auto result = mc::mdb::get_sub_objects(
    &bus,
    "/redfish/v1/Systems",
    1,
    {"xyz.openbmc_project.Inventory.Item"}
);
```

---

#### get_sub_paths

获取指定路径下的所有子路径。

**函数签名**

```cpp
mc::variant get_sub_paths(
    mc::dbus::sd_bus* bus,
    std::string_view path,
    int32_t depth,
    const mc::variants& interfaces
);
```

**参数**

- `bus`: DBus 连接对象指针
- `path`: 对象路径
- `depth`: 查询深度（0 表示仅当前层，-1 表示无限深度）
- `interfaces`: 接口名称列表（用于过滤）

**返回值**

返回路径列表的 variant，格式为 array。

**示例**

```cpp
auto result = mc::mdb::get_sub_paths(
    &bus,
    "/redfish/v1",
    2,
    {}
);
```

---

#### get_parent_objects

获取指定路径的所有父对象。

**函数签名**

```cpp
mc::variant get_parent_objects(
    mc::dbus::sd_bus* bus,
    std::string_view path,
    const mc::variants& interfaces
);
```

**参数**

- `bus`: DBus 连接对象指针
- `path`: 对象路径
- `interfaces`: 需要查询的接口名称列表

**返回值**

返回父对象信息的 variant，格式为 dict。

---

### 服务查询接口

#### get_service_name

根据发送者获取服务名称。

**函数签名**

```cpp
mc::variant get_service_name(
    mc::dbus::sd_bus* bus,
    std::string_view sender
);
```

**参数**

- `bus`: DBus 连接对象指针
- `sender`: 发送者名称（通常是 DBus 唯一名称）

**返回值**

返回服务名称的 variant，格式为 string。

---

#### get_service_names

获取所有已注册的服务名称。

**函数签名**

```cpp
mc::variant get_service_names(mc::dbus::sd_bus* bus);
```

**参数**

- `bus`: DBus 连接对象指针

**返回值**

返回服务名称列表的 variant，格式为 array。

---

### 路径查询接口

#### get_path

根据接口名称和过滤条件查询对象路径（支持缓存）。

**函数签名**

```cpp
mc::variant get_path(
    mc::dbus::sd_bus* bus,
    std::string_view interface,
    std::string_view filter,
    bool ignore_case,
    bool enable_cache
);
```

**参数**

- `bus`: DBus 连接对象指针
- `interface`: 接口名称
- `filter`: 过滤条件（JSON 格式的 dict，例如 `{"Name": "eth0"}`）
- `ignore_case`: 是否忽略大小写
- `enable_cache`: 是否启用缓存机制

**返回值**

返回对象路径的 variant，格式为 string。

**缓存机制**

当 `enable_cache` 为 `true` 时：

1. **缓存查找**: 首先在本地缓存中查找，如果命中则直接返回
2. **远程调用**: 如果缓存未命中，则调用 MDB 服务的 GetPath 方法
3. **订阅信号**: 创建对以下 DBus 信号的订阅：
   - `NameOwnerChanged`: 监听服务退出
   - `InterfacesRemoved`: 监听接口移除
   - `PropertiesChanged`: 监听属性变更
4. **二次确认**: 在订阅完成后再次调用 GetPath，确保在订阅期间值未发生变化
5. **自动失效**: 当接收到相关信号时，自动清除对应的缓存条目

**示例**

```cpp
// 不使用缓存
auto path = mc::mdb::get_path(
    &bus,
    "xyz.openbmc_project.Network.EthernetInterface",
    R"({"InterfaceName": "eth0"})",
    false,
    false
);

// 使用缓存
auto cached_path = mc::mdb::get_path(
    &bus,
    "xyz.openbmc_project.Network.EthernetInterface",
    R"({"InterfaceName": "eth0"})",
    false,
    true
);
```

---

#### get_interface_owners

获取实现指定接口的所有对象路径。

**函数签名**

```cpp
mc::variant get_interface_owners(
    mc::dbus::sd_bus* bus,
    std::string_view interface
);
```

**参数**

- `bus`: DBus 连接对象指针
- `interface`: 接口名称

**返回值**

返回路径列表的 variant，格式为 array。

---

#### is_valid_path

验证路径是否有效。

**函数签名**

```cpp
mc::variant is_valid_path(
    mc::dbus::sd_bus* bus,
    std::string_view path,
    bool ignore_case
);
```

**参数**

- `bus`: DBus 连接对象指针
- `path`: 待验证的路径
- `ignore_case`: 是否忽略大小写

**返回值**

返回布尔值的 variant，true 表示路径有效。

---

#### get_sub_paths_paging

分页获取子路径。

**函数签名**

```cpp
mc::variant get_sub_paths_paging(
    mc::dbus::sd_bus* bus,
    std::string_view path,
    int32_t depth,
    const mc::variants& interfaces,
    int32_t skip,
    int32_t top
);
```

**参数**

- `bus`: DBus 连接对象指针
- `path`: 对象路径
- `depth`: 查询深度
- `interfaces`: 接口名称列表
- `skip`: 跳过的条目数
- `top`: 返回的最大条目数

**返回值**

返回路径列表的 variant，格式为 array。

---

### 类和对象接口

#### get_classes

获取服务中的所有类。

**函数签名**

```cpp
mc::variant get_classes(
    mc::dbus::sd_bus* bus,
    std::string_view service
);
```

**参数**

- `bus`: DBus 连接对象指针
- `service`: 服务名称

**返回值**

返回类名列表的 variant，格式为 array。

---

#### get_object_list

获取指定类的所有对象。

**函数签名**

```cpp
mc::variant get_object_list(
    mc::dbus::sd_bus* bus,
    std::string_view class_name
);
```

**参数**

- `bus`: DBus 连接对象指针
- `class_name`: 类名称

**返回值**

返回对象列表的 variant，格式为 array。

---

#### get_object_owner

获取对象的拥有者服务。

**函数签名**

```cpp
mc::variant get_object_owner(
    mc::dbus::sd_bus* bus,
    std::string_view object_name
);
```

**参数**

- `bus`: DBus 连接对象指针
- `object_name`: 对象名称

**返回值**

返回服务名称的 variant，格式为 string。

---

#### get_matched_objects

根据模式匹配获取对象。

**函数签名**

```cpp
mc::variant get_matched_objects(
    mc::dbus::sd_bus* bus,
    std::string_view object_name,
    std::string_view interface_pattern
);
```

**参数**

- `bus`: DBus 连接对象指针
- `object_name`: 对象名称
- `interface_pattern`: 接口名称模式（支持通配符）

**返回值**

返回匹配的对象列表的 variant，格式为 array。

---

#### get_traced_object

获取追踪的对象信息。

**函数签名**

```cpp
mc::variant get_traced_object(mc::dbus::sd_bus* bus);
```

**参数**

- `bus`: DBus 连接对象指针

**返回值**

返回追踪对象信息的 variant，格式为 dict。

---

### 缓存管理接口

#### clear_cache

清除所有缓存的路径信息和信号订阅。

**函数签名**

```cpp
void clear_cache();
```

**参数**

- `bus`: DBus 连接对象指针（用于移除信号订阅）

**说明**

此函数会：
1. 清除所有缓存的路径条目
2. 移除所有 DBus 信号订阅
3. 清除所有索引数据

通常在以下情况调用：
- 系统重新配置后
- 检测到数据不一致时
- 内存优化需要时

**示例**

```cpp
// 清除所有缓存
mc::mdb::clear_cache();
```

---

#### set_max_cache_size

设置 get_path 缓存的最大数量。

**函数签名**

```cpp
void set_max_cache_size(size_t max_size);
```

**参数**

- `max_size`: 最大缓存数量，0 表示不限制

**说明**

此函数用于配置 get_path 方法的最大缓存数量，默认值为 400。

- 当设置为 0 时，表示不限制缓存数量
- 当缓存达到上限时，将采用 LRU（Least Recently Used）策略淘汰最久未使用的条目
- 该配置是全局的，对所有 DBus 连接有效

**示例**

```cpp
// 设置最大缓存数量为 200
mc::mdb::service::set_max_cache_size(200);

// 设置为不限制
mc::mdb::service::set_max_cache_size(0);
```

---

#### get_max_cache_size

获取当前设置的最大缓存数量。

**函数签名**

```cpp
size_t get_max_cache_size();
```

**返回值**

返回最大缓存数量，0 表示不限制。

**示例**

```cpp
size_t max_size = mc::mdb::service::get_max_cache_size();
ilog("当前最大缓存数量: ${size}", ("size", max_size));
```

---

#### get_cache_size

获取当前缓存的条目数量。

**函数签名**

```cpp
size_t get_cache_size();
```

**返回值**

返回当前缓存的条目数量。

**说明**

此函数用于监控缓存使用情况，可用于：
- 性能分析
- 内存使用监控
- 缓存命中率计算

**示例**

```cpp
size_t current_size = mc::mdb::service::get_cache_size();
size_t max_size = mc::mdb::service::get_max_cache_size();
ilog("缓存使用: ${current}/${max}", ("current", current_size)("max", max_size));
```

---

## 使用注意事项

### 缓存策略

1. **何时使用缓存**: 对于频繁查询且不经常变化的路径，应启用缓存
2. **何时不使用缓存**: 对于可能频繁变化的路径，应禁用缓存
3. **手动失效**: 在已知数据变化时，可调用 `clear_cache` 手动清除缓存
4. **缓存上限**: 默认最大缓存数量为 400，可通过 `set_max_cache_size` 配置
5. **LRU 淘汰**: 当缓存达到上限时，自动淘汰最久未使用的条目

### 线程安全

- 所有函数都是线程安全的
- 内部使用互斥锁保护缓存数据结构
- 可以在多线程环境中安全调用

### 性能考虑

1. **缓存命中**: 缓存命中时响应时间 < 1μs
2. **缓存未命中**: 需要进行 DBus 调用，响应时间取决于网络和服务负载
3. **信号订阅**: 首次查询时建立订阅，后续查询共享订阅
4. **LRU 淘汰开销**: 当缓存满时，淘汰操作需要遍历所有缓存条目找到最久未使用的条目，时间复杂度 O(n)
5. **内存使用**: 每个缓存条目约占 200-500 字节（取决于路径和过滤条件长度），默认 400 个条目约占 80-200KB

### 错误处理

所有函数在失败时都会抛出异常，调用者应使用 try-catch 捕获：

```cpp
try {
    auto path = mc::mdb::get_path(&bus, interface, filter, false, true);
    // 使用 path
} catch (const mc::exception& e) {
    // 处理异常
    elog("查询路径失败: ${error}", ("error", e.what()));
}
```

## 相关文档

- [MDB Service 详细设计](../../11.mdb_service_design.md)
- [MDB Service Lua API](../../lua_api/mdb.md#mdb_service)
- [Privilege API](mdb_privilege.md)
