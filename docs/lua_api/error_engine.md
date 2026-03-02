# 错误引擎 Lua API 文档

## 概述

错误引擎 Lua API 提供了在 Lua 脚本中创建、管理和传递错误对象的功能。通过 `mc_error` 模块，Lua 开发者可以创建错误对象、设置错误参数、格式化错误消息、序列化错误以及进行错误转换。

## 模块加载

```lua
local mc_error = require("mc_error")
```

---

## 错误创建函数

### mc_error.new

创建新的错误对象。

**语法**:

```lua
mc_error.new(name, format, params)
```

**参数**:

- `name` (string) - 错误名称
- `format` (string) - 格式字符串
- `params` (table, 可选) - 错误参数表

**返回值**:

- `err` (userdata) - 错误对象

**示例**:

```lua
local mc_error = require("mc_error")

-- 创建简单错误（不带参数）
local err1 = mc_error.new("file_not_found", "文件不存在")

-- 创建带参数的错误
local err2 = mc_error.new("user_error", "用户 ${name} 的年龄是 ${age}", {
    name = "张三",
    age = 30
})

print(err2.message)  -- "用户 张三 的年龄是 30"
```

### mc_error.new_error

创建新的错误对象（兼容接口，支持 printf 风格格式）。

**语法**:

```lua
mc_error.new_error(name, format, ...)
```

**参数**:

- `name` (string) - 错误名称
- `format` (string) - 格式字符串（支持 printf 风格占位符如 %s, %d）
- `...` - 可变参数，用于替换格式字符串中的占位符

**返回值**:

- `err` (userdata) - 错误对象

**示例**:

```lua
local mc_error = require("mc_error")

-- 使用 printf 风格格式
local err = mc_error.new_error("timeout", "操作在 %dms 后超时", 5000)
print(err.message)  -- "操作在 5000ms 后超时"

-- 使用多个参数
local err2 = mc_error.new_error("user_info", "用户 %s 的年龄是 %d", "张三", 30)
print(err2.message)  -- "用户 张三 的年龄是 30"
```

### mc_error.new_message_error

从 table 创建错误对象。

**语法**:

```lua
mc_error.new_message_error(table)
```

**参数**:

- `table` (table) - 包含错误信息的表，支持以下字段：
  - `name` (string) - 错误名称
  - `message` 或 `format` (string) - 错误格式（message 优先）
  - `params` 或 `args` (table) - 错误参数（params 优先）
  - 其他字段 - 保存到 uservalue

**返回值**:

- `err` (userdata) - 错误对象

**示例**:

```lua
local mc_error = require("mc_error")

local err = mc_error.new_message_error({
    name = "config_error",
    format = "配置文件 ${file} 加载失败",
    params = {
        file = "/etc/myapp/config"
    },
    custom_field = "自定义数据"  -- 保存到 uservalue
})

print(err.message)  -- "配置文件 /etc/myapp/config 加载失败"
print(err.custom_field)  -- "自定义数据"
```

---

## 错误对象方法

### err:tostring

将错误对象转换为字符串。

**语法**:

```lua
err:tostring()
```

**返回值**:

- `str` (string) - 错误字符串

**示例**:

```lua
local mc_error = require("mc_error")

local err = mc_error.new("error", "操作失败")
print(err:tostring())  -- "error: 操作失败"
```

### err:args

获取错误参数表。

**语法**:

```lua
err:args()
```

**返回值**:

- `params` (table) - 参数表

**示例**:

```lua
local mc_error = require("mc_error")

local err = mc_error.new("error", "${greeting}, ${name}", {
    greeting = "你好",
    name = "世界"
})

local params = err:args()
for k, v in pairs(params) do
    print(k, v)
end
```

### err:traceback

获取调用栈信息。

**语法**:

```lua
err:traceback()
```

**返回值**:

- `stack` (string) - 调用栈字符串

**示例**:

```lua
local mc_error = require("mc_error")

local err = mc_error.new("error", "发生错误")
err:traceback()

local stack = err:traceback()
print(stack)
```

### err:post_process

参数后处理，将参数名映射为位置索引。

**语法**:

```lua
err:post_process(param_struct)
```

**参数**:

- `param_struct` (table) - 参数结构定义，数组格式，每个元素是一个包含 `name` 字段的表

**示例**:

```lua
local mc_error = require("mc_error")

local err = mc_error.new("error", "%username: %age", {
    username = "张三",
    age = 30
})

local param_struct = {
    {name = "username"},
    {name = "age"}
}

err:post_process(param_struct)
-- 参数将被转换为位置索引
```

### err:raise

抛出 Lua 错误。

**语法**:

```lua
err:raise()
```

**异常**: 总是抛出 Lua 错误

**示例**:

