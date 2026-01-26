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

## 子模块文档

### MDB Service

MDB Service 模块提供了与 BMC MDB 服务（`bmc.kepler.maca`）通信的接口，包括：

- 对象查询接口（get_object, get_sub_objects, get_sub_paths, get_parent_objects）
- 服务查询接口（get_service_name, get_service_names）
- 路径查询接口（get_path, get_interface_owners, is_valid_path, get_sub_paths_paging）
- 类和对象接口（get_classes, get_object_list, get_object_owner, get_matched_objects, get_traced_object）
- 缓存管理接口（clear_cache, set_max_cache_size, get_max_cache_size, get_cache_size）

**详细文档**: [MDB Service Lua API](mdb_service.md)

### Privilege

Privilege 模块提供了权限验证功能，包括：

- 权限常量（ReadOnly, DiagnoseMgmt, SecurityMgmt, BasicSetting, UserMgmt, PowerMgmt, VMMMgmt, KVMMgmt, ConfigureSelf）
- 认证状态常量（NoAuth, AuthRequired）
- 权限验证函数（validate, get_privilege_str）

**详细文档**: [MDB Privilege Lua API](mdb_privilege.md)

---

## 快速开始

### 示例 1: 查询对象路径

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

print("网络接口路径:", path)
conn:close()
```

### 示例 2: 权限验证

```lua
local lmdb = require('lmdb')

-- 验证用户管理权限
local ok, err = pcall(function()
    lmdb.privilege.validate(lmdb.privilege.UserMgmt)
end)

if ok then
    print("权限验证成功，可以执行用户管理操作")
else
    print("权限验证失败:", err)
end
```

### 示例 3: 获取子对象

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
    print("路径:", path)
    for key, value in pairs(obj) do
        print("  " .. key .. ":", value)
    end
end

conn:close()
```

---

## 模块结构

```lua
lmdb
├── mdb_service              -- MDB 服务接口
│   ├── get_object()
│   ├── get_sub_objects()
│   ├── get_sub_paths()
│   ├── get_parent_objects()
│   ├── get_service_name()
│   ├── get_service_names()
│   ├── get_path()
│   ├── get_interface_owners()
│   ├── is_valid_path()
│   ├── get_sub_paths_paging()
│   ├── get_classes()
│   ├── get_object_list()
│   ├── get_object_owner()
│   ├── get_matched_objects()
│   ├── get_traced_object()
│   ├── clear_cache()
│   ├── set_max_cache_size()
│   ├── get_max_cache_size()
│   └── get_cache_size()
│
└── privilege                -- 权限验证接口
    ├── ReadOnly             -- 常量
    ├── DiagnoseMgmt         -- 常量
    ├── SecurityMgmt         -- 常量
    ├── BasicSetting         -- 常量
    ├── UserMgmt             -- 常量
    ├── PowerMgmt            -- 常量
    ├── VMMMgmt              -- 常量
    ├── KVMMgmt              -- 常量
    ├── ConfigureSelf        -- 常量
    ├── NoAuth               -- 常量
    ├── AuthRequired         -- 常量
    ├── get_privilege_str()
    └── validate()
```

---

## 使用场景

### RESTful API 开发

```lua
local lmdb = require('lmdb')

function handle_get_user(user_id)
    -- 验证只读权限
    lmdb.privilege.validate(lmdb.privilege.ReadOnly)
    
    -- 查询用户路径
    local path = lmdb.mdb_service.get_path(
        conn:get_raw(),
        "xyz.openbmc_project.User.Manager",
        string.format('{"UserId": "%s"}', user_id),
        false,
        true
    )
    
    -- 获取用户对象
    local user = lmdb.mdb_service.get_object(
        conn:get_raw(),
        path,
        {"xyz.openbmc_project.User.Attributes"}
    )
    
    return user
end

function handle_delete_user(user_id)
    -- 验证用户管理权限
    lmdb.privilege.validate(lmdb.privilege.UserMgmt)
    
    -- 执行删除操作
    -- ...
end
```

### 系统监控

```lua
local lmdb = require('lmdb')

function monitor_system()
    local conn = ldbus.blocking.open_system()
    
    -- 获取所有传感器
    local sensors = lmdb.mdb_service.get_interface_owners(
        conn:get_raw(),
        "xyz.openbmc_project.Sensor.Value"
    )
    
    -- 读取传感器值
    for _, path in ipairs(sensors) do
        local obj = lmdb.mdb_service.get_object(
            conn:get_raw(),
            path,
            {"xyz.openbmc_project.Sensor.Value"}
        )
        
        print(string.format("%s: %s", path, obj.Value))
    end
    
    conn:close()
end
```

### 配置管理

```lua
local lmdb = require('lmdb')

function update_system_config(config_data)
    -- 验证基础设置权限
    lmdb.privilege.validate(lmdb.privilege.BasicSetting)
    
    -- 查询配置对象路径
    local path = lmdb.mdb_service.get_path(
        conn:get_raw(),
        "xyz.openbmc_project.Network.SystemConfiguration",
        '{}',
        false,
        true
    )
    
    if path then
        -- 更新配置
        -- ...
        print("配置更新成功")
    else
        print("未找到配置对象")
    end
end
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
5. **权限检查**: 在操作前进行权限验证，避免不必要的操作

---

## 相关文档

- [MDB Service Lua API](mdb_service.md)
- [MDB Privilege Lua API](mdb_privilege.md)
- [MDB Service C++ API](../../api/mdb/mdb_service.md)
- [MDB Privilege C++ API](../../api/mdb/mdb_privilege.md)
- [MDB Service 详细设计](../../11.mdb_service_design.md)
- [Privilege 使用指南](../../mdb_privilege_usage.md)
