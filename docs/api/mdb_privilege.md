# MDB Privilege C++ API

## 概述

MDB Privilege 模块提供了权限验证功能，用于在处理请求时验证用户是否具有所需的权限。该模块与认证系统集成，从执行上下文中获取认证和权限信息，并根据期望的权限级别进行验证。

## 头文件

```cpp
#include <mc/mdb/privilege.h>
```

## 命名空间

```cpp
namespace mc::mdb
```

## 枚举类型

### auth_state

认证状态枚举。

```cpp
enum class auth_state {
    no_auth       = 0,  // 无需认证
    auth_required = 1   // 需要认证
};
```

**枚举值说明**

- `no_auth`: 表示当前操作无需认证或已完成认证
- `auth_required`: 表示当前操作需要进行权限验证

---

### privilege

权限级别枚举。

```cpp
enum privilege : uint32_t {
    read_only      = 1,   // 只读
    diagnose_mgmt  = 2,   // 诊断管理
    security_mgmt  = 4,   // 安全管理
    basic_setting  = 8,   // 基础设置
    user_mgmt      = 16,  // 用户管理
    power_mgmt     = 32,  // 电源管理
    vmm_mgmt       = 64,  // VMM管理
    kvm_mgmt       = 128, // KVM管理
    configure_self = 256  // 自我配置
};
```

**枚举值说明**

| 权限 | 值 | 说明 |
|------|-----|------|
| `read_only` | 1 | 只读权限，可以查看信息但不能修改 |
| `diagnose_mgmt` | 2 | 诊断管理权限，可以进行系统诊断操作 |
| `security_mgmt` | 4 | 安全管理权限，可以配置安全相关设置 |
| `basic_setting` | 8 | 基础设置权限，可以修改基本配置 |
| `user_mgmt` | 16 | 用户管理权限，可以管理用户账户 |
| `power_mgmt` | 32 | 电源管理权限，可以控制电源状态 |
| `vmm_mgmt` | 64 | VMM管理权限 |
| `kvm_mgmt` | 128 | KVM管理权限 |
| `configure_self` | 256 | 自我配置权限，用于首次登录后的密码修改 |

**权限组合**

权限值可以通过按位或运算组合，例如：

```cpp
uint32_t combined = privilege::read_only | privilege::basic_setting;  // 值为 9
```

---

## 接口函数

### get_privilege_str

将权限数组转换为权限值的字符串表示。

**函数签名**

```cpp
std::string get_privilege_str(const std::vector<uint32_t>& privileges);
```

**参数**

- `privileges`: 权限值数组

**返回值**

返回所有权限值按位或运算后的字符串表示。

**实现逻辑**

1. 对数组中的所有权限值进行按位或运算
2. 将结果转换为字符串返回

**示例**

```cpp
std::vector<uint32_t> privs = {
    mc::mdb::privilege::read_only,
    mc::mdb::privilege::basic_setting,
    mc::mdb::privilege::user_mgmt
};

std::string priv_str = mc::mdb::get_privilege_str(privs);
// priv_str = "25" (1 | 8 | 16 = 25)
```

---

### validate

验证当前用户是否具有指定的权限。

**函数签名**

```cpp
void validate(uint32_t expected_privilege);
```

**参数**

- `expected_privilege`: 期望的权限值（可以是单个权限或多个权限的按位或）

**异常**

- `mc::insufficient_privilege_exception`: 用户权限不足
- `mc::password_changed_required_exception`: 需要先修改密码

**验证流程**

1. **获取上下文**: 从 `mc::engine::context` 获取当前执行上下文
2. **检查 Auth 字段**: 
   - 如果 Auth 字段不存在或不是字符串，直接返回（无需验证）
   - 如果 Auth 字段不等于 `auth_required`，直接返回
3. **检查 Privilege 字段**:
   - 如果 Privilege 字段不存在或不是字符串，抛出 `insufficient_privilege_exception`
   - 将 Privilege 字段转换为整数
4. **特殊处理**: 
   - 如果期望权限为 0 且用户权限为 `configure_self`，抛出 `password_changed_required_exception`
   - 如果期望权限为 0，直接返回
5. **权限验证**:
   - 计算用户权限与期望权限的交集：`intersection = user_priv & expected_priv`
   - 如果交集不等于期望权限，抛出 `insufficient_privilege_exception`
6. **更新上下文**: 将 Auth 字段设置为 `no_auth`，表示已完成验证

**示例**

```cpp
try {
    // 验证是否具有用户管理权限
    mc::mdb::validate(mc::mdb::privilege::user_mgmt);
    
    // 执行用户管理操作
    // ...
    
} catch (const mc::insufficient_privilege_exception& e) {
    // 权限不足
    elog("权限不足: ${error}", ("error", e.what()));
    
} catch (const mc::password_changed_required_exception& e) {
    // 需要修改密码
    wlog("需要修改密码: ${error}", ("error", e.what()));
}
```