```lua
local mc_error = require("mc_error")

local err = mc_error.new("error", "操作失败")

if invalid_input then
    err:raise()  -- 抛出错误
end

-- 下面的代码不会执行
print("这行不会输出")
```

### err:encode

将错误对象序列化为 JSON 字符串。

**语法**:

```lua
err:encode()
```

**返回值**:

- `json` (string) - JSON 字符串

**示例**:

```lua
local mc_error = require("mc_error")

local err = mc_error.new("error", "发生错误", {
    code = 100,
    detail = "详细信息"
})

local json = err:encode()
print(json)
```

---

## 错误对象属性

### err.name

错误名称。

**类型**: string

**示例**:

```lua
local mc_error = require("mc_error")

local err = mc_error.new("file_error", "文件错误")
print(err.name)  -- "file_error"

-- 修改名称
err.name = "new_error"
print(err.name)  -- "new_error"
```

### err.message

格式化后的错误消息（只读）。

**类型**: string

**示例**:

```lua
local mc_error = require("mc_error")

local err = mc_error.new("error", "用户 ${name} 的年龄是 ${age}", {
    name = "张三",
    age = 30
})

print(err.message)  -- "用户 张三 的年龄是 30"

-- message 是只读的，不能直接修改
err.message = "新消息"  -- 无效
```

### err.params

错误参数表（通过创建时设置，或在 uservalue 中存储额外数据）。

**类型**: table

**注意**:

- 通过 `mc_error.new()` 的第三个参数设置的参数会存储在内部的 params 中
- 通过 `err.xxx = yyy` 设置的属性会存储在 uservalue 中

**示例**:

```lua
local mc_error = require("mc_error")

-- 方式1：创建时设置参数
local err = mc_error.new("error", "${greeting}, ${name}", {
    greeting = "你好",
    name = "世界"
})

-- 访问参数
for k, v in pairs(err:args()) do
    print(k, v)
end

-- 方式2：通过 uservalue 设置额外属性
err.custom_field = "自定义数据"
print(err.custom_field)  -- "自定义数据"
```

---

## 错误转换函数

### mc_error.load_registries

从文件加载错误定义。

**语法**:

```lua
mc_error.load_registries(base_path, custom_path)
```

**参数**:

- `base_path` (string) - 基础错误定义文件路径（JSON）
- `custom_path` (string) - 自定义错误定义文件路径（JSON，可选）

**返回值**: 无

**示例**:

```lua
local mc_error = require("mc_error")

-- 只加载基础定义
mc_error.load_registries("/etc/errors/base.json")

-- 加载基础定义和自定义定义
mc_error.load_registries(
    "/etc/errors/base.json",
    "/etc/errors/custom.json"
)
```

### mc_error.load_registries_from_string

从字符串加载错误定义。

**语法**:

```lua
mc_error.load_registries_from_string(base_json, custom_json)
```

**参数**:

- `base_json` (string) - 基础错误定义 JSON 字符串
- `custom_json` (string) - 自定义错误定义 JSON 字符串（可选）

**返回值**: 无

**示例**:

```lua
local mc_error = require("mc_error")

local base_json = [[{
    "errors": {
        "file_not_found": {
            "format": "文件 ${path} 不存在"
        }
    }
}]]

mc_error.load_registries_from_string(base_json)
```

### mc_error.convert

转换错误为标准消息格式。

**语法**:

```lua
mc_error.convert(err)
```

**参数**:

- `err` (userdata) - 错误对象

**返回值**:

- `result` (table) - 转换后的消息表，包含以下字段：
  - `message_id` (string) - 消息ID
  - `message_name` (string) - 消息名称
  - `message` (string) - 转换后的消息
  - `severity` (string) - 严重级别
  - `registry_prefix` (string) - 注册表前缀
  - `registry_version` (string) - 注册表版本

**示例**:

```lua
local mc_error = require("mc_error")

-- 先加载错误定义
local base_json = [[{
    "errors": {
        "file_not_found": {
            "message_id": "FILE001",
            "message_name": "File Not Found",
            "message": "File ${path} not found",
            "severity": "error"
        }
    }
}]]

mc_error.load_registries_from_string(base_json)

-- 创建错误并转换
local err = mc_error.new("file_not_found", "文件 ${path} 不存在", {
    path = "/etc/config"
})

local result = mc_error.convert(err)
print(result.message)     -- "File /etc/config not found"
print(result.message_id)  -- "FILE001"
print(result.severity)    -- "error"
```

### mc_error.convert_to_dict

转换错误为 dict 格式。

**语法**:

```lua
mc_error.convert_to_dict(err)
```

**参数**:

- `err` (userdata) - 错误对象

**返回值**:

- `dict` (table) - dict 表

**示例**:

```lua
local mc_error = require("mc_error")

local err = mc_error.new("error", "发生错误", {
    code = 100
})

local dict = mc_error.convert_to_dict(err)
-- dict 包含转换后的错误信息
```

