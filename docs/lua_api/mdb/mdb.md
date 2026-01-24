# MDB Lua API

## 概述

MDB Lua API 提供了 MDB Service 和 Privilege 模块的 Lua 绑定，使 Lua 脚本能够方便地访问 BMC 的 MDB 服务和进行权限验证。

## 加载模块

```lua
local lmdb = require('lmdb')
```

模块包含两个子模块：
- `lmdb.mdb_service`: MDB 服务接口
- `lmdb.privilege`: 权限验证接口

---

## mdb_service 模块

### 概述

`mdb_service` 模块提供了与 BMC MDB 服务（`bmc.kepler.maca`）通信的接口。

### 对象查询接口

#### get_object

获取指定路径的对象信息。

**函数签名**

```lua
object = lmdb.mdb_service.get_object(conn, path, interfaces)
```

**参数**

- `conn`: DBusConnection lightuserdata（从 ldbus 获取）
- `path`: 对象路径（字符串）
- `interfaces`: 接口名称列表（Lua table）

**返回值**

返回对象信息（table），包含对象的所有属性。

**示例**

```lua
local ldbus = require('ldbus')
local lmdb = require('lmdb')

local conn = ldbus.blocking.open_system()
local result = lmdb.mdb_service.get_object(
    conn:get_raw(),
    "/redfish/v1/Systems/1",
    {"xyz.openbmc_project.Inventory.Item.System"}
)

print("对象信息:", result)
```

---

#### get_sub_objects

获取指定路径下的所有子对象。

**函数签名**

```lua
objects = lmdb.mdb_service.get_sub_objects(conn, path, depth, interfaces)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `path`: 对象路径（字符串）
- `depth`: 查询深度（整数，0 表示仅当前层，-1 表示无限深度）
- `interfaces`: 接口名称列表（Lua table）

**返回值**

返回子对象信息（table），key 为路径，value 为对象信息。

**示例**

```lua
local result = lmdb.mdb_service.get_sub_objects(
    conn:get_raw(),
    "/redfish/v1/Systems",
    1,
    {"xyz.openbmc_project.Inventory.Item"}
)

for path, obj in pairs(result) do
    print("路径:", path)
    print("对象:", obj)
end
```

---

#### get_sub_paths

获取指定路径下的所有子路径。

**函数签名**

```lua
paths = lmdb.mdb_service.get_sub_paths(conn, path, depth, interfaces)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `path`: 对象路径（字符串）
- `depth`: 查询深度（整数）
- `interfaces`: 接口名称列表（Lua table）

**返回值**

返回路径列表（table 数组）。

**示例**

```lua
local paths = lmdb.mdb_service.get_sub_paths(
    conn:get_raw(),
    "/redfish/v1",
    2,
    {}
)

for i, path in ipairs(paths) do
    print(i, path)
end
```

---

#### get_parent_objects

获取指定路径的所有父对象。

**函数签名**

```lua
parents = lmdb.mdb_service.get_parent_objects(conn, path, interfaces)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `path`: 对象路径（字符串）
- `interfaces`: 接口名称列表（Lua table）

**返回值**

返回父对象信息（table）。

---

### 服务查询接口

#### get_service_name

根据发送者获取服务名称。

**函数签名**

```lua
service_name = lmdb.mdb_service.get_service_name(conn, sender)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `sender`: 发送者名称（字符串）

**返回值**

返回服务名称（字符串）。

---

#### get_service_names

获取所有已注册的服务名称。

**函数签名**

```lua
services = lmdb.mdb_service.get_service_names(conn)
```

**参数**

- `conn`: DBusConnection lightuserdata

**返回值**

返回服务名称列表（table 数组）。

**示例**

```lua
local services = lmdb.mdb_service.get_service_names(conn:get_raw())

for i, service in ipairs(services) do
    print("服务:", service)
end
```

---

### 路径查询接口

#### get_path

根据接口名称和过滤条件查询对象路径。

**函数签名**

```lua
path = lmdb.mdb_service.get_path(conn, interface, filter, ignore_case, enable_cache)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `interface`: 接口名称（字符串）
- `filter`: 过滤条件（JSON 字符串，例如 `'{"Name": "eth0"}'`）
- `ignore_case`: 是否忽略大小写（布尔值）
- `enable_cache`: 是否启用缓存（布尔值）

**返回值**

返回对象路径（字符串）。

**示例**

```lua
-- 不使用缓存
local path = lmdb.mdb_service.get_path(
    conn:get_raw(),
    "xyz.openbmc_project.Network.EthernetInterface",
    '{"InterfaceName": "eth0"}',
    false,
    false
)
print("路径:", path)

-- 使用缓存（推荐用于频繁查询的路径）
local cached_path = lmdb.mdb_service.get_path(
    conn:get_raw(),
    "xyz.openbmc_project.Network.EthernetInterface",
    '{"InterfaceName": "eth0"}',
    false,
    true
)
```

**缓存说明**

- 当 `enable_cache` 为 `true` 时，系统会自动管理缓存
- 缓存会在相关对象发生变化时自动失效
- 适合用于频繁查询且不常变化的路径

---

#### get_interface_owners

获取实现指定接口的所有对象路径。

**函数签名**

```lua
paths = lmdb.mdb_service.get_interface_owners(conn, interface)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `interface`: 接口名称（字符串）