**组合权限验证**

```cpp
// 验证是否同时具有基础设置和电源管理权限
uint32_t required = mc::mdb::privilege::basic_setting | 
                    mc::mdb::privilege::power_mgmt;
mc::mdb::validate(required);
```

---

## 上下文字段

### Auth 字段

- **类型**: 字符串
- **值**: `auth_state` 枚举值的字符串表示
- **用途**: 标识当前请求是否需要权限验证

### Privilege 字段

- **类型**: 字符串
- **值**: 用户权限值的字符串表示
- **用途**: 存储当前用户的权限信息

---

## 使用场景

### 场景 1: RESTful API 权限验证

```cpp
void handle_post_user(mc::engine::context& ctx) {
    // 验证是否具有用户管理权限
    mc::mdb::validate(mc::mdb::privilege::user_mgmt);
    
    // 创建用户
    // ...
}
```

### 场景 2: 多权限验证

```cpp
void handle_system_config(mc::engine::context& ctx) {
    // 需要基础设置或安全管理权限
    try {
        mc::mdb::validate(mc::mdb::privilege::basic_setting);
    } catch (const mc::insufficient_privilege_exception&) {
        // 如果没有基础设置权限，尝试安全管理权限
        mc::mdb::validate(mc::mdb::privilege::security_mgmt);
    }
    
    // 执行配置操作
    // ...
}
```

### 场景 3: 权限组合

```cpp
void handle_power_operation(mc::engine::context& ctx) {
    // 同时需要电源管理和基础设置权限
    uint32_t required = mc::mdb::privilege::power_mgmt | 
                        mc::mdb::privilege::basic_setting;
    mc::mdb::validate(required);
    
    // 执行电源操作
    // ...
}
```

### 场景 4: 首次登录密码修改

```cpp
void handle_first_login(mc::engine::context& ctx) {
    try {
        // 验证权限为 0，检查是否需要修改密码
        mc::mdb::validate(0);
        
    } catch (const mc::password_changed_required_exception& e) {
        // 提示用户修改密码
        return create_password_change_required_response();
    }
}
```

---

## 异常说明

### insufficient_privilege_exception

**异常消息**

```
There are insufficient privileges for the account or credentials associated 
with the current session to perform the requested operation.
```

**抛出条件**

- Privilege 字段无效或无法转换为整数
- 用户权限与期望权限的交集不等于期望权限

### password_changed_required_exception

**异常消息**

```
Indicates that the password for the account provided must be changed before 
accessing the service. The password can be changed with a PATCH to the 
Password property in the manager account resource instance. Implementations 
that provide a default password for an account may require a password change 
prior to first access to the service.
```

**抛出条件**

- 期望权限为 0
- 用户权限为 `configure_self`（值为 128）

---

## 权限设计原则

1. **最小权限原则**: 每个操作只要求完成该操作所需的最小权限
2. **权限分离**: 不同类型的操作使用不同的权限级别
3. **权限组合**: 支持通过按位或组合多个权限
4. **显式验证**: 所有需要权限的操作都必须显式调用 `validate` 函数
5. **安全默认**: 默认拒绝访问，只有通过验证后才允许操作

---

## 集成指南

### 1. 设置上下文

在处理请求时，需要在上下文中设置 Auth 和 Privilege 字段：

```cpp
void handle_request(mc::engine::context& ctx, const session_info& session) {
    // 设置认证状态
    ctx.set_arg("Auth", std::to_string(
        static_cast<int>(mc::mdb::auth_state::auth_required)));
    
    // 设置用户权限
    ctx.set_arg("Privilege", std::to_string(session.privilege));
    
    // 处理请求
    // ...
}
```

### 2. 验证权限

在需要权限的操作前调用 `validate`：

```cpp
void delete_user(mc::engine::context& ctx, const std::string& username) {
    // 验证用户管理权限
    mc::mdb::validate(mc::mdb::privilege::user_mgmt);
    
    // 删除用户
    // ...
}
```

### 3. 错误处理

捕获并处理权限异常：

```cpp
try {
    mc::mdb::validate(expected_priv);
    perform_operation();
    
} catch (const mc::insufficient_privilege_exception& e) {
    return create_error_response(403, "Forbidden", e.what());
    
} catch (const mc::password_changed_required_exception& e) {
    return create_error_response(403, "Password Change Required", e.what());
}
```

---

## 相关文档

- [MDB Privilege 使用指南](../mdb_privilege_usage.md)
- [MDB Privilege Lua API](../lua_api/mdb.md#privilege)
- [异常设计](../8.exception_design.md)
