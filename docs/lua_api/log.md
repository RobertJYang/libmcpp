# 日志 Lua API 使用说明

## 概述

`mc_log` 模块提供了在 Lua 中记录结构化日志的能力，与 C++ 的 `mc::log` 对应。加载模块后得到默认的 logger 对象，支持多级日志、键值对格式化（`${key}`）、可选模块名、限流/不限流、南向硬件流、mc 流以及串口 printf 等输出方式。

## 加载模块

```lua
-- 加载后得到默认 logger 对象
local log = require("mc_log")
```

## 主要特性

- **多级日志**：trace、debug、info、notice、warn、error、fatal
- **结构化格式**：消息模板使用 `${key}`，参数为键值对（key, value, key, value, ...）
- **可选模块名**：支持 `log:info(module_name, message, ...)` 形式，便于按模块分类
- **限流与不限流**：默认按级别限流；`*_easy` 方法不限流
- **南向硬件流**：`hw_stream_*` 输出到南向相关日志文件
- **mc 流**：`mc_stream_*` 输出到 LOG_LOCAL5 对应日志
- **串口 printf**：`*_printf` 使用 Lua `string.format` 格式化后输出
- **mdbctl 终端**：`mdbctl_log` 将日志输出到 mdbctl 终端（需先通过 mdbctl attach）

## 日志级别常量

模块（logger 元表）上提供以下级别常量，可用于 `set_level`、`log(level, ...)`、`is_enabled` 等：

| 常量   | 描述         |
|--------|--------------|
| `log.ALL`    | 所有级别     |
| `log.TRACE`  | 跟踪         |
| `log.DEBUG`  | 调试         |
| `log.INFO`   | 信息         |
| `log.NOTICE` | 通知         |
| `log.WARN`   | 警告         |
| `log.ERROR`  | 错误         |
| `log.FATAL`  | 致命         |
| `log.OFF`    | 关闭         |

**示例：**

```lua
local log = require("mc_log")
log:set_level(log.DEBUG)
if log:is_enabled(log.INFO) then
    log:info("当前级别已启用 info")
end
```

## logger 对象方法

### 获取与配置

#### `logger:get_name()`

获取当前 logger 名称。

**返回值：** 字符串

**示例：**

```lua
local name = log:get_name()
```

#### `logger:set_name(name)`

设置 logger 名称。

**参数：**

- `name`: 字符串

**示例：**

```lua
log:set_name("mymodule")
```

#### `logger:get_level()`

获取当前日志级别。

**返回值：** 整数（对应级别常量）

**示例：**

```lua
local lvl = log:get_level()
```

#### `logger:set_level(level)`

设置日志级别。支持链式调用。

**参数：**

- `level`: 整数，可使用 `log.TRACE`、`log.INFO` 等常量

**返回值：** logger 自身

**示例：**

```lua
log:set_level(log.INFO)
log:set_level(log.DEBUG):info("链式调用示例")
```

#### `logger:is_enabled(level)`

判断指定级别是否会被输出。

**参数：**

- `level`: 整数，级别常量

**返回值：** boolean

**示例：**

```lua
if log:is_enabled(log.DEBUG) then
    log:debug("仅当 debug 启用时输出")
end
```

#### `logger:system([system_id])`

设置或清除系统 ID（用于输出中标识）。不传参数时重置为未设置。返回新 logger 用于链式调用，不修改原 logger。

**参数：**

- `system_id`: 可选，整数；不传则重置

**返回值：** 新的 logger（带 system_id 的副本）

**示例：**

```lua
local log_with_sys = log:system(1)
log_with_sys:info("本条带 system_id")
```

链式调用示例：

```lua
log:system(1):info("本条带 system_id")
```

#### `logger:period(period_s)`

设置日志打印间隔（秒），0 表示不限制。返回新 logger 用于链式调用。

**参数：**

- `period_s`: 非负整数，秒

**返回值：** 新的 logger（带周期配置的副本）

**示例：**

```lua
local limited = log:period(5)
limited:info("此 logger 同一条消息 5 秒内只打一次")
```

链式调用示例：

```lua
log:period(5):info("此 logger 同一条消息 5 秒内只打一次")
```

#### `logger:condition(cond)`

设置 condition，控制是否输出日志。cond 为 false 时不打印，为 true 时打印。返回新 logger 用于链式调用，仿照 system/period。

**参数：**

- `cond`: boolean，true 时输出日志，false 时不输出

**返回值：** 新的 logger（带 condition 配置的副本）

**示例：**

```lua
local muted = log:condition(false)
muted:info("此条不会输出")
```

链式调用示例：

```lua
log:condition(false):info("此条不会输出")
log:condition(true):info("此条会输出")
```

### 通用日志接口

#### `logger:log(level, message, key1, value1, key2, value2, ...)`

按指定级别记录一条日志，消息中可使用 `${key}` 占位符，后面为键值对。

**参数：**

