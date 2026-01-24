# MDB Lua API

## 概述

MDB 模块的 Lua API 提供了对 MDB Service 和 Privilege 功能的完整访问。该 API 允许 Lua 脚本查询 D-Bus 对象、管理缓存、验证权限等。

## 模块加载

```lua
local mdb = require("mdb")
```

该模块包含两个子模块：
- `mdb.service`: MDB 服务客户端接口
- `mdb.privilege`: 权限验证接口

---

## mdb.service 模块

MDB Service 模块提供对 `bmc.kepler.maca` D-Bus 服务的访问接口。

### 对象查询

#### get_object

获取指定路径和接口的对象信息。

**语法**

```lua
local result = mdb.service.get_object(conn, path, interfaces)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `path`: 对象路径字符串
- `interfaces`: 接口名称表（数组）

**返回值**

返回对象信息（table）。

**示例**

```lua
local conn = ... -- DBusConnection对象
local interfaces = {"org.freedesktop.DBus"}
local result = mdb.service.get_object(conn, "/org/freedesktop/DBus", interfaces)
```

---

#### get_sub_objects

获取指定路径的子对象。

**语法**

```lua
local result = mdb.service.get_sub_objects(conn, path, depth, interfaces)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `path`: 对象路径字符串
- `depth`: 查询深度（整数，-1 表示无限深度）
- `interfaces`: 接口名称表（数组）

**返回值**

返回子对象信息表。

**示例**

```lua
local result = mdb.service.get_sub_objects(conn, "/", 1, {})
```

---

#### get_sub_paths

获取指定路径的子路径列表。

**语法**

```lua
local result = mdb.service.get_sub_paths(conn, path, depth, interfaces)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `path`: 对象路径字符串
- `depth`: 查询深度（整数）
- `interfaces`: 接口名称表（数组）

**返回值**

返回子路径列表（table）。

**示例**

```lua
local paths = mdb.service.get_sub_paths(conn, "/", 1, {})
for _, path in ipairs(paths) do
    print(path)
end
```

---

#### get_sub_paths_paging

分页获取子路径列表。

**语法**

```lua
local result = mdb.service.get_sub_paths_paging(conn, path, depth, interfaces, skip, top)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `path`: 对象路径字符串
- `depth`: 查询深度（整数）
- `interfaces`: 接口名称表（数组）
- `skip`: 跳过的数量（整数）
- `top`: 获取的数量（整数）

**返回值**

返回分页后的子路径列表。

**示例**

```lua
-- 跳过前10个，获取20个
local result = mdb.service.get_sub_paths_paging(conn, "/", 1, {}, 10, 20)
```

---

#### get_parent_objects

获取指定路径的父对象。

**语法**

```lua
local result = mdb.service.get_parent_objects(conn, path, interfaces)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `path`: 对象路径字符串
- `interfaces`: 接口名称表（数组）

**返回值**

返回父对象信息。

---

### 服务发现

#### get_service_names

获取所有已注册的服务名称列表。

**语法**

```lua
local services = mdb.service.get_service_names(conn)
```

**参数**

- `conn`: DBusConnection lightuserdata

**返回值**

返回服务名称数组。

**示例**

```lua
local services = mdb.service.get_service_names(conn)
for _, service in ipairs(services) do
    print(service)
end
```

---

#### get_service_name

根据发送者名称获取服务名称。

**语法**

```lua
local service_name = mdb.service.get_service_name(conn, sender)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `sender`: 发送者名称字符串

**返回值**

返回服务名称字符串。

---

#### get_interface_owners

获取实现指定接口的所有对象的拥有者列表。

**语法**

```lua
local owners = mdb.service.get_interface_owners(conn, interface)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `interface`: 接口名称字符串

**返回值**

返回拥有者列表（table）。

---

### 路径查询

#### get_path

根据接口和过滤条件查找对象路径（支持缓存）。

**语法**

```lua
local path = mdb.service.get_path(conn, interface, filter, ignore_case, enable_cache)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `interface`: 接口名称字符串
- `filter`: 过滤条件字符串（JSON 格式），如 `"{\"Property\":\"Value\"}"`
- `ignore_case`: 是否忽略大小写（布尔值）
- `enable_cache`: 是否启用缓存（布尔值）

**返回值**

返回符合条件的对象路径字符串。

**示例**

```lua
-- 使用缓存查询
local path = mdb.service.get_path(
    conn,
    "xyz.openbmc_project.Inventory.Item.Cpu",
    '{"ProcessorType":"CPU"}',
    false,  -- 精确匹配
    true    -- 启用缓存
)

-- 不使用缓存（实时查询）
local path2 = mdb.service.get_path(
    conn,
    "xyz.openbmc_project.Inventory.Item.Cpu",
    "{}",
    true,   -- 模糊匹配
    false   -- 禁用缓存
)
```

---

#### is_valid_path

验证指定路径是否有效。

