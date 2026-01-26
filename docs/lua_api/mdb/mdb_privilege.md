# MDB Privilege Lua API

## 概述

MDB Privilege 模块提供了权限验证功能的 Lua 绑定，使 Lua 脚本能够方便地进行权限验证和权限管理。

## 加载模块

```lua
local lmdb = require('lmdb')
local privilege = lmdb.privilege
```

---

## 常量

### 权限常量

```lua
lmdb.privilege.ReadOnly       -- 只读权限 (1)
lmdb.privilege.DiagnoseMgmt   -- 诊断管理权限 (2)
lmdb.privilege.SecurityMgmt   -- 安全管理权限 (4)
lmdb.privilege.BasicSetting   -- 基础设置权限 (8)
lmdb.privilege.UserMgmt       -- 用户管理权限 (16)
lmdb.privilege.PowerMgmt      -- 电源管理权限 (32)
lmdb.privilege.VMMMgmt        -- VMM管理权限 (64)
lmdb.privilege.KVMMgmt        -- KVM管理权限 (64)
lmdb.privilege.ConfigureSelf  -- 自我配置权限 (128)
```

**权限说明**

| 权限 | 值 | 说明 |
|------|-----|------|
| `ReadOnly` | 1 | 只读权限，可以查看信息但不能修改 |
| `DiagnoseMgmt` | 2 | 诊断管理权限，可以进行系统诊断操作 |
| `SecurityMgmt` | 4 | 安全管理权限，可以配置安全相关设置 |
| `BasicSetting` | 8 | 基础设置权限，可以修改基本配置 |
| `UserMgmt` | 16 | 用户管理权限，可以管理用户账户 |
| `PowerMgmt` | 32 | 电源管理权限，可以控制电源状态 |
| `VMMMgmt` / `KVMMgmt` | 64 | 虚拟机管理权限 |
| `ConfigureSelf` | 128 | 自我配置权限，用于首次登录后的密码修改 |

### 认证状态常量

```lua
lmdb.privilege.NoAuth         -- 无需认证 (0)
lmdb.privilege.AuthRequired   -- 需要认证 (1)
```

---

## 函数

### get_privilege_str

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
    lmdb.privilege.ReadOnly,
    lmdb.privilege.BasicSetting,
    lmdb.privilege.UserMgmt
}

local priv_str = lmdb.privilege.get_privilege_str(privs)
print("权限值:", priv_str)  -- 输出: "25" (1 | 8 | 16 = 25)
```

---

### validate

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
    lmdb.privilege.validate(lmdb.privilege.UserMgmt)
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
local bit = require('bit')
local required = bit.bor(
    lmdb.privilege.BasicSetting,
    lmdb.privilege.PowerMgmt
)

local ok, err = pcall(function()
    lmdb.privilege.validate(required)
end)
```

---

## 完整示例

### 示例 1: 权限验证和用户操作

```lua
local lmdb = require('lmdb')

function delete_user(username)
    -- 验证用户管理权限
    local ok, err = pcall(function()
        lmdb.privilege.validate(lmdb.privilege.UserMgmt)
    end)
    
    if not ok then
        return nil, "权限不足: " .. tostring(err)
    end
    
    -- 执行删除用户操作
    print("删除用户:", username)
    
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

### 示例 2: 多权限验证

```lua
local lmdb = require('lmdb')

function configure_system()
    -- 尝试验证基础设置权限
    local ok = pcall(function()
        lmdb.privilege.validate(lmdb.privilege.BasicSetting)
    end)
    
    if not ok then
        -- 如果没有基础设置权限，尝试安全管理权限
        local ok2 = pcall(function()
            lmdb.privilege.validate(lmdb.privilege.SecurityMgmt)
        end)
        
        if not ok2 then
            error("需要基础设置或安全管理权限")
        end
    end
    
    -- 执行配置操作
    print("执行系统配置...")
end
```

### 示例 3: 权限等级判断

```lua
local lmdb = require('lmdb')

function check_user_role()
    -- 检查是否为管理员（具有用户管理权限）
    local is_admin = pcall(function()
        lmdb.privilege.validate(lmdb.privilege.UserMgmt)
    end)
    
    if is_admin then
        print("当前用户: 管理员")
        return "admin"
    end
    
    -- 检查是否为操作员（具有基础设置权限）
    local is_operator = pcall(function()
        lmdb.privilege.validate(lmdb.privilege.BasicSetting)
    end)
    
    if is_operator then
        print("当前用户: 操作员")
        return "operator"
    end
    
    -- 只读用户
    print("当前用户: 访客")
    return "guest"
end

local role = check_user_role()
```

### 示例 4: RESTful API 权限控制

```lua
local lmdb = require('lmdb')

function handle_post_request(path, data)
    -- 根据不同的路径验证不同的权限
    if path:match("/api/users") then
        -- 用户管理接口需要用户管理权限
        lmdb.privilege.validate(lmdb.privilege.UserMgmt)
        
    elseif path:match("/api/system/power") then
        -- 电源管理接口需要电源管理权限
        lmdb.privilege.validate(lmdb.privilege.PowerMgmt)
        
    elseif path:match("/api/system/config") then
        -- 系统配置接口需要基础设置权限
        lmdb.privilege.validate(lmdb.privilege.BasicSetting)
    end
    
    -- 执行实际操作
    print("处理请求:", path)
end

-- 使用 pcall 捕获权限错误
local ok, err = pcall(handle_post_request, "/api/users", {})
if not ok then
    print("权限错误:", err)
end
```

---

## 错误处理

所有权限验证函数在失败时都会抛出 Lua 错误，建议使用 `pcall` 进行错误捕获：

```lua
local ok, result = pcall(function()
    lmdb.privilege.validate(lmdb.privilege.UserMgmt)
end)

if ok then
    print("权限验证成功")
else
    print("权限验证失败:", result)
end
```

---

## 权限组合技巧

### 使用 bit 库进行权限组合

```lua
local bit = require('bit')
local lmdb = require('lmdb')

-- 组合多个权限
local combined = bit.bor(
    lmdb.privilege.ReadOnly,
    lmdb.privilege.BasicSetting,
    lmdb.privilege.PowerMgmt
)

-- 验证组合权限
lmdb.privilege.validate(combined)
```

### 检查是否包含特定权限

```lua
local bit = require('bit')

function has_privilege(user_priv, check_priv)
    return bit.band(user_priv, check_priv) == check_priv
end

-- 使用示例
local user_privileges = bit.bor(
    lmdb.privilege.ReadOnly,
    lmdb.privilege.BasicSetting
)

if has_privilege(user_privileges, lmdb.privilege.BasicSetting) then
    print("用户具有基础设置权限")
end
```

---

## 权限设计原则

1. **最小权限原则**: 每个操作只要求完成该操作所需的最小权限
2. **权限分离**: 不同类型的操作使用不同的权限级别
3. **显式验证**: 所有需要权限的操作都必须显式调用 `validate` 函数
4. **安全默认**: 默认拒绝访问，只有通过验证后才允许操作

---

## 相关文档

- [MDB Privilege C++ API](../../api/mdb/mdb_privilege.md)
- [MDB Service Lua API](mdb_service.md)
- [Privilege 使用指南](../../mdb_privilege_usage.md)