- `level`: 整数，级别常量
- `message`: 字符串，格式模板（如 `"用户 ${user} 登录"`）
- 后续为成对的 key（字符串）、value（number/string/boolean）

**示例：**

```lua
log:log(log.INFO, "请求 ${method} ${path}", "method", "GET", "path", "/api")
```

### 按级别记录（限流）

以下方法默认带限流：`trace`、`debug`、`info`、`notice`、`warn`、`error`、`fatal`。

**调用形式：**

- `logger:level(message, key1, value1, ...)`  
- 或 `logger:level(module_name, message, key1, value1, ...)`（当第一个和第二个参数均为字符串时，第一个作为 module_name）

**示例：**

```lua
log:info("服务启动")
log:info("数据库连接 ${host}:${port}", "host", "localhost", "port", 3306)
log:info("db", "连接已建立")  -- module_name = "db"
```

| 方法            | 级别   |
|-----------------|--------|
| `logger:trace(...)`  | trace  |
| `logger:debug(...)`  | debug  |
| `logger:info(...)`   | info   |
| `logger:notice(...)` | notice |
| `logger:warn(...)`   | warn   |
| `logger:error(...)`  | error  |
| `logger:fatal(...)`  | fatal  |

### 按级别记录（不限流）

`*_easy` 方法行为与上面一致，但不限流，每条都会输出。

| 方法                  | 级别   |
|-----------------------|--------|
| `logger:trace_easy(...)`  | trace  |
| `logger:debug_easy(...)`  | debug  |
| `logger:info_easy(...)`   | info   |
| `logger:notice_easy(...)` | notice |
| `logger:warn_easy(...)`   | warn   |
| `logger:error_easy(...)`  | error  |
| `logger:fatal_easy(...)`  | fatal  |

### 操作日志

#### `logger:operation(initiator, executor, fmt, key1, value1, ...)`

记录操作日志，对应 C++ 的 `operation_log`，类别为 operation。

**参数：**

- `initiator`: Lua 表，必须包含 `Interface`、`UserName`、`ClientIp`；值可为带 `:value()` 方法的对象（如 variant），空字符串用 `'Unknown'` 代替
- `executor`: 字符串，模块名（module_name）
- `fmt`: 字符串，日志内容，支持 `${key}` 占位符
- 可选：后续为成对 key、value，用于格式化

**示例：**

```lua
local log = require("mc_log")

-- 基本用法
local initiator = ctx:get_initiator()  -- 返回 {Interface=..., UserName=..., ClientIp=...}
log:operation(initiator, 'maca', 'reset successfully')

-- 带占位符
log:operation(initiator, 'maca', '用户 ${user} 执行 ${action}', 'user', 'admin', 'action', '登录')
```

### 运行日志

#### `logger:running(level, message)`

记录运行日志，格式为：时间、级别、日志内容。示例：`2026-02-04 15:20:32 WARN : test running log`。

**参数：**

- `level`: 整数，使用 `log.RLOG_DEBUG`、`log.RLOG_INFO`、`log.RLOG_NOTICE`、`log.RLOG_WARN`、`log.RLOG_ERROR`
- `message`: 字符串，日志内容

**示例：**

```lua
local log = require("mc_log")

log:running(log.RLOG_WARN, 'test running log')
log:running(log.RLOG_INFO, 'service started')
```

### 维护日志

#### `logger:maintenance(level, error_code, message)` 或 `logger:maintenance(level, message)`

记录维护日志。有错误码时格式：时间、级别、错误码、日志内容；无错误码时：时间、级别、日志内容。

**参数：**

- `level`: 整数，使用 `log.MLOG_DEBUG`、`log.MLOG_INFO`、`log.MLOG_NOTICE`、`log.MLOG_WARN`、`log.MLOG_ERROR`（不含 trace、fatal）
- `error_code`: 字符串，可选，错误码
- `message`: 字符串，日志内容

**示例：**

```lua
local log = require("mc_log")

-- 带错误码
log:maintenance(log.MLOG_ERROR, 'SVR-0001111', 'test maintenance log')
-- 无错误码，仅输出时间、级别、日志内容
log:maintenance(log.MLOG_WARN, 'disk space low')
```

### 安全日志

#### `logger:security(message)`

记录安全日志，格式为：时间、级别、日志内容。仅需传入日志内容，使用 info 级别。

**参数：**

- `message`: 字符串，日志内容

**示例：**

```lua
local log = require("mc_log")

log:security('test security log')
```

### 南向硬件流与 mc 流

南向硬件流（`hw_stream_*`）和 mc 流（`mc_stream_*`）有固定输出格式（时间、模块名、级别、文件名(行号)、日志文本），并写入对应日志文件；mc 流对应 LOG_LOCAL5。

**调用形式：** 与普通级别方法相同，支持可选 `module_name`。

