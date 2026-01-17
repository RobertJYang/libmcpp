# DBus Error Lua API

## 概述

错误对象用于表示 DBus 操作中的错误信息。

## 错误对象

错误对象是 userdata 类型，通常作为函数调用失败时的第二个返回值。

### 创建方式

错误对象不能直接创建，由 DBus 操作失败时自动返回。

```lua
local success, error = conn:request_name("com.example.Test")
if not success and error then
    -- error 是错误对象
end
```

## 方法

### is_set

检查错误是否已设置。

#### 语法

```lua
has_error = error:is_set()
```

#### 返回值

- `has_error` (boolean) - 已设置返回 true，否则返回 false

#### 示例

```lua
local ok, err = conn:request_name("com.example.Test")
if not ok and err then
    if err:is_set() then
        print("确实有错误")
    end
end
```

### ensure_ok

如果错误已设置，则抛出 Lua 错误。

#### 语法

```lua
error:ensure_ok()
```

#### 返回值

无返回值。如果有错误，会抛出 Lua 错误并终止执行。

#### 示例

```lua
local ok, err = conn:request_name("com.example.Test")
if not ok and err then
    -- 如果有错误，这会抛出异常并终止脚本
    err:ensure_ok()
end

-- 如果没有异常，继续执行
print("请求成功")
```

## 属性

### name

错误名称。

#### 类型

- (string | nil) - 错误名称

#### 示例

```lua
local ok, err = conn:request_name("com.example.Test")
if not ok and err then
    print("错误名称: " .. (err.name or "未知"))
end
```

#### 常见错误名称

- `"org.freedesktop.DBus.Error.AccessDenied"` - 访问被拒绝
- `"org.freedesktop.DBus.Error.NoReply"` - 无回复
- `"org.freedesktop.DBus.Error.Timeout"` - 超时
- `"org.freedesktop.DBus.Error.Failed"` - 一般失败
- `"org.freedesktop.DBus.Error.ServiceUnknown"` - 服务未知
- `"org.freedesktop.DBus.Error.NameHasNoOwner"` - 名称无所有者

### message

错误消息内容。

#### 类型

- (string | nil) - 错误消息

#### 示例

```lua
local ok, err = conn:request_name("com.example.Test")
if not ok and err then
    print("错误消息: " .. (err.message or "无消息"))
end
```

## 字符串表示

错误对象支持 `tostring()` 转换：

```lua
local ok, err = conn:request_name("com.example.Test")
if not ok and err then
    print(tostring(err))  -- 输出: "错误名称 : 错误消息"
end
```

格式为：`name : message`

## 使用示例

### 基本错误检查

```lua
local ldbus = require('ldbus')

local dbus = ldbus.blocking.open_user()

local ok, err = dbus:request_name("com.example.Test")

if not ok then
    if err then
        print("请求失败:")
        print("  名称: " .. (err.name or "unknown"))
        print("  消息: " .. (err.message or "no message"))
    else
        print("请求失败: 未知错误")
    end
    dbus:close()
    return
end

print("请求成功")
dbus:close()
```

### 使用 ensure_ok

```lua
local ldbus = require('ldbus')

local dbus = ldbus.blocking.open_user()

-- 如果失败，直接抛出异常
local ok, err = dbus:request_name("com.example.Test")
if not ok and err then
    err:ensure_ok()  -- 这会抛出异常，脚本终止
end

-- 只有成功才会执行到这里
print("请求成功")
dbus:close()
```

### 错误分类处理

```lua
local ldbus = require('ldbus')

local function handle_error(err)
    if not err or not err:is_set() then
        return
    end
    
    local name = err.name
    
    if name == "org.freedesktop.DBus.Error.AccessDenied" then
        print("访问被拒绝，请检查权限")
    elseif name == "org.freedesktop.DBus.Error.Timeout" then
        print("操作超时，请稍后重试")
    elseif name == "org.freedesktop.DBus.Error.ServiceUnknown" then
        print("服务不存在，请检查服务名称")
    else
        print("未知错误: " .. tostring(err))
    end
end

local dbus = ldbus.blocking.open_user()

local ok, err = dbus:request_name("com.example.Test")
if not ok then
    handle_error(err)
end

dbus:close()
```

### 错误日志记录