---

## 工具函数

### mc_error.raise_error

直接抛出错误。

**语法**:

```lua
mc_error.raise_error(name, message, ...)
```

**参数**:

- `name` (string) - 错误名称
- `message` (string) - 错误消息（支持 printf 风格格式）
- `...` - 可变参数，用于替换格式字符串中的占位符

**异常**: 总是抛出 Lua 错误

**示例**:

```lua
local mc_error = require("mc_error")

-- 简单用法
mc_error.raise_error("error", "操作失败")

-- 带参数
mc_error.raise_error("validation_error", "字段 %s 验证失败", "username")

-- 使用多个参数
mc_error.raise_error("timeout", "操作在 %dms 后超时，重试 %d 次", 5000, 3)
```

### mc_error.print_log

增强的日志打印，自动包含调用栈信息。

**语法**:

```lua
mc_error.print_log(severity, format, ...)
```

**参数**:

- `severity` (string) - 日志级别（"debug", "info", "warning", "error", "fatal"）
- `format` (string) - 格式字符串（支持 printf 风格）
- `...` - 可变参数

**示例**:

```lua
local mc_error = require("mc_error")

mc_error.print_log("info", "用户 %s 登录成功", "admin")
mc_error.print_log("error", "操作失败: %s", "无效参数")
mc_error.print_log("warning", "配置文件 %s 不存在", "/etc/config")
```

### mc_error.print_trace

打印错误调用栈跟踪。

**语法**:

```lua
mc_error.print_trace(backtrace_level, err_data)
```

**参数**:

- `backtrace_level` (number) - 回溯级别（可选）
- `err_data` (userdata/table) - 错误对象或错误数据

**示例**:

```lua
local mc_error = require("mc_error")

local err = mc_error.new("error", "发生错误", {
    code = 100
})

mc_error.print_trace(1, err)
```

---

## 完整示例

### 基本错误处理

```lua
local mc_error = require("mc_error")

-- 创建错误对象
local err = mc_error.new("file_not_found", "文件 ${path} 不存在", {
    path = "/etc/config.conf"
})

-- 访问错误信息
print("错误名称: " .. err.name)
print("错误消息: " .. err.message)

-- 序列化错误
local json = err:encode()
print("JSON: " .. json)
```

### 错误链和异常

```lua
local mc_error = require("mc_error")

-- 检查并抛出错误
local function load_config(path)
    if path == nil then
        local err = mc_error.new("invalid_arg", "路径不能为空")
        err:raise()
    end

    -- 模拟文件操作失败
    local file = io.open(path, "r")
    if not file then
        local err = mc_error.new("file_error", "无法打开文件 ${path}", {
            path = path
        })
        err:raise()
    end

    return file
end

-- 捕获和处理错误
local ok, err = pcall(load_config, nil)
if not ok then
    print("捕获错误: " .. err.name)
    print("错误消息: " .. err.message)
end
```

### 使用不同的参数设置方式

```lua
local mc_error = require("mc_error")

-- 方式1：创建时设置参数
local err1 = mc_error.new("user_error", "用户 ${name} 的年龄是 ${age}", {
    name = "张三",
    age = 30
})

print(err1.message)  -- "用户 张三 的年龄是 30"

-- 方式2：使用 new_error（printf 风格）
local err2 = mc_error.new_error("timeout", "操作在 %dms 后超时", 5000)
print(err2.message)  -- "操作在 5000ms 后超时"

-- 方式3：从 table 创建
local err3 = mc_error.new_message_error({
    name = "config_error",
    format = "配置文件 ${file} 加载失败",
    params = {
        file = "/etc/myapp/config"
    }
})
print(err3.message)  -- "配置文件 /etc/myapp/config 加载失败"
```

### 错误转换

```lua
local mc_error = require("mc_error")

-- 加载错误定义
local base_json = [[{
    "errors": {
        "file_not_found": {
            "message_id": "FILE001",
            "message_name": "File Not Found",
            "message": "File ${path} not found",
            "severity": "error"
        }
    }
}]]

mc_error.load_registries_from_string(base_json)

-- 创建错误并转换
local err = mc_error.new("file_not_found", "文件 ${path} 不存在", {
    path = "/etc/config"
})

local result = mc_error.convert(err)
print(result.message)     -- "File /etc/config not found"
print(result.message_id)  -- "FILE001"
print(result.severity)    -- "error"
```

### 错误日志

```lua
local mc_error = require("mc_error")

-- 使用增强日志
mc_error.print_log("info", "应用程序启动")
mc_error.print_log("debug", "加载配置文件: %s", "/etc/config")

-- 错误日志
local function process_request(url)
    if not url then
        mc_error.print_log("error", "URL 不能为空")
        return nil
    end

    mc_error.print_log("info", "处理请求: %s", url)
    return true
end

process_request("http://example.com")
```