**返回值**

返回路径列表（table 数组）。

**示例**

```lua
local paths = lmdb.mdb_service.get_interface_owners(
    conn:get_raw(),
    "xyz.openbmc_project.Network.EthernetInterface"
)

for i, path in ipairs(paths) do
    print("接口实例:", path)
end
```

---

#### is_valid_path

验证路径是否有效。

**函数签名**

```lua
valid = lmdb.mdb_service.is_valid_path(conn, path, ignore_case)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `path`: 待验证的路径（字符串）
- `ignore_case`: 是否忽略大小写（布尔值）

**返回值**

返回布尔值，`true` 表示路径有效。

**示例**

```lua
local valid = lmdb.mdb_service.is_valid_path(
    conn:get_raw(),
    "/redfish/v1/Systems/1",
    false
)

if valid then
    print("路径有效")
else
    print("路径无效")
end
```

---

#### get_sub_paths_paging

分页获取子路径。

**函数签名**

```lua
paths = lmdb.mdb_service.get_sub_paths_paging(conn, path, depth, interfaces, skip, top)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `path`: 对象路径（字符串）
- `depth`: 查询深度（整数）
- `interfaces`: 接口名称列表（Lua table）
- `skip`: 跳过的条目数（整数）
- `top`: 返回的最大条目数（整数）

**返回值**

返回路径列表（table 数组）。

**示例**

```lua
-- 获取前 10 个路径
local page1 = lmdb.mdb_service.get_sub_paths_paging(
    conn:get_raw(),
    "/redfish/v1/Systems",
    1,
    {},
    0,    -- skip
    10    -- top
)

-- 获取接下来的 10 个路径
local page2 = lmdb.mdb_service.get_sub_paths_paging(
    conn:get_raw(),
    "/redfish/v1/Systems",
    1,
    {},
    10,   -- skip
    10    -- top
)
```

---

### 类和对象接口

#### get_classes

获取服务中的所有类。

**函数签名**

```lua
classes = lmdb.mdb_service.get_classes(conn, service)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `service`: 服务名称（字符串）

**返回值**

返回类名列表（table 数组）。

---

#### get_object_list

获取指定类的所有对象。

**函数签名**

```lua
objects = lmdb.mdb_service.get_object_list(conn, class_name)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `class_name`: 类名称（字符串）

**返回值**

返回对象列表（table 数组）。

---

#### get_object_owner

获取对象的拥有者服务。

**函数签名**

```lua
owner = lmdb.mdb_service.get_object_owner(conn, object_name)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `object_name`: 对象名称（字符串）

**返回值**

返回服务名称（字符串）。

---

#### get_matched_objects

根据模式匹配获取对象。

**函数签名**

```lua
objects = lmdb.mdb_service.get_matched_objects(conn, object_name, interface_pattern)
```

**参数**

- `conn`: DBusConnection lightuserdata
- `object_name`: 对象名称（字符串）
- `interface_pattern`: 接口名称模式（字符串，支持通配符）

**返回值**

返回匹配的对象列表（table 数组）。

---

#### get_traced_object

获取追踪的对象信息。

**函数签名**

```lua
traced = lmdb.mdb_service.get_traced_object(conn)
```

**参数**

- `conn`: DBusConnection lightuserdata

**返回值**

返回追踪对象信息（table）。

---

## privilege 模块

### 概述

`privilege` 模块提供了权限验证功能。

### 常量

#### 权限常量

```lua
lmdb.privilege.READ_ONLY       -- 只读权限 (1)
lmdb.privilege.DIAGNOSE_MGMT   -- 诊断管理权限 (2)
lmdb.privilege.SECURITY_MGMT   -- 安全管理权限 (4)
lmdb.privilege.BASIC_SETTING   -- 基础设置权限 (8)
lmdb.privilege.USER_MGMT       -- 用户管理权限 (16)
lmdb.privilege.POWER_MGMT      -- 电源管理权限 (32)
lmdb.privilege.VMM_MGMT        -- VMM管理权限 (64)
lmdb.privilege.KVM_MGMT        -- KVM管理权限 (64)
lmdb.privilege.CONFIGURE_SELF  -- 自我配置权限 (128)
```

#### 认证状态常量

```lua
lmdb.privilege.NO_AUTH         -- 无需认证 (0)
lmdb.privilege.AUTH_REQUIRED   -- 需要认证 (1)
```

---

### 函数

#### get_privilege_str

将权限数组转换为权限值的字符串表示。

**函数签名**

```lua
priv_str = lmdb.privilege.get_privilege_str(privileges)
```

**参数**

- `privileges`: 权限值数组（Lua table）

**返回值**

返回所有权限值按位或运算后的字符串。

**示例**

```lua
local lmdb = require('lmdb')

local privs = {
    lmdb.privilege.READ_ONLY,
    lmdb.privilege.BASIC_SETTING,
    lmdb.privilege.USER_MGMT
}