**语法**

```lua
local is_valid = mdb.service.is_valid_path(conn, path, ignore_case)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `path`: 对象路径字符串
- `ignore_case`: 是否忽略大小写（布尔值）

**返回值**

返回布尔值，`true` 表示路径有效。

**示例**

```lua
local is_valid = mdb.service.is_valid_path(conn, "/org/freedesktop/DBus", false)
if is_valid then
    print("路径有效")
end
```

---

### 类和对象管理

#### get_classes

获取指定服务的类列表。

**语法**

```lua
local classes = mdb.service.get_classes(conn, service)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `service`: 服务名称字符串

**返回值**

返回类名称数组。

---

#### get_object_list

获取指定类的对象列表。

**语法**

```lua
local objects = mdb.service.get_object_list(conn, class_name)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `class_name`: 类名称字符串

**返回值**

返回对象列表数组。

---

#### get_object_owner

获取指定对象的拥有者。

**语法**

```lua
local owner = mdb.service.get_object_owner(conn, object_name)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `object_name`: 对象名称字符串

**返回值**

返回拥有者信息。

---

#### get_matched_objects

根据对象名称和接口模式获取匹配的对象列表。

**语法**

```lua
local objects = mdb.service.get_matched_objects(conn, object_name, interface_pattern)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `object_name`: 对象名称字符串
- `interface_pattern`: 接口模式字符串（支持通配符）

**返回值**

返回匹配的对象列表。

---

#### get_traced_object

获取追踪对象信息（用于调试）。

**语法**

```lua
local traced = mdb.service.get_traced_object(conn)
```

**参数**

- `conn`: DBusConnection lightuserdata

**返回值**

返回追踪对象信息。

---

### 缓存管理

#### clear_cache

清理所有缓存（包括信号订阅）。

**语法**

```lua
mdb.service.clear_cache()
```

**参数**

- `conn`: DBusConnection lightuserdata

**示例**

```lua
mdb.service.clear_cache()
print("缓存已清理")
```

---

#### set_max_cache_size

设置缓存的最大条目数量。

**语法**

```lua
mdb.service.set_max_cache_size(max_size)
```

**参数**

- `max_size`: 最大缓存数量（整数）
  - `0`: 不限制缓存大小
  - `> 0`: 限制最大条目数

**默认值**: 400

**示例**

```lua
-- 限制缓存为100条
mdb.service.set_max_cache_size(100)

-- 不限制缓存大小
mdb.service.set_max_cache_size(0)
```

---

#### get_max_cache_size

获取当前设置的最大缓存数量。

**语法**

```lua
local max_size = mdb.service.get_max_cache_size()
```

**返回值**

返回最大缓存数量（整数），`0` 表示不限制。

**示例**

```lua
local max_size = mdb.service.get_max_cache_size()
print("最大缓存数量: " .. max_size)
```

---

#### get_cache_size

获取当前缓存的条目数量。

**语法**

```lua
local cache_size = mdb.service.get_cache_size()
```

**返回值**

返回当前缓存的条目数量（整数）。

**示例**

```lua
local cache_size = mdb.service.get_cache_size()
print("当前缓存数量: " .. cache_size)
```

---

## mdb.privilege 模块

Privilege 模块提供权限验证功能。

### 常量

#### 权限级别常量

```lua
mdb.privilege.READ_ONLY        -- 只读权限 (1)
mdb.privilege.DIAGNOSE_MGMT    -- 诊断管理权限 (2)
mdb.privilege.SECURITY_MGMT    -- 安全管理权限 (4)
mdb.privilege.BASIC_SETTING    -- 基础设置权限 (8)
mdb.privilege.USER_MGMT        -- 用户管理权限 (16)
mdb.privilege.POWER_MGMT       -- 电源管理权限 (32)
mdb.privilege.VMM_MGMT         -- VMM管理权限 (64)
mdb.privilege.KVM_MGMT         -- KVM管理权限 (128)
mdb.privilege.CONFIGURE_SELF   -- 自我配置权限 (256)
```

#### 认证状态常量

```lua
mdb.privilege.NO_AUTH          -- 无需认证 (0)
mdb.privilege.AUTH_REQUIRED    -- 需要认证 (1)
```

---

### 函数

#### get_privilege_str

将权限数组转换为权限值的字符串表示。

**语法**

```lua
local priv_str = mdb.privilege.get_privilege_str(privileges)
```

**参数**

- `privileges`: 权限值表（数组）

**返回值**

返回所有权限值按位或运算后的字符串表示。

**示例**

```lua
local privileges = {
    mdb.privilege.READ_ONLY,
    mdb.privilege.BASIC_SETTING,
    mdb.privilege.USER_MGMT
}