| 方法                         | 级别   | 流类型     |
|------------------------------|--------|------------|
| `logger:hw_stream_info(...)`  | info   | 南向硬件流 |
| `logger:hw_stream_warn(...)`  | warn   | 南向硬件流 |
| `logger:hw_stream_notice(...)`| notice | 南向硬件流 |
| `logger:hw_stream_error(...)` | error  | 南向硬件流 |
| `logger:mc_stream_info(...)`  | info   | mc 流      |
| `logger:mc_stream_warn(...)`  | warn   | mc 流      |
| `logger:mc_stream_notice(...)`| notice | mc 流      |
| `logger:mc_stream_error(...)` | error  | mc 流      |

**示例：**

```lua
log:hw_stream_info("device", "设备 ${id} 就绪", "id", "sensor_01")
log:mc_stream_warn("配置项 ${key} 缺失", "key", "timeout")
```

### 串口 printf 方式

`*_printf` 使用 Lua `string.format(fmt, ...)` 格式化消息，再按对应级别输出到串口 printf 通道。

**调用形式：**

- `logger:level_printf(fmt, arg1, arg2, ...)`  
- 或 `logger:level_printf(module_name, fmt, arg1, arg2, ...)`（前两个参数均为字符串时，第一个为 module_name）

| 方法                     | 级别   |
|--------------------------|--------|
| `logger:debug_printf(...)`  | debug  |
| `logger:info_printf(...)`   | info   |
| `logger:notice_printf(...)` | notice |
| `logger:warn_printf(...)`   | warn   |
| `logger:error_printf(...)`  | error  |

**示例：**

```lua
log:info_printf("value=%d, name=%s", 42, "test")
log:warn_printf("serial", "port %s busy", "/dev/ttyS0")
```

#### `logger:mdbctl_log(fmt)`

将日志内容输出到 mdbctl 终端（需先通过 mdbctl attach）。内部使用 mdbctl_logger，仅输出到 socket，不含 console/file。

**参数：**

- `fmt`: 字符串，日志内容

**示例：**

```lua
log:mdbctl_log("调试信息")
log:mdbctl_log("当前状态: " .. status)
```

### 抛出异常

#### `logger:raise(fmt, key1, value1, key2, value2, ...)`

按日志的 `${key}` 格式模板格式化消息后抛出 Lua 错误（对应 C++ 异常转 Lua 错误）。

**参数：**

- `fmt`: 字符串，格式模板
- 后续为成对 key、value

**示例：**

```lua
log:raise("连接失败: ${host}:${port}", "host", "127.0.0.1", "port", 8080)
```

## 结构化消息格式

- 消息字符串中使用 `${key}` 占位符，会被后续键值对中的同名 key 替换。
- 键值对顺序：`key1, value1, key2, value2, ...`；value 支持 number、string、boolean。
- 若前两个参数均为字符串，则第一个可作为可选 `module_name`（仅部分接口支持，见上文）。

**示例：**

```lua
log:info("用户 ${user} 在 ${time} 登录", "user", "admin", "time", "2025-01-30 10:00:00")
```

## 使用示例

### 示例 1：基本级别与结构化

```lua
local log = require("mc_log")

log:set_level(log.INFO)
log:info("应用启动")
log:info("监听 ${host}:${port}", "host", "0.0.0.0", "port", 8080)
log:warn("磁盘剩余 ${free}MB", "free", 1024)
```

### 示例 2：带模块名

```lua
local log = require("mc_log")

log:info("http", "请求 ${method} ${path}", "method", "GET", "path", "/api/users")
log:error("db", "连接超时: ${host}", "host", "localhost")
```

### 示例 3：先判断级别再打日志

```lua
local log = require("mc_log")

if log:is_enabled(log.DEBUG) then
    log:debug("调试信息: ${data}", "data", expensive_to_string())
end
```

### 示例 4：南向流与 printf

```lua
local log = require("mc_log")

log:hw_stream_info("sensor", "温度 ${temp}℃", "temp", 25.5)
log:info_printf("result=%d", 100)
```

### 示例 5：抛错

```lua
local log = require("mc_log")

local function connect(host, port)
    if not ok then
        log:raise("连接失败: ${host}:${port}", "host", host, "port", port)
    end
end
```

## 注意事项

1. **模块加载**：`require("mc_log")` 返回的是默认 logger，不是表；级别常量通过 logger 的元表访问（如 `log.INFO`）。
2. **限流**：默认的 `trace`/`debug`/`info` 等会限流，高频日志请用 `*_easy` 或先 `is_enabled` 再打。
3. **system/period/condition**：`system()`、`period()`、`condition()` 返回新 logger 副本，不修改原 logger，适合临时配置。
4. **键值对**：结构化参数必须成对出现，且 key 为字符串，value 为 number/string/boolean。
5. **错误处理**：`raise` 会直接抛 Lua 错误，可用 `pcall` 捕获。

## 相关文档

- [C++ Log API](../api/log.md) - C++ 日志接口
- [日志设计说明](../7.log_design.md) - 日志模块设计
- [Lua API 索引](index.md)
