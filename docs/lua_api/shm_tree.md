# lshm_tree Lua API 文档

## 概述

`lshm_tree` 是共享内存对象树（shm_tree）的 Lua 绑定，提供 MDB 查询与 SHM 属性访问，对应 C++ 的 `mc::dbus::shm_tree` 静态方法及 bmc.kepler.Mdb、org.freedesktop.DBus.Properties、bmc.kepler.Object.Properties 的本地 handler。

## 模块加载

```lua
local lshm_tree = require('lshm_tree')
```

## MDB 查询函数

| Lua 函数 | 参数 | 返回值 |
|----------|------|--------|
| `get_mdb_object(path, interfaces)` | path: string；interfaces: 数组 | table 或 nil，`{service = {iface,...}}` |
| `get_mdb_sub_paths(path, depth, interfaces[, skip, top, ignore_case])` | path, depth, interfaces 必选；skip/top 默认 0；ignore_case 默认 false | 数组或 nil |
| `is_valid_mdb_path(path, ignore_case)` | path: string；ignore_case: boolean | boolean |
| `get_mdb_path(interface_name, filter_json, ignore_case)` | interface_name, filter_json: string；ignore_case: boolean | 多值：path, service_name |
| `get_mdb_interface_owners(interface_name)` | interface_name: string | 数组 `[[service, path],...]` |
| `get_mdb_service_name(sender)` | sender: string（unique_name） | string |
| `get_mdb_sub_objects(path, depth, interfaces)` | path, depth, interfaces（数组） | table 或 nil |
| `get_mdb_parent_objects(path, interfaces)` | path, interfaces（数组） | table 或 nil |
| `get_mdb_service_names()` | 无 | 数组 |
| `get_mdb_classes(service_name)` | service_name: string | 数组 |
| `get_mdb_object_list(class_name)` | class_name: string | 数组 |
| `get_mdb_object_owner(object_name)` | object_name: string | 数组 |
| `get_mdb_matched_objects(service_name, iface_pattern)` | service_name, iface_pattern: string | 数组 |

## SHM 属性访问函数

| Lua 函数 | 参数 | 返回值 |
|----------|------|--------|
| `call_shm_get_property(service, path, interface, property)` | 四个 string | 属性值或 nil |
| `call_shm_get_all_properties(service, path, interface)` | 三个 string | table 或 nil，`{prop=value,...}` |
| `call_shm_get_properties_by_names(service, path, interface, prop_names)` | service, path, interface: string；prop_names: 数组 | 多值：props_table, errors_table；失败为 nil |

## 示例

```lua
local lshm_tree = require('lshm_tree')

-- 检查路径是否存在
local ok = lshm_tree.is_valid_mdb_path("/com/example/Object", false)

-- 获取服务名列表
local names = lshm_tree.get_mdb_service_names()

-- 按路径和接口查对象
local obj = lshm_tree.get_mdb_object("/com/example/Object", {"com.example.Interface"})

-- 获取单个属性
local val = lshm_tree.call_shm_get_property("com.example.Svc", "/obj", "com.example.If", "Status")

-- 获取接口下全部属性
local props = lshm_tree.call_shm_get_all_properties("com.example.Svc", "/obj", "com.example.If")

-- 按属性名批量获取
local succ, errs = lshm_tree.call_shm_get_properties_by_names(
    "com.example.Svc", "/obj", "com.example.If", {"A", "B"})
```

## 错误处理

接口在参数错误或内部失败时会 `luaL_error`，可用 `pcall` 捕获：

```lua
local ok, err = pcall(lshm_tree.get_mdb_object, "/path", {"iface"})
if not ok then
    print("调用失败: " .. tostring(err))
end
```

## 相关文档

- [DBus 共享内存 API (C++)](../api/dbus/shm.md) — MDB 与 SHM 属性查询的 C++ 接口
- [dbus](dbus.md) — DBus Lua 绑定