local priv_str = mdb.privilege.get_privilege_str(privileges)
-- priv_str = "25" (1 | 8 | 16 = 25)
print("组合权限值: " .. priv_str)
```

---

#### validate

验证当前用户是否具有指定的权限。

**语法**

```lua
mdb.privilege.validate(expected_privilege)
```

**参数**

- `expected_privilege`: 期望的权限值（整数，可以是单个权限或多个权限的按位或）

**异常**

- 权限不足时抛出异常
- 需要修改密码时抛出异常

**示例**

```lua
-- 验证是否具有用户管理权限
local ok, err = pcall(function()
    mdb.privilege.validate(mdb.privilege.USER_MGMT)
end)

if not ok then
    print("权限验证失败: " .. err)
end

-- 验证组合权限
local required = bit.bor(mdb.privilege.BASIC_SETTING, mdb.privilege.POWER_MGMT)
mdb.privilege.validate(required)
```

---

## 完整示例

### 示例 1: 查询 CPU 信息

```lua
local mdb = require("mdb")

function get_cpu_info(conn)
    -- 查询CPU对象路径
    local cpu_path = mdb.service.get_path(
        conn,
        "xyz.openbmc_project.Inventory.Item.Cpu",
        '{"ProcessorType":"CPU"}',
        false,  -- 精确匹配
        true    -- 启用缓存
    )
    
    if not cpu_path or cpu_path == "" then
        print("未找到CPU对象")
        return nil
    end
    
    -- 获取CPU对象信息
    local interfaces = {"xyz.openbmc_project.Inventory.Item.Cpu"}
    local cpu_info = mdb.service.get_object(conn, cpu_path, interfaces)
    
    return cpu_info
end
```

---

### 示例 2: 权限验证和用户操作

```lua
local mdb = require("mdb")

function delete_user(conn, username)
    -- 验证用户管理权限
    local ok, err = pcall(function()
        mdb.privilege.validate(mdb.privilege.USER_MGMT)
    end)
    
    if not ok then
        return {
            success = false,
            error = "权限不足: " .. err
        }
    end
    
    -- 执行删除操作
    -- ...
    
    return {
        success = true,
        message = "用户 " .. username .. " 已删除"
    }
end
```

---

### 示例 3: 缓存管理

```lua
local mdb = require("mdb")

function manage_cache()
    -- 获取缓存状态
    local current_size = mdb.service.get_cache_size()
    local max_size = mdb.service.get_max_cache_size()
    
    print(string.format("缓存使用情况: %d/%d", current_size, max_size))
    
    -- 如果缓存使用率高，调整大小
    if max_size > 0 and current_size > max_size * 0.9 then
        print("缓存使用率高，扩大缓存")
        mdb.service.set_max_cache_size(max_size * 2)
    end
    
    -- 清理缓存
    if current_size > 1000 then
        print("缓存过大，清理缓存")
        mdb.service.clear_cache()
    end
end
```

---

### 示例 4: 遍历所有服务

```lua
local mdb = require("mdb")

function list_all_services(conn)
    -- 获取所有服务名称
    local services = mdb.service.get_service_names(conn)
    
    print("已注册的服务:")
    for _, service in ipairs(services) do
        print("  - " .. service)
        
        -- 获取服务的类列表
        local classes = mdb.service.get_classes(conn, service)
        if classes and #classes > 0 then
            print("    类:")
            for _, class in ipairs(classes) do
                print("      - " .. class)
            end
        end
    end
end
```

---

### 示例 5: 分页查询子路径

```lua
local mdb = require("mdb")

function paginate_sub_paths(conn, base_path, page_size)
    local skip = 0
    local page = 1
    
    while true do
        -- 分页获取子路径
        local paths = mdb.service.get_sub_paths_paging(
            conn,
            base_path,
            1,      -- depth
            {},     -- interfaces
            skip,   -- skip
            page_size  -- top
        )
        
        if not paths or #paths == 0 then
            break
        end
        
        print(string.format("第 %d 页:", page))
        for _, path in ipairs(paths) do
            print("  " .. path)
        end
        
        skip = skip + page_size
        page = page + 1
    end
end

-- 使用示例
paginate_sub_paths(conn, "/", 10)
```

---

## 错误处理

所有 MDB Lua API 函数在出错时会抛出 Lua 错误，建议使用 `pcall` 进行错误处理：

```lua
local ok, result = pcall(function()
    return mdb.service.get_path(conn, "interface", "{}", false, true)
end)

if not ok then
    print("操作失败: " .. result)
else
    print("获取路径成功: " .. result)
end
```

---

## 性能提示

1. **使用缓存**: 频繁查询相同条件时启用缓存
2. **批量操作**: 尽可能使用分页接口减少调用次数
3. **合理设置缓存大小**: 根据系统负载调整缓存上限
4. **定期清理**: 在适当时机清理缓存释放资源

---

## 相关文档

- [MDB Service C++ API](./mdb_service.md)
- [MDB Privilege C++ API](./mdb_privilege.md)
- [Lua C API 设计指南](../lua/c_api_design.md)
