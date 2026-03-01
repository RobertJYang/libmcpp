# 错误引擎接口设计文档

## 目录

- [一、C++ 接口](#一c-接口)
  - [1.1 error_exception 类](#11-error_exception-类)
  - [1.2 异常宏](#12-异常宏)
  - [1.3 error 类扩展](#13-error-类扩展)
  - [1.4 error_message_converter 类](#14-error_message_converter-类)
- [二、Lua 接口](#二lua-接口)
  - [2.1 基础错误接口](#21-基础错误接口)
  - [2.2 Lua API 兼容性层](#22-lua-api-兼容性层)
  - [2.3 错误消息转换器接口](#23-错误消息转换器接口)
- [三、使用流程示例](#三使用流程示例)
  - [3.1 C++ 侧使用流程](#31-c-侧使用流程)
  - [3.2 Lua 侧使用流程](#32-lua-侧使用流程)
- [四、关键设计要点](#四关键设计要点)

---

## 一、C++ 接口

### 1.1 error_exception 类

**位置**: `include/mc/exception.h`

**描述**: 错误引擎异常类，用于支持结构化参数的错误异常，与错误引擎集成。

```cpp
class MC_API error_exception : public exception {
public:
    enum code_enum {
        code_value = error_engine_exception_code // 使用错误引擎异常代码
    };

    /**
     * @brief 构造函数
     *
     * @param error_name 错误名称（需要在错误定义文件中注册）
     * @param args 结构化参数（使用数字键 0, 1, 2...）
     * @param code 异常代码
     */
    error_exception(const char* error_name,
                  const mc::dict& args,
                  int64_t code = error_engine_exception_code);

    /**
     * @brief 获取结构化参数
     */
    const mc::dict& args() const noexcept { return m_args; }

    /**
     * @brief 动态复制异常
     */
    virtual std::shared_ptr<exception> dynamic_copy_exception() const override;

    /**
     * @brief 动态重新抛出异常
     */
    virtual void dynamic_rethrow_exception() const override;

private:
    mc::dict m_args;  // 结构化参数（数字键 0, 1, 2...）
};
```

**使用示例**:

```cpp
// 抛出带结构化参数的错误异常
MC_THROW_ERROR("PropertyValueOutOfRange", {0, "100"}, {1, "Threshold"});

// 单参数
MC_THROW_ERROR("PropertyDuplicate", {0, "TestProperty"});

// C++ 代码中捕获并转换
try {
    // 业务逻辑
} catch (const mc::error_exception& e) {
    auto err = mc::error::from_exception(e);
    // 使用 err...
}
```

### 1.2 异常宏

**位置**: `include/mc/exception.h`

#### MC_THROW_ERROR

抛出错误引擎异常宏，使用数字键传递结构化参数。

```cpp
/**
 * @brief 抛出错误引擎异常宏
 *
 * @param error_name 错误名称（需要在错误定义文件中注册）
 * @param ... 结构化参数列表，使用初始化列表语法：{0, value1}, {1, value2}
 *
 * @example
 * // 单参数
 * MC_THROW_ERROR("PropertyDuplicate", {0, "TestProperty"});
 *
 * // 多参数
 * MC_THROW_ERROR("PropertyValueOutOfRange", {0, "100"}, {1, "Threshold"});
 */
#define MC_THROW_ERROR(error_name, ...) \
    throw mc::error_exception(error_name, \
                             mc::dict{__VA_ARGS__}, \
                             error_engine_exception_code)
```

**使用示例**:

```cpp
// 在验证器中使用
void check_integer(const mc::variant& value, const std::string& name) {
    std::string final_val = mc::to_string(value);
    if (!is_valid_integer(final_val)) {
        MC_THROW_ERROR("PropertyValueTypeError", {0, final_val}, {1, final_name});
    }
}

void check_range(const mc::variant& value, int64_t min, int64_t max, const std::string& name) {
    std::string final_val = mc::to_string(value);
    int64_t int_val = std::stoll(final_val);
    if (int_val < min || int_val > max) {
        MC_THROW_ERROR("PropertyValueOutOfRange", {0, final_val}, {1, final_name});
    }
}
```

#### MC_THROW_ERROR_WITH_MESSAGE

抛出带有自定义错误消息的异常宏，用于 DBus 调用失败等场景。

```cpp
/**
 * @brief 抛出带有错误消息的异常宏
 *
 * @param error_name 错误名称（需要在错误定义文件中注册）
 * @param error_message 自定义错误消息（使用 MC_LOG_MESSAGE 格式）
 * @param ... 结构化参数列表，使用初始化列表语法：{0, value1}, {1, value2}
 *
 * @example
 * MC_THROW_ERROR_WITH_MESSAGE("DBusMethodCallFailed",
 *                           MC_LOG_MESSAGE(error, "Timeout: ${error_name}",
 *                                     ("error_name", "org.freedesktop.DBus.Error.Timeout")));
 */
#define MC_THROW_ERROR_WITH_MESSAGE(error_name, error_message, ...) \
    throw mc::error_exception(error_name, \
                             mc::dict{}, \
                             mc::log::message(mc::log::level::error, \
                                               mc::log::context(), \
                                               error_message), \
                             error_engine_exception_code)
```

**使用示例**:

```cpp
// 在 DBus 调用中使用
auto reply = m_connection.send_with_reply_and_block(std::move(msg));
if (!reply.is_valid() || !reply.is_method_return()) {
    std::string error_name(reply.get_error_name());
    std::string error_message = reply.get_error_message();
    MC_THROW_ERROR_WITH_MESSAGE(error_name.c_str(),
                              MC_LOG_MESSAGE(error, "${error_name}: ${error_message}",
                                                ("error_name", error_name)
                                                ("error_message", error_message)));
}
```

### 1.3 error 类扩展

**位置**: `include/mc/error.h`

#### from_exception 方法

新增 `error_exception` 重载，支持从错误引擎异常提取结构化参数。

```cpp
struct MC_API error : public mc::enable_shared_from_this<error>,
                 public error_info {
    /**
     * @brief 从异常创建错误（新增 error_exception 重载）
     */
    static mc::shared_ptr<error> from_exception(std::exception_ptr e);
    static mc::shared_ptr<error> from_exception(const mc::exception& e);
    static mc::shared_ptr<error> from_exception(const mc::error_exception& e);  // 新增
    static mc::shared_ptr<error> from_exception(const std::exception& e);
};
```

**实现逻辑**:

```cpp
// 从 error_exception 提取结构化参数
error_ptr error::from_exception(const mc::error_exception& e) {
    auto err = mc::make_shared<error>();
    err->set_name(e.name());
    err->set_args(e.args());  // 复制结构化参数（数字键）
    return err;
}

// 更新 from_exception(std::exception_ptr e) 优先捕获 error_exception
error_ptr error::from_exception(std::exception_ptr e) {
    try {
        std::rethrow_exception(e);
    } catch (mc::error_exception& e) {
        return from_exception(e);  // 优先处理 error_exception
    } catch (mc::exception& e) {
        return from_exception(e);
    } catch (std::exception& e) {
        return from_exception(std::exception_wrapper::from_current_exception(e));
    } catch (...) {
        return from_exception(mc::exception());
    }
}
```

#### get_message 方法

支持自动查找错误格式的增强方法。

```cpp
/**
 * @brief 获取错误消息（支持自动查找格式）
 *
 * 如果 format 为空，自动从错误引擎注册表查找对应错误名的格式
 * 根据格式来源选择正确的格式化方法：
 * - 注册表格式：使用 error_message_parser::format_message (支持 %1, %2)
 * - 直接格式：使用 mc::format_dict (支持 ${key})
 */
std::string get_message() const;
```

**实现逻辑**:

```cpp
std::string error::get_message() const {
    if (m_cached_message.has_value()) {
        return m_cached_message.value();
    }

    std::string_view format_to_use = this->format;
    bool from_registry = false;

    // 如果 format 为空，从错误引擎查找
    if (format_to_use.empty() && !this->name.empty()) {
        auto info = mc::error_engine::get_instance().get_error_info(this->name);
        if (info.is_valid()) {
            format_to_use = info.format;
            from_registry = true;
        }
    }

    if (format_to_use.empty()) {
        return {};
    }

    std::string formatted;
    // 根据格式来源选择格式化方法
    if (from_registry) {
        // 注册表格式：支持 %1, %2 占位符
        formatted = mc::error_message_parser::format_message(format_to_use, args);
    } else {
        // 直接格式：支持 ${key} 占位符
        formatted = mc::format_dict(format_to_use, args);
    }

    m_cached_message = formatted;
    return formatted;
}
```

#### append_arg 整数键重载

支持数组格式参数传递。

```cpp
/**
 * @brief 支持整数 key 的 append_arg 重载（用于数组格式参数传递）
 */
template <typename T>
error& append_arg(int key, T&& value) {
    args[key] = std::forward<T>(value);
    invalidate_cache();
    return *this;
}
```

**使用示例**:

```cpp
auto err = mc::make_shared<mc::error>("PropertyValueOutOfRange",
                                     "The value '%1' for property %2 is out of range.");
err->append_arg(0, "100");
err->append_arg(1, "Threshold");
// 或者
err->append_arg(0, "100").append_arg(1, "Threshold");
```

### 1.4 error_message_converter 类

**位置**: `include/mc/error_message_converter.h`

**描述**: 错误消息转换器，用于将 mc::error 转换为 Redfish 标准消息格式。

```cpp
class MC_API error_message_converter {
public:
    /**
     * @brief 加载错误定义文件
     *
     * @param base_json_path base.json 路径
     * @param custom_json_path custom.json 路径
     */
    void load_registries(std::string_view base_json_path,
                        std::string_view custom_json_path);

    /**
     * @brief 从字符串加载错误定义
     */
    void load_registries_from_string(std::string_view base_json,
                                   std::string_view custom_json);

    /**
     * @brief 查找错误定义
     */
    std::optional<error_message_definition> find_definition(
        std::string_view message_name) const;

    /**
     * @brief 转换错误为标准消息格式
     */
    standard_error_message convert(const mc::error& err) const;

    /**
     * @brief 转换错误为 dict 格式
     */
    mc::dict convert_to_dict(const mc::error& err) const;

    /**
     * @brief 获取单例
     */
    static error_message_converter& get_instance();
};
```

#### standard_error_message 结构

```cpp
struct standard_error_message {
    std::string message_name;       // 错误名称
    std::string message_id;         // 完整消息 ID（如 "Base.1.0.PropertyValueOutOfRange"）
    std::string message;            // 格式化后的消息
    std::string severity;           // 严重级别
    std::string registry_prefix;  // 注册表前缀
    std::string registry_version; // 注册表版本
    int http_status_code;          // HTTP 状态码
    std::string resolution;        // 解决方案
    mc::dict message_args;         // 消息参数（数字键 0, 1, 2...）

    mc::dict to_dict() const;      // 转换为 dict 格式
};
```

**使用示例**:

```cpp
// 加载错误定义
mc::error_message_converter::get_instance().load_registries(
    "/path/to/base.json",
    "/path/to/custom.json"
);

// 转换错误为标准消息格式
auto std_msg = mc::error_message_converter::get_instance().convert(*err);

// std_msg 包含:
// - message_id: "Base.1.0.PropertyValueOutOfRange"
// - message: "The value '100' for property Threshold is out of range."
// - severity: "Warning"
// - message_args: {0: "100", 1: "Threshold"}

// 转换为 dict 格式（用于 JSON 序列化）
auto dict = mc::error_message_converter::get_instance().convert_to_dict(*err);
```

#### 辅助函数

```cpp
/**
 * @brief 转换错误为标准消息格式
 */
standard_error_message to_standard_message(const mc::error& err);

/**
 * @brief 转换错误为 dict 格式
 */
mc::dict to_standard_message_dict(const mc::error& err);
```

---

## 二、Lua 接口

### 2.1 基础错误接口

**模块**: `mc_error`

```lua
-- 加载模块
local mc_error = require("mc_error")

-- ============================================================================
-- 创建新的错误对象
-- ============================================================================
-- @param name 错误名称
-- @param format 格式化字符串（可选）
-- @param params 参数表（可选）或可变参数
-- @return error 对象

-- 使用示例
local err = mc_error.new("PropertyValueOutOfRange",
                        "The value '%1' for property %2 is out of range.",
                        {0, "100"}, {1, "Threshold"})

-- ============================================================================
-- 报告错误到错误引擎
-- ============================================================================
-- @param name 错误名称
-- @param args_dict 参数字典
-- @return error 对象

local err = mc_error.report("PropertyValueOutOfRange", {0, "100", 1, "Threshold"})

-- ============================================================================
-- 获取最后一个错误
-- ============================================================================
-- @return error 对象或 nil

local err = mc_error.last_error()
if err ~= nil then
    print(err:message())
end

-- ============================================================================
-- 错误对象方法
-- ============================================================================

-- 转换为字符串
local str = err:tostring()

-- 获取错误名称
local name = err:name()

-- 获取错误消息
local msg = err:message()

-- 获取参数字典
local args = err:args()

-- 转换为标准消息格式
local std_msg = err:to_standard_message()

-- 转换为 dict 格式
local dict = err:to_dict()

-- 检查错误是否已设置
local is_set = err:is_set()

-- 直接访问属性
print(err.name)       -- 等同于 err:name()
print(err.message)    -- 等同于 err:message()
```

### 2.2 Lua API 兼容性层

#### create_error

创建错误对象并记录日志，支持调用栈跟踪。

**位置**: `src/luaclib/error/l_error.cpp`

```lua
-- ============================================================================
-- create_error(error_info, ...)
-- 创建错误对象并记录日志，支持调用栈跟踪
-- ============================================================================
-- @param error_info 错误信息表，包含以下字段：
--   - Name: 错误名称（必需）
--   - Format: 格式化字符串（必需）
--   - RegistryPrefix: 注册表前缀（可选）
--   - Severity: 严重级别（可选）：trace, debug, info, warning, error, fatal
--   - BacktraceLevel: 栈跟踪级别（可选），默认 0
-- @param ... 可变参数，使用数字键 (0, 1, 2...)
-- @return error 对象

-- 使用示例 1：完整 error_info
local error_info = {
    Name = "PropertyValueOutOfRange",
    Format = "The value '%1' for property %2 is out of range.",
    RegistryPrefix = "Base.1.0",
    Severity = "error",
    BacktraceLevel = 2
}
local err = mc_error\.create_error(error_info, "100", "Threshold")

-- 使用示例 2：简化版本
local err = mc_error\.create_error({
    Name = "PropertyValueOutOfRange",
    Format = "The value '%1' for property %2 is out of range.",
    Severity = "error",
    BacktraceLevel = 2
}, "100", "Threshold")

-- 实现逻辑：
-- function create_error(error_info, ...)
--     local err_data = new_error(error_info.Name, error_info.Format, ...)
--     err_data.registry_prefix = error_info.RegistryPrefix
--     print_log(error_info.Severity, error_info.Format, ...)
--     if error_info.BacktraceLevel > 0 then
--         print_trace(error_info.BacktraceLevel, err_data)
--     end
--     return err_data
-- end
```

**行为说明**:

1. 从 `error_info.Name` 和 `error_info.Format` 创建错误对象
2. 设置 `registry_prefix`（如果提供）
3. 调用 `print_log` 记录日志（如果提供 `Severity`）
4. 如果 `BacktraceLevel > 0`，调用 `print_trace` 打印调用栈

#### new_error

创建新的错误对象，支持格式化参数。

```lua
-- ============================================================================
-- new_error(name, fmt, params, ...)
-- 创建新的错误对象，支持格式化参数
-- ============================================================================
-- @param name 错误名称
-- @param fmt 格式化字符串（可选），默认 ""
-- @param params 参数表（可选）或可变参数
-- @return error 对象

-- 使用示例 1：使用参数表
local err = mc_error\.new_error("PropertyValueOutOfRange",
                              "The value '%1' for property %2 is out of range.",
                              {0, "100"}, {1, "Threshold"})

-- 使用示例 2：使用可变参数（自动使用数字键 0, 1, 2...）
local err = mc_error\.new_error("PropertyValueOutOfRange",
                              "The value '%1' for property %2 is out of range.",
                              "100", "Threshold")
```

**关键特性**:

- 第 3 个参数如果是 table，作为参数字典
- 如果不是 table，自动收集可变参数，使用数字键 (0, 1, 2...)
- 支持 Redfish %1, %2 格式占位符

#### safe_call

安全调用函数，捕获并返回错误而不传播。

```lua
-- ============================================================================
-- safe_call(func, ...)
-- 安全调用函数，捕获并返回错误而不传播
-- ============================================================================
-- @param func 要调用的函数
-- @param ... 传递给函数的参数
-- @return 成功: nil, result1, result2, ...
--         失败: error 对象

-- 使用示例 1：成功情况
local function add(a, b)
    return a + b
end

local ok, result = mc_error\.safe_call(add, 1, 2)
if ok == nil then
    -- 成功，result 是返回值
    print("Success: " .. result)  -- 输出: Success: 3
end

-- 使用示例 2：失败情况
local function divide(a, b)
    if b == 0 then
        error("Division by zero")
    end
    return a / b
end

local ok, result_or_err = mc_error\.safe_call(divide, 10, 0)
if ok ~= nil then
    -- 失败，result_or_err 是 error 对象
    print("Error: " .. result_or_err:message())
end

-- 使用示例 3：返回多个值
local function get_user_data(id)
    return "User" .. id, id * 10, "active"
end

local ok, name, score, status = mc_error\.safe_call(get_user_data, 123)
if ok == nil then
    print(name, score, status)  -- 输出所有返回值
end
```

**实现逻辑**:

```lua
-- function safe_call(f, ...)
--     return process_result(xpcall(f, error_func, ...))
-- end
--
-- function process_result(ok, ...)
--     if ok then
--         return nil, ...
--     end
--     return ...
-- end
--
-- function error_func(e)
--     if getmetatable(e) == Error then
--         return e
--     end
--     local is_table = (type(e) == 'table' or type(e) == 'userdata')
--     return new_error(is_table and e.name or 'Error.Unknown',
--                    is_table and e.message or tostring(e))
-- end
```

**关键特性**:

- 成功时返回 `nil` 后跟所有返回值
- 失败时返回错误对象
- 检查错误是否已有 Error metatable，避免重复包装
- 支持从 table 提取 `name` 和 `message` 字段

#### raise_error

抛出错误，通过 new_error 创建错误对象然后抛出。

```lua
-- ============================================================================
-- raise_error(name, message, params, ...)
-- 抛出错误，通过 new_error 创建错误对象然后抛出
-- ============================================================================
-- @param name 错误名称
-- @param format 格式化字符串（可选），默认 ""
-- @param params 参数表（可选）或可变参数
-- @throws Lua 错误（使用 lua_error）

-- 使用示例 1：使用参数表
mc_error\.raise_error("PropertyValueOutOfRange",
                    "The value '%1' for property %2 is out of range.",
                    {0, "100"}, {1, "Threshold"})

-- 使用示例 2：使用可变参数
mc_error\.raise_error("PropertyValueOutOfRange",
                    "The value '%1' for property %2 is out of range.",
                    "100", "Threshold")

-- 使用示例 3：在条件判断中使用
if not validate_value(value, threshold) then
    mc_error\.raise_error("PropertyValueOutOfRange",
                        "Validation failed for ${value}",
                        {value = tostring(value)})
end
```

**行为说明**:

1. 创建错误对象（等同于 `new_error`）
2. 将错误对象推入 Lua 栈
3. 调用 `lua_error(L)` 抛出错误（使用 Lua longjmp）

#### print_log

增强的日志打印方法，自动包含调用栈信息（文件名和行号）。

```lua
-- ============================================================================
-- print_log(severity, format, ...)
-- 增强的日志打印方法，自动包含调用栈信息（文件名和行号）
-- ============================================================================
-- @param severity 严重级别：trace, debug, info, warning, error, fatal
-- @param format 格式化字符串
-- @param ... 可变参数

-- 使用示例 1：基本用法
mc_error\.print_log("info", "Processing value: ${value}", {value = 100})
mc_error\.print_log("error", "Operation failed: ${reason}", {reason = "Timeout"})

-- 使用示例 2：不同严重级别
mc_error\.print_log("trace", "Entering function")
mc_error\.print_log("debug", "Variable x = ${x}", {x = 42})
mc_error\.print_log("info", "Processing complete")
mc_error\.print_log("warning", "Deprecated API used")
mc_error\.print_log("error", "Operation failed")
mc_error\.print_log("fatal", "Critical error")
```

**日志输出格式**:

```
[filename:line] [severity] message
```

**示例输出**:

```
[script.lua:42] [info] Processing value: 100
[script.lua:45] [error] Operation failed: Timeout
```

#### print_trace

打印错误调用栈跟踪信息。

```lua
-- ============================================================================
-- print_trace(backtrace_level, err_data)
-- 打印错误调用栈跟踪信息
-- ============================================================================
-- @param backtrace_level 栈跟踪级别（默认 0）
-- @param err_data 错误对象（可选）

-- 使用示例 1：打印当前调用栈
mc_error\.print_trace(0)

-- 使用示例 2：打印错误对象的调用栈
mc_error\.print_trace(2, err)

-- 使用示例 3：打印完整调用栈
mc_error\.print_trace(10)
```

**输出示例**:

```
========== 错误调用栈跟踪 ==========
  [0] script.lua:42 in function 'validate_value'
  [1] script.lua:78 in function 'process_request'
  [2] script.lua:100 in main chunk
=====================================
```

### 2.3 错误消息转换器接口

**模块**: `mc_error_converter`

```lua
-- ============================================================================
-- 错误消息转换器（用于 Redfish 等标准格式）
-- ============================================================================

-- 加载错误定义文件
-- @param base_path base.json 文件路径
-- @param custom_path custom.json 文件路径
-- @return boolean 成功返回 true，失败返回 false, error_message
local ok, err_msg = mc_error_converter.load_registries(
    "/path/to/base.json",
    "/path/to/custom.json"
)
if not ok then
    print("Failed to load: " .. err_msg)
end

-- 从字符串加载错误定义
-- @param base_json base.json 内容字符串
-- @param custom_json custom.json 内容字符串
local ok, err_msg = mc_error_converter.load_registries_from_string(
    base_json_string,
    custom_json_string
)

-- 转换错误为标准消息格式
-- @param err 错误对象
-- @return standard_message 表（包含 message_id, message, severity 等字段）
local std_msg = mc_error_converter.convert(err)
print(std_msg.message_id)        -- "Base.1.0.PropertyValueOutOfRange"
print(std_msg.message)            -- "The value '100' for property Threshold is out of range."
print(std_msg.severity)           -- "Warning"
print(std_msg.registry_prefix)    -- "Base"
print(std_msg.registry_version)   -- "1.0"

-- 转换错误为 dict 格式
-- @param err 错误对象
-- @return dict
local dict = mc_error_converter.convert_to_dict(err)
```

---

## 三、使用流程示例

### 3.1 C++ 侧使用流程

#### 场景 1：验证器中抛出错误

```cpp
#include <mc/exception.h>
#include <mc/log.h>

void validate_property_value(const mc::variant& value,
                          const std::string& property_name,
                          int64_t min,
                          int64_t max) {
    std::string final_val = mc::to_string(value);

    // 检查类型
    if (!is_valid_integer(final_val)) {
        MC_THROW_ERROR("PropertyValueTypeError",
                      {0, final_val},
                      {1, property_name});
    }

    // 检查范围
    int64_t int_val = std::stoll(final_val);
    if (int_val < min || int_val > max) {
        MC_THROW_ERROR("PropertyValueOutOfRange",
                      {0, final_val},
                      {1, property_name});
    }
}
```

#### 场景 2：DBus 调用失败处理

```cpp
#include <mc/dbus/bus_mode_impl.h>
#include <mc/exception.h>

variants blocking_bus_impl::call(const std::string& service,
                               const std::string& path,
                               const std::string& interface,
                               const std::string& method,
                               const std::string& signature,
                               variants&& args) {
    auto msg = message::new_method_call(service, path, interface, method);
    // ... 写入参数 ...

    auto reply = m_connection.send_with_reply_and_block(std::move(msg));

    if (reply.is_valid() && reply.is_method_return()) {
        return reply.read_args();
    }

    // DBus 调用失败，抛出错误异常
    std::string error_name(reply.get_error_name());
    std::string error_message = reply.get_error_message();

    MC_THROW_ERROR_WITH_MESSAGE(error_name.c_str(),
                              MC_LOG_MESSAGE(error, "${error_name}: ${error_message}",
                                                ("error_name", error_name)
                                                ("error_message", error_message)));
}
```

#### 场景 3：服务中捕获并转换

```cpp
#include <mc/engine/service.h>
#include <mc/exception.h>
#include <mc/error.h>

void service::handle_method_call(context& ctx,
                              const message& msg,
                              call_info& info) {
    try {
        // 处理方法调用
        process_method(ctx, msg, info);

    } catch (const mc::error_exception& e) {
        // 捕获错误引擎异常
        auto err = mc::error::from_exception(e);
        auto ctx_err = ctx.get_error();

        if (ctx_err) {
            // 使用上下文错误
            info.response = mc::dbus::message::new_error(msg,
                                                        ctx_err->name,
                                                        ctx_err->to_string_format_inplace());
        } else {
            // 使用转换后的错误
            info.response = mc::dbus::message::new_error(msg,
                                                        errors::failed.name,
                                                        err->get_message());
        }
    }
}
```

#### 场景 4：转换为 Redfish 标准格式

```cpp
#include <mc/error_message_converter.h>
#include <mc/error.h>

void send_redfish_response(const mc::error_ptr& err) {
    // 加载错误定义（通常在启动时完成）
    static bool loaded = false;
    if (!loaded) {
        mc::error_message_converter::get_instance().load_registries(
            "/etc/redfish/base.json",
            "/etc/redfish/custom.json"
        );
        loaded = true;
    }

    // 转换为标准消息格式
    auto std_msg = mc::error_message_converter::get_instance().convert(*err);

    // 构建 Redfish 响应
    redfish::Response response;
    response.status_code = std_msg.http_status_code;
    response.json_body = {
        {"error", {
            {"code", std_msg.message_id},
            {"message", std_msg.message}
        }}
    };

    // 发送响应
    send_response(response);
}
```

### 3.2 Lua 侧使用流程

#### 场景 1：创建和处理错误

```lua
local mc_error = require("mc_error")

-- 创建错误对象（使用可变参数）
local function validate_value(value, threshold)
    if value > threshold then
        local err = mc_error.new_error(
            "PropertyValueOutOfRange",
            "The value '%1' for property %2 is out of range.",
            tostring(value),
            tostring(threshold)
        )

        -- 记录日志和调用栈
        mc_error.print_log("error", "Validation failed: ${value}", {value = value})
        mc_error.print_trace(2, err)

        -- 抛出错误
        mc_error.raise_error(
            "PropertyValueOutOfRange",
            "The value '%1' for property %2 is out of range.",
            tostring(value),
            tostring(threshold)
        )
    end
end

-- 使用
local ok, err = mc_error.safe_call(validate_value, 150, 100)
if ok ~= nil then
    print("Error: " .. err:message())
    print("Args: " .. err:tostring())
end
```

#### 场景 2：使用 create_error

```lua
local mc_error = require("mc_error")

-- 处理用户输入
local function process_user_input(input)
    local threshold = 100

    if input.value > threshold then
        -- 创建错误对象（带日志和栈跟踪）
        local err = mc_error.create_error({
            Name = "PropertyValueOutOfRange",
            Format = "The value '%1' for property %2 is out of range.",
            Severity = "error",
            BacktraceLevel = 2
        }, tostring(input.value), tostring(threshold))

        -- 返回错误给调用者
        return nil, err
    end

    return true, nil
end

-- 调用
local success, err = process_user_input({value = 150})
if not success then
    print("Error: " .. err:message())

    -- 转换为标准消息格式（用于 Redfish 响应）
    local std_msg = mc_error_converter.convert(err)
    print("Message ID: " .. std_msg.message_id)
    print("HTTP Status: " .. std_msg.http_status_code)
end
```

#### 场景 3：安全调用模式

```lua
local mc_error = require("mc_error")

-- 定义可能失败的业务逻辑
local function risky_operation(param1, param2)
    -- 模拟错误
    if param1 == nil then
        error("Parameter 1 is required")
    end

    if param2 > 100 then
        error("Parameter 2 out of range")
    end

    return param1 + param2
end

-- 使用 safe_call 包装
local function call_with_safety(p1, p2)
    local ok, result_or_err = mc_error.safe_call(risky_operation, p1, p2)

    if ok == nil then
        -- 成功
        return true, result_or_err
    else
        -- 失败
        print("Operation failed: " .. result_or_err:message())
        print("Error name: " .. result_or_err:name())

        -- 打印调用栈
        mc_error.print_trace(2, result_or_err)

        return false, result_or_err
    end
end

-- 使用
local success, result = call_with_safety(10, 20)
if success then
    print("Result: " .. result)
end
```

#### 场景 4：Redfish 响应生成

```lua
local mc_error = require("mc_error")
local mc_error_converter = require 'mc_error_converter'

-- 初始化（通常在应用启动时完成）
mc_error_converter.load_registries(
    "/etc/redfish/base.json",
    "/etc/redfish/custom.json"
)

-- 处理 HTTP 请求
local function handle_request(request)
    -- 验证属性
    local function validate_property(value, schema)
        if schema.minimum and value < schema.minimum then
            return nil, mc_error.new_error(
                "PropertyValueOutOfRange",
                "The value '%1' for property %2 is below minimum %3.",
                tostring(value),
                schema.property_name,
                tostring(schema.minimum)
            )
        end

        if schema.maximum and value > schema.maximum then
            return nil, mc_error.new_error(
                "PropertyValueOutOfRange",
                "The value '%1' for property %2 is above maximum %3.",
                tostring(value),
                schema.property_name,
                tostring(schema.maximum)
            )
        end

        return true, nil
    end

    -- 验证请求体
    local ok, err = validate_property(request.body.value, request.schema)
    if not ok then
        -- 转换为 Redfish 标准格式
        local std_msg = mc_error_converter.convert(err)

        -- 构建错误响应
        return {
            status_code = std_msg.http_status_code,
            body = {
                error = {
                    code = std_msg.message_id,
                    message = std_msg.message
                }
            }
        }
    end

    -- 处理成功
    return {
        status_code = 200,
        body = { value = request.body.value }
    }
end
```

---

## 四、关键设计要点

### 4.1 数字键系统

**设计原则**: C++ 和 Lua 都使用数字键 (0, 1, 2...) 而非字符串键，支持 Redfish %1, %2 格式。

**C++ 侧**:

```cpp
// 正确：使用数字键
MC_THROW_ERROR("PropertyValueOutOfRange", {0, "100"}, {1, "Threshold"});

// 错误：使用字符串键
// MC_THROW_ERROR("PropertyValueOutOfRange", {"value", "100"}, {"name", "Threshold"});
```

**Lua 侧**:

```lua
-- 正确：使用数字键（自动或手动）
mc_error\.new_error("ErrorName", "Format %1 %2", "100", "Threshold")

-- 错误：使用字符串键
-- mc_error\.new_error("ErrorName", "Format ${value} ${name}", {value = "100", name = "Threshold"})
```

**原因**: Redfish 标准使用 %1, %2 占位符，数字键直接对应参数位置。

### 4.2 双格式化路径

**设计原则**: 根据格式来源选择正确的格式化方法。

| 格式来源 | 占位符格式 | 格式化方法 | 使用场景 |
|---------|----------|-----------|---------|
| 错误引擎注册表 | `%1`, `%2`, ... | `error_message_parser::format_message` | Redfish 标准错误 |
| 直接指定 | `${key}` | `mc::format_dict` | 自定义错误 |

**示例**:

```cpp
// 注册表格式（使用 %1, %2）
auto err = mc::make_shared<mc::error>("PropertyValueOutOfRange");  // format 从注册表获取
err->append_arg(0, "100");
err->append_arg(1, "Threshold");
// 格式化结果: "The value '100' for property Threshold is out of range."

// 直接格式（使用 ${key}）
auto err = mc::make_shared<mc::error>("CustomError", "Value: ${value}, Name: ${name}");
err->append_arg("value", "100");
err->append_arg("name", "Threshold");
// 格式化结果: "Value: 100, Name: Threshold"
```

### 4.3 统一异常类型

**设计原则**: 所有错误引擎相关异常使用 `error_exception`。

**异常层次**:

```
std::exception
    └── mc::exception
            ├── mc::unhandled_exception
            ├── mc::std_exception_wrapper
            ├── mc::error_exception          ← 错误引擎专用
            └── mc::timeout_exception
            └── ... (其他标准异常)
```

**使用建议**:

- **验证器错误**: 使用 `MC_THROW_ERROR` 抛出 `error_exception`
- **DBus 错误**: 使用 `MC_THROW_ERROR_WITH_MESSAGE` 抛出 `error_exception`
- **其他错误**: 使用对应的专用异常类（如 `timeout_exception`）

### 4.4 自动格式查找

**设计原则**: `error::get_message` 在 format 为空时自动查找注册表。

**查找顺序**:

1. 检查 `format` 字段是否为空
2. 如果为空，从错误引擎查找 `name` 对应的格式
3. 如果找到，使用注册表格式
4. 如果未找到，返回空字符串

**示例**:

```cpp
// 场景 1：format 已指定
auto err = mc::make_shared<mc::error>("ErrorName", "Custom message: ${value}");
err->append_arg("value", "100");
std::string msg = err->get_message();  // "Custom message: 100"

// 场景 2：format 为空，自动查找
auto err = mc::make_shared<mc::error>("PropertyValueOutOfRange");  // format 为 ""
err->append_arg(0, "100");
err->append_arg(1, "Threshold");
std::string msg = err->get_message();  // 从注册表查找并格式化
```

### 4.5 错误转换

**设计原则**: `error::from_exception` 支持从 `error_exception` 提取结构化参数。

**转换流程**:

```
error_exception (异常)
    ↓ from_exception
mc::error (错误对象)
    ↓ convert
standard_error_message (标准消息)
    ↓ to_dict
mc::dict (字典)
    ↓ JSON 序列化
Redfish JSON 响应
```

**关键点**:

1. `error_exception.args()` 包含数字键的结构化参数
2. `error::from_exception(const error_exception&)` 复制这些参数到 `mc::error.args`
3. `error_message_converter::convert` 使用这些参数格式化消息
4. `standard_error_message.to_dict` 转换为可 JSON 序列化的字典

### 4.6 Lua 安全调用

**设计原则**: `safe_call` 区分已有错误对象和新建错误对象。

**错误处理流程**:

```
lua_pcall 失败
    ↓
检查错误对象 metatable
    ↓
├── 是 Error metatable → 直接使用该错误对象
└── 不是 Error metatable → 创建新错误对象
    ├── 如果是 table/userdata → 提取 name 和 message
    └── 如果是其他类型 → 转换为字符串作为 message
```

**关键代码**:

```lua
function error_func(e)
    -- 检查是否已有 Error metatable
    if getmetatable(e) == Error then
        return e  -- 直接返回
    end

    -- 创建新错误对象
    local is_table = (type(e) == 'table' or type(e) == 'userdata')
    return new_error(
        is_table and e.name or 'Error.Unknown',
        is_table and e.message or tostring(e)
    )
end
```

### 4.7 错误定义文件格式

**base.json / custom.json 格式**:

```json
{
    "registry_prefix": "Base",
    "registry_version": "1.0",
    "language": "en",
    "messages": [
        {
            "name": "PropertyValueOutOfRange",
            "format": "The value '%1' for property %2 is out of range.",
            "severity": "Warning",
            "resolution": "Reduce the value to be within the minimum and maximum range.",
            "http_status_code": 400
        },
        {
            "name": "PropertyValueTypeError",
            "format": "The property %1 is of type %2. The type %3 is not supported by this property.",
            "severity": "Warning",
            "resolution": "Change the value type or use a property that supports this type.",
            "http_status_code": 400
        }
    ]
}
```

**字段说明**:

| 字段 | 类型 | 必需 | 说明 |
|-----|------|------|------|
| registry_prefix | string | 是 | 注册表前缀（如 "Base", "Custom"）|
| registry_version | string | 是 | 注册表版本（如 "1.0", "2.1"）|
| language | string | 否 | 语言代码（默认 "en"）|
| messages | array | 是 | 错误定义数组 |
| messages[].name | string | 是 | 错误名称（唯一标识符）|
| messages[].format | string | 是 | 格式化字符串（支持 %1, %2...）|
| messages[].severity | string | 否 | 严重级别 |
| messages[].resolution | string | 否 | 解决方案描述 |
| messages[].http_status_code | int | 否 | HTTP 状态码 |

---

## 附录

### A. 完整异常代码列表

**位置**: `include/mc/exception.h`

```cpp
enum exception_code {
    // 通用异常代码
    unknow_exception_code           = 0,   // 未知异常代码
    unhandled_exception_code        = 1,   // 未处理的第三方异常
    timeout_exception_code          = 2,   // 超时异常
    file_not_found_exception_code   = 3,   // 文件未找到异常
    parse_error_exception_code      = 4,   // 解析错误异常
    invalid_arg_exception_code      = 5,   // 无效参数异常
    key_not_found_exception_code    = 6,   // 键未找到异常
    bad_cast_exception_code         = 7,   // 类型转换异常
    out_of_range_exception_code     = 8,   // 越界异常
    canceled_exception_code         = 9,   // 取消操作异常
    assert_exception_code           = 10,  // 断言异常
    eof_exception_code              = 11,  // 文件结束异常
    system_error_code               = 12,  // 系统错误异常
    std_exception_code              = 13,  // 标准库异常
    invalid_op_exception_code       = 14,  // 无效操作异常
    null_optional_code              = 15,  // 空可选值异常
    overflow_code                  = 16,  // 溢出异常
    underflow_code                 = 17,  // 下溢异常
    divide_by_zero_code            = 18,  // 除零异常
    bad_function_call_code          = 19,  // 函数调用异常
    bad_alloc_code                 = 20,  // 内存分配异常
    busy_exception_code             = 21,  // 繁忙异常
    method_call_exception_code      = 22,  // 方法调用异常
    not_implemented_exception_code  = 23,  // 未实现异常
    bad_property_exception_code     = 24,  // 属性错误
    bad_method_exception_code       = 25,  // 方法错误
    bad_type_exception_code         = 26,  // 类型错误
    not_found_exception_code        = 27,  // 未找到异常
    invalid_argument_exception_code        = 28,  // 无效参数异常
    runtime_exception_code                 = 29,  // 运行时异常
    format_error_code                      = 30,  // 格式化错误异常
    insufficient_privilege_exception_code  = 31,  // 权限不足异常
    password_changed_required_exception_code = 32,  // 需要修改密码异常
    error_engine_exception_code        = 33,  // 错误引擎异常（新增）
};
```

### B. 标准异常类列表

**位置**: `include/mc/exception.h`

```cpp
// 使用宏定义的标准异常类
MC_DECLARE_EXCEPTION_CLASS(timeout_exception, timeout_exception_code, "操作超时", "timeout")
MC_DECLARE_EXCEPTION_CLASS(file_not_found_exception, file_not_found_exception_code, "文件未找到", "file_not_found")
MC_DECLARE_EXCEPTION_CLASS(parse_error_exception, parse_error_exception_code, "解析错误", "parse_error")
MC_DECLARE_EXCEPTION_CLASS(invalid_arg_exception, invalid_arg_exception_code, "无效参数", "invalid_arg")
MC_DECLARE_EXCEPTION_CLASS(key_not_found_exception, key_not_found_exception_code, "键未找到", "key_not_found")
MC_DECLARE_EXCEPTION_CLASS(assert_exception, assert_exception_code, "断言失败", "assert")
MC_DECLARE_EXCEPTION_CLASS(bad_cast_exception, bad_cast_exception_code, "类型转换错误", "bad_cast")
MC_DECLARE_EXCEPTION_CLASS(out_of_range_exception, out_of_range_exception_code, "索引越界", "out_of_range")
MC_DECLARE_EXCEPTION_CLASS(canceled_exception, canceled_exception_code, "操作已取消", "canceled")
MC_DECLARE_EXCEPTION_CLASS(eof_exception, eof_exception_code, "文件结束", "eof")
MC_DECLARE_EXCEPTION_CLASS(system_exception, system_error_code, "系统错误", "system")
MC_DECLARE_EXCEPTION_CLASS(invalid_op_exception, invalid_op_exception_code, "无效操作", "invalid_operation")
MC_DECLARE_EXCEPTION_CLASS(null_optional_exception, null_optional_code, "访问空可选值", "null_optional")
MC_DECLARE_EXCEPTION_CLASS(overflow_exception, overflow_code, "数值溢出", "overflow")
MC_DECLARE_EXCEPTION_CLASS(underflow_exception, underflow_code, "数值下溢", "underflow")
MC_DECLARE_EXCEPTION_CLASS(divide_by_zero_exception, divide_by_zero_code, "除零错误", "divide_by_zero")
MC_DECLARE_EXCEPTION_CLASS(file_open_exception, file_not_found_exception_code, "无法打开文件", "file_open")
MC_DECLARE_EXCEPTION_CLASS(not_implemented_exception, not_implemented_exception_code, "未实现", "not_implemented")
MC_DECLARE_EXCEPTION_CLASS(bad_function_call_exception, bad_function_call_code, "函数调用错误", "bad_function_call")
MC_DECLARE_EXCEPTION_CLASS(bad_alloc_exception, bad_alloc_code, "内存分配错误", "bad_alloc")
MC_DECLARE_EXCEPTION_CLASS(busy_exception, busy_exception_code, "繁忙", "busy")
MC_DECLARE_EXCEPTION_CLASS(method_call_exception, method_call_exception_code, "方法调用错误", "method_call")
MC_DECLARE_EXCEPTION_CLASS(bad_property_exception, bad_property_exception_code, "属性错误", "bad_property")
MC_DECLARE_EXCEPTION_CLASS(bad_method_exception, bad_method_exception_code, "方法错误", "bad_method")
MC_DECLARE_EXCEPTION_CLASS(bad_type_exception, bad_type_exception_code, "类型错误", "bad_type")
MC_DECLARE_EXCEPTION_CLASS(not_found_exception, not_found_exception_code, "未找到", "not_found")
MC_DECLARE_EXCEPTION_CLASS(invalid_argument_exception, invalid_argument_exception_code, "无效参数", "invalid_argument")
MC_DECLARE_EXCEPTION_CLASS(runtime_exception, runtime_exception_code, "运行时错误", "runtime")
MC_DECLARE_EXCEPTION_CLASS(format_error, format_error_code, "格式化错误", "format_error")
MC_DECLARE_EXCEPTION_CLASS(insufficient_privilege_exception, insufficient_privilege_exception_code, "权限不足", "insufficient_privilege")
MC_DECLARE_EXCEPTION_CLASS(password_changed_required_exception, password_changed_required_exception_code, "需要修改密码", "password_changed_required")
```

### C. 相关文件索引

| 文件路径 | 说明 |
|---------|------|
| `include/mc/exception.h` | 异常类定义和宏 |
| `include/mc/error.h` | 错误类定义 |
| `include/mc/error_message_converter.h` | 错误消息转换器定义 |
| `include/mc/error_message_parser.h` | 错误消息解析器定义 |
| `include/mc/error_engine.h` | 错误引擎定义 |
| `src/exception.cpp` | 异常类实现 |
| `src/error.cpp` | 错误类实现 |
| `src/error_message_converter.cpp` | 错误消息转换器实现 |
| `src/error_message_parser.cpp` | 错误消息解析器实现 |
| `src/validate/validator.cpp` | 验证器实现（使用 MC_THROW_ERROR）|
| `src/dbus/bus_mode_impl.cpp` | DBus 总线模式实现（使用 MC_THROW_ERROR_WITH_MESSAGE）|
| `src/engine/service.cpp` | 服务实现（捕获 error_exception）|
| `src/luaclib/error/l_error.h` | Lua 错误接口头文件 |
| `src/luaclib/error/l_error.cpp` | Lua 错误接口实现 |