```lua
local ldbus = require('ldbus')

-- 错误日志函数
local function log_error(operation, err)
    if not err then
        print(string.format("[ERROR] %s 失败: 未知错误", operation))
        return
    end
    
    if not err:is_set() then
        print(string.format("[ERROR] %s 失败: 错误未设置", operation))
        return
    end
    
    print(string.format(
        "[ERROR] %s 失败: %s - %s",
        operation,
        err.name or "unknown",
        err.message or "no message"
    ))
end

local dbus = ldbus.blocking.open_user()

-- 操作 1
local ok1, err1 = dbus:request_name("com.example.Test1")
if not ok1 then
    log_error("请求名称 Test1", err1)
end

-- 操作 2
local ok2, err2 = dbus:request_name("com.example.Test2")
if not ok2 then
    log_error("请求名称 Test2", err2)
end

dbus:close()
```

### 错误重试逻辑

```lua
local ldbus = require('ldbus')

-- 带重试的操作
local function request_name_with_retry(conn, name, max_retries)
    local retries = 0
    
    while retries < max_retries do
        local ok, err = conn:request_name(name)
        
        if ok then
            print("成功（第 " .. (retries + 1) .. " 次尝试）")
            return true
        end
        
        -- 检查是否是可重试的错误
        if err and err.name == "org.freedesktop.DBus.Error.Timeout" then
            print("超时，重试...")
            retries = retries + 1
            os.execute("sleep 1")
        else
            -- 不可重试的错误
            if err then
                print("失败: " .. err.name)
            end
            return false
        end
    end
    
    print("重试次数已用完")
    return false
end

local dbus = ldbus.blocking.open_user()

-- 最多重试 3 次
local success = request_name_with_retry(dbus, "com.example.Test", 3)

dbus:close()
```

### 安全的错误访问

```lua
local ldbus = require('ldbus')

-- 安全地访问错误属性
local function safe_error_string(err)
    if not err then
        return "无错误对象"
    end
    
    -- 使用 pcall 避免访问 nil 时出错
    local ok, name = pcall(function() return err.name end)
    local name_str = ok and name or "unknown"
    
    local ok2, msg = pcall(function() return err.message end)
    local msg_str = ok2 and msg or "no message"
    
    return name_str .. " : " .. msg_str
end

local dbus = ldbus.blocking.open_user()

local ok, err = dbus:request_name("com.example.Test")
if not ok then
    print("错误: " .. safe_error_string(err))
end

dbus:close()
```

## 注意事项

1. **错误对象生命周期**：错误对象由 Lua GC 管理
2. **nil 检查**：错误对象可能为 nil，使用前需检查
3. **属性访问**：name 和 message 可能为 nil
4. **ensure_ok 慎用**：会终止脚本执行，除非你确实希望如此
5. **错误名称**：遵循 DBus 错误名称规范

## 错误码参考

### 标准 DBus 错误

| 错误名称 | 说明 |
|---------|------|
| `org.freedesktop.DBus.Error.Failed` | 一般失败 |
| `org.freedesktop.DBus.Error.NoMemory` | 内存不足 |
| `org.freedesktop.DBus.Error.ServiceUnknown` | 服务未知 |
| `org.freedesktop.DBus.Error.NameHasNoOwner` | 名称无所有者 |
| `org.freedesktop.DBus.Error.NoReply` | 无回复 |
| `org.freedesktop.DBus.Error.IOError` | I/O 错误 |
| `org.freedesktop.DBus.Error.BadAddress` | 地址错误 |
| `org.freedesktop.DBus.Error.NotSupported` | 不支持 |
| `org.freedesktop.DBus.Error.LimitsExceeded` | 超出限制 |
| `org.freedesktop.DBus.Error.AccessDenied` | 访问被拒绝 |
| `org.freedesktop.DBus.Error.Timeout` | 超时 |
| `org.freedesktop.DBus.Error.InvalidArgs` | 参数无效 |
| `org.freedesktop.DBus.Error.UnknownMethod` | 方法未知 |
| `org.freedesktop.DBus.Error.UnknownObject` | 对象未知 |
| `org.freedesktop.DBus.Error.UnknownInterface` | 接口未知 |

## 相关文档

- [blocking.md](blocking.md) - 阻塞模式
- [nonblock.md](nonblock.md) - 非阻塞模式
- [connection.md](connection.md) - 连接对象
- [../dbus.md](../dbus.md) - DBus Lua API 主文档