local priv_str = lmdb.privilege.get_privilege_str(privs)
print("权限值:", priv_str)  -- 输出: "25" (1 | 8 | 16 = 25)
```

---

#### validate

验证当前用户是否具有指定的权限。

**函数签名**

```lua
lmdb.privilege.validate(expected_privilege)
```

**参数**

- `expected_privilege`: 期望的权限值（整数）

**异常**

如果权限不足，会抛出 Lua 错误。

**示例**

```lua
local lmdb = require('lmdb')

-- 验证用户管理权限
local ok, err = pcall(function()
    lmdb.privilege.validate(lmdb.privilege.USER_MGMT)
end)

if not ok then
    print("权限验证失败:", err)
else
    print("权限验证成功")
    -- 执行需要权限的操作
end
```

**组合权限验证**

```lua
-- 验证是否同时具有基础设置和电源管理权限
local required = bit.bor(
    lmdb.privilege.BASIC_SETTING,
    lmdb.privilege.POWER_MGMT
)

local ok, err = pcall(function()
    lmdb.privilege.validate(required)
end)
```

---

## 完整示例

### 示例 1: 查询网络接口路径

```lua
local ldbus = require('ldbus')
local lmdb = require('lmdb')

-- 创建 DBus 连接
local conn = ldbus.blocking.open_system()

-- 查询网络接口路径（使用缓存）
local path = lmdb.mdb_service.get_path(
    conn:get_raw(),
    "xyz.openbmc_project.Network.EthernetInterface",
    '{"InterfaceName": "eth0"}',
    false,  -- 不忽略大小写
    true    -- 启用缓存
)

if path then
    print("网络接口路径:", path)
    
    -- 获取接口详细信息
    local obj = lmdb.mdb_service.get_object(
        conn:get_raw(),
        path,
        {"xyz.openbmc_project.Network.EthernetInterface"}
    )
    
    print("接口信息:", obj)
else
    print("未找到网络接口")
end

conn:close()
```

### 示例 2: 权限验证和用户操作

```lua
local ldbus = require('ldbus')
local lmdb = require('lmdb')

function delete_user(username)
    -- 验证用户管理权限
    local ok, err = pcall(function()
        lmdb.privilege.validate(lmdb.privilege.USER_MGMT)
    end)
    
    if not ok then
        return nil, "权限不足: " .. tostring(err)
    end
    
    -- 执行删除用户操作
    -- ...
    
    return true
end

-- 调用函数
local success, err = delete_user("testuser")
if success then
    print("用户删除成功")
else
    print("用户删除失败:", err)
end
```

### 示例 3: 遍历所有系统对象

```lua
local ldbus = require('ldbus')
local lmdb = require('lmdb')

local conn = ldbus.blocking.open_system()

-- 获取所有子对象
local objects = lmdb.mdb_service.get_sub_objects(
    conn:get_raw(),
    "/redfish/v1/Systems",
    1,
    {"xyz.openbmc_project.Inventory.Item"}
)

-- 遍历对象
for path, obj in pairs(objects) do
    print("==================")
    print("路径:", path)
    
    -- 打印对象属性
    for key, value in pairs(obj) do
        print("  " .. key .. ":", value)
    end
end

conn:close()
```

### 示例 4: 分页查询

```lua
local ldbus = require('ldbus')
local lmdb = require('lmdb')

local conn = ldbus.blocking.open_system()

local page_size = 10
local page_num = 0

while true do
    local paths = lmdb.mdb_service.get_sub_paths_paging(
        conn:get_raw(),
        "/redfish/v1/Systems",
        1,
        {},
        page_num * page_size,  -- skip
        page_size              -- top
    )
    
    if #paths == 0 then
        break
    end
    
    print("第 " .. (page_num + 1) .. " 页:")
    for i, path in ipairs(paths) do
        print("  " .. i .. ": " .. path)
    end
    
    page_num = page_num + 1
end

conn:close()
```

---

## 错误处理

所有函数在失败时都会抛出 Lua 错误，建议使用 `pcall` 进行错误捕获：

```lua
local ok, result = pcall(function()
    return lmdb.mdb_service.get_path(
        conn:get_raw(),
        interface,
        filter,
        false,
        true
    )
end)

if ok then
    print("查询成功:", result)
else
    print("查询失败:", result)
end
```

---

## 性能建议

1. **使用缓存**: 对于频繁查询的路径，启用 `get_path` 的缓存功能
2. **批量查询**: 使用 `get_sub_objects` 批量获取对象，而不是逐个查询
3. **限制深度**: 在 `get_sub_objects` 和 `get_sub_paths` 中合理设置深度参数
4. **分页查询**: 对于大量数据，使用 `get_sub_paths_paging` 进行分页

---

## 相关文档

- [MDB Service C++ API](../api/mdb_service.md)
- [MDB Privilege C++ API](../api/mdb_privilege.md)
- [MDB Service 详细设计](../11.mdb_service_design.md)
- [MDB Privilege 使用指南](../mdb_privilege_usage.md)
