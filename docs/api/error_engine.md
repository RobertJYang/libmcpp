# 错误引擎 C++ API 文档

## 概述

`mc::error_engine` 是 libmcpp 提供的统一错误处理框架，用于在 C++ 应用程序中创建、管理和传递错误信息。错误引擎采用三层架构设计，提供了灵活的错误定义、格式化、序列化和异常集成功能。

核心组件包括:

- `error_info` - 轻量级错误信息载体
- `error` - 完整的错误对象，支持参数、序列化和调用栈
- `error_with_owner` - 持有字符串所有权的错误类
- `error_engine` - 错误注册和管理中心（单例）

## 主要特性

- **统一错误处理** - 提供一致的错误定义和处理方式
- **灵活的参数支持** - 支持命名参数和位置参数
- **消息格式化** - 支持 `%n` 和 `${key}` 两种占位符格式
- **JSON 序列化** - 支持错误对象的序列化和反序列化
- **调用栈捕获** - 自动捕获和保存调用栈信息
- **异常集成** - 与标准异常和 mc::exception 无缝集成
- **线程安全** - 支持线程局部错误存储
- **懒加载优化** - 消息格式化采用懒加载，提升性能

## 基本用法

### 创建错误对象

```cpp
#include <mc/error.h>
#include <mc/error_engine.h>

// 使用 make_error 创建
auto err = mc::make_error("file_not_found", "文件 ${path} 不存在");
err("path", "/etc/config.conf");

// 使用 error_info 创建
mc::error_info info("timeout", "操作超时");
mc::error err2(info);

// 使用 error_with_owner 创建（持有字符串所有权）
mc::error_with_owner err3("custom_error", "自定义错误: ${msg}");
err3("msg", "错误详情");
```

### 访问错误信息

```cpp
mc::error err = mc::make_error("error", "发生错误");

// 获取错误名称和格式
std::string_view name = err.get_name();      // "error"
std::string_view format = err.get_format();  // "发生错误"

// 设置和获取错误级别
err.set_level(mc::error_level::warning);
mc::error_level level = err.get_level();  // error_level::warning

// 检查错误是否已设置
if (err.is_set()) {
    std::cout << "错误已设置" << std::endl;
}
```

### 错误格式化

```cpp
mc::error err = mc::make_error("user_error", "用户 ${name} 的年龄是 ${age}");
err("name", "张三")("age", 30);

std::string message = err.get_message();
std::cout << message << std::endl;
// "用户 张三 的年龄是 30"
```

### 错误序列化

```cpp
mc::error err = mc::make_error("error", "发生错误");
err("code", 100);

// 序列化为 JSON
std::string json = err.encode();
std::cout << json << std::endl;

// 从 JSON 反序列化
auto err2 = mc::error::decode(json);
```

---

## error_info

`error_info` 是轻量级错误信息结构，使用 `std::string_view` 引用字符串，不持有字符串所有权。

### 构造函数

| 函数 | 描述 |
|------|------|
| `error_info()` | 默认构造函数 |
| `error_info(std::string_view name, std::string_view format = {}, error_level level = error_level::error)` | 构造错误信息 |

**示例**:

```cpp
mc::error_info info1("file_not_found");
mc::error_info info2("timeout", "操作超时");
mc::error_info info3("warning", "警告", mc::error_level::warning);
```

### 成员变量

| 变量 | 类型 | 描述 |
|------|------|------|
| `name` | `std::string_view` | 错误名称 |
| `format` | `std::string_view` | 格式字符串 |
| `level` | `error_level` | 错误级别 |

### 方法

#### is_valid

```cpp
bool is_valid();
```

检查错误信息是否有效（错误名称非空）。

**返回值**: 有效返回 `true`，否则返回 `false`

**示例**:

```cpp
mc::error_info info1("error");
mc::error_info info2;

bool valid1 = info1.is_valid();  // true
bool valid2 = info2.is_valid();  // false
```

---

## error

`error` 是完整的错误对象，提供错误参数管理、消息格式化、序列化等完整功能。

### 构造函数

| 函数 | 描述 |
|------|------|
| `error()` | 创建空错误对象 |
| `error(const error_info& info)` | 从错误信息创建 |
| `error(std::string_view name, std::string_view format, error_level level = error_level::error)` | 直接创建错误对象 |
| `error(const error& other)` | 复制构造函数 |

**示例**:

```cpp
// 默认构造
mc::error err1;

// 从 error_info 构造
mc::error_info info("timeout", "操作超时");
mc::error err2(info);

// 直接构造
mc::error err3("error", "发生错误");
mc::error err4("warning", "警告", mc::error_level::warning);

// 复制构造
mc::error err5(err3);
```

### 成员变量

| 变量 | 类型 | 描述 |
|------|------|------|
| `args` | `mc::dict` | 错误参数字典 |
| `registry_prefix` | `std::string` | 注册表前缀 |
| `prev_error` | `mc::shared_ptr<error>` | 前一个错误（错误链） |

### 属性访问方法

| 函数 | 描述 |
|------|------|
| `std::string_view get_name() const` | 获取错误名称 |
| `std::string_view get_format() const` | 获取格式字符串 |
| `const mc::dict& get_args() const` | 获取错误参数字典 |
| `std::string get_message() const` | 获取格式化后的消息（懒加载） |
| `error_level get_level() const` | 获取错误级别 |
| `void set_level(error_level level)` | 设置错误级别 |
| `void set_name(std::string_view name)` | 设置错误名称 |
| `void set_format(std::string_view format)` | 设置格式字符串 |

**示例**:

```cpp
mc::error err("error", "操作 ${action} 失败");
err("action", "保存数据");

// 访问属性
std::cout << "名称: " << err.get_name() << std::endl;
std::cout << "格式: " << err.get_format() << std::endl;
std::cout << "消息: " << err.get_message() << std::endl;

// 修改属性
err.set_level(mc::error_level::warning);
err.set_name("new_error");
err.set_format("新格式: ${detail}");
```

### 参数管理方法

#### operator()

```cpp
template <typename T>
error& operator()(std::string_view key, T&& value);
```

链式添加参数。

**参数**:

- `key` - 参数名
- `value` - 参数值

**返回值**: `*this`，支持链式调用

**示例**:

```cpp
mc::error err("error", "用户 ${name} 的年龄是 ${age}，住在 ${city}");
err("name", "张三")("age", 30)("city", "北京");

std::cout << err.get_message() << std::endl;
// "用户 张三 的年龄是 30，住在 北京"
```

#### append_arg

```cpp
template <typename T>
error& append_arg(std::string_view key, T&& value);
template <typename T>
error& append_arg(int key, T&& value);
```

添加单个参数。支持字符串键和整数键。

**参数**:

- `key` - 参数名（字符串或整数）
- `value` - 参数值

**返回值**: `*this`，支持链式调用

**示例**:

```cpp
mc::error err("error", "索引 %1 超出范围，最大值: %2");
err.append_arg(0, 100);      // 设置 %1 = 100
err.append_arg(1, 50);       // 设置 %2 = 50

std::cout << err.get_message() << std::endl;
// "索引 100 超出范围，最大值: 50"
```

#### operator%

```cpp
error& operator%(mc::dict args);
```

批量设置错误参数。

**参数**:

- `args` - 参数字典

**返回值**: `*this`

**示例**:

```cpp
mc::error err("error", "用户 ${name} 的年龄是 ${age}");

err % mc::dict{
    {"name", "李四"},
    {"age", 25}
};

std::cout << err.get_message() << std::endl;
// "用户 李四 的年龄是 25"
```

#### set_args

```cpp
error& set_args(const mc::dict& args);
```

设置错误参数（替换现有参数）。

**参数**:

- `args` - 参数字典

**返回值**: `*this`

**示例**:

```cpp
mc::error err("error", "${greeting}, ${name}");

// 先设置一组参数
err("greeting", "你好")("name", "王五");
std::cout << err.get_message() << std::endl;
// "你好, 王五"

// 替换为新的参数
err.set_args(mc::dict{
    {"greeting", "Hello"},
    {"name", "Alice"}
});
std::cout << err.get_message() << std::endl;
// "Hello, Alice"
```

### 序列化方法

#### encode

```cpp
std::string encode(const mc::json::json_encode_options& options =
                       mc::json::json_encode_options::default_encode_options) const;
```

将错误对象序列化为 JSON 字符串。

**参数**:

- `options` - JSON 编码选项（可选）

**返回值**: JSON 字符串

**示例**:

```cpp
mc::error err("validation_error", "验证失败: ${reason}");
err("reason", "参数为空")("field", "username");

std::string json = err.encode();
std::cout << json << std::endl;
```

输出:
```json
{
  "name": "validation_error",
  "message": "验证失败: 参数为空",
  "format": "验证失败: ${reason}",
  "params": {
    "reason": "参数为空",
    "field": "username"
  }
}
```

#### decode

```cpp
static mc::shared_ptr<error> decode(
    std::string_view json,
    const mc::json::json_decode_options& options =
        mc::json::json_decode_options::default_decode_options);
```

从 JSON 字符串反序列化错误对象。

**参数**:

- `json` - JSON 字符串
- `options` - JSON 解码选项（可选）

**返回值**: 错误对象指针

**异常**: 当 JSON 格式不正确时抛出 `mc::parse_error_exception`

**示例**:

```cpp
std::string json = R"({
    "name": "error",
    "message": "发生错误",
    "format": "发生错误"
})";

try {
    auto err = mc::error::decode(json);
    std::cout << "错误名称: " << err->get_name() << std::endl;
} catch (const mc::parse_error_exception& e) {
    std::cerr << "JSON 解析失败" << std::endl;
}
```

### 调用栈方法

#### traceback

```cpp
void traceback();
```

捕获当前调用栈并保存到错误对象中。

**示例**:

```cpp
mc::error err("error", "发生错误");
err.traceback();

std::string stack = err.get_traceback();
std::cout << "调用栈:\n" << stack << std::endl;
```

#### get_traceback

```cpp
const std::string& get_traceback() const noexcept;
```

获取已保存的调用栈信息。

**返回值**: 调用栈字符串的常量引用

**示例**:

```cpp
mc::error err("error", "发生错误");
err.traceback();

const std::string& stack = err.get_traceback();
if (!stack.empty()) {
    std::cout << "有调用栈信息" << std::endl;
}
```

### 异常集成方法

#### from_exception

```cpp
static mc::shared_ptr<error> from_exception(std::exception_ptr e);
static mc::shared_ptr<error> from_exception(const mc::exception& e);
static mc::shared_ptr<error> from_exception(const mc::error_exception& e);
static mc::shared_ptr<error> from_exception(const std::exception& e);
```

从异常对象创建错误对象。

**参数**:

- `e` - 异常对象

**返回值**: 错误对象指针

**示例**:

```cpp
try {
    throw std::runtime_error("文件打开失败");
} catch (const std::exception& e) {
    auto err = mc::error::from_exception(e);
    std::cout << "捕获异常: " << err->get_message() << std::endl;
}
```

#### raise

```cpp
[[noreturn]] void raise() const;
```

抛出异常。此方法总是抛出 `mc::error_exception`，不会返回。

**异常**: 总是抛出 `mc::error_exception`

**示例**:

```cpp
mc::error err("error", "操作失败");
err("reason", "无效参数");

if (invalid_input) {
    err.raise();  // 抛出异常
}
```

### 工具方法

#### to_string

```cpp
std::string to_string() const;
```

将错误转换为字符串表示。

**返回值**: 字符串（格式: "name: message"）

**示例**:

```cpp
mc::error err("error", "操作失败");
std::string str = err.to_string();
std::cout << str << std::endl;
// "error: 操作失败"
```

#### is_set

```cpp
bool is_set() const;
```

检查错误是否已设置（错误名称非空或有前一个错误）。

**返回值**: 已设置返回 `true`，否则返回 `false`

**示例**:

```cpp
mc::error err1("error", "错误");
mc::error err2;

bool set1 = err1.is_set();  // true
bool set2 = err2.is_set();  // false
```

#### has_error

```cpp
bool has_error(std::string_view name) const;
```

检查错误链中是否包含特定名称的错误。

**参数**:

- `name` - 错误名称

**返回值**: 包含返回 `true`，否则返回 `false`

**示例**:

```cpp
mc::error err1("file_error", "文件错误");
mc::error err2("io_error", "IO 错误");
err2.prev_error = mc::make_shared<mc::error>(err1);

bool has_file = err2.has_error("file_error");  // true
bool has_io = err2.has_error("io_error");      // true
bool has_net = err2.has_error("net_error");    // false
```

---

## error_with_owner

`error_with_owner` 继承自 `error`，持有错误名称和格式字符串的所有权。适用于需要动态构造错误名或格式字符串的场景。

### 构造函数

| 函数 | 描述 |
|------|------|
| `error_with_owner()` | 创建空对象 |
| `error_with_owner(std::string name, std::string format)` | 创建对象（持有所有权） |

**示例**:

```cpp
// 动态构造错误名
std::string error_name = "error_" + std::to_string(100);
mc::error_with_owner err(error_name, "发生错误");
```

---

## error_engine

`error_engine` 是错误注册和管理中心，采用单例模式。

### 单例访问

| 函数 | 描述 |
|------|------|
| `static error_engine& get_instance()` | 获取单例实例 |
| `static void reset_for_test()` | 重置引擎（仅用于测试） |

**示例**:

```cpp
auto& engine = mc::error_engine::get_instance();
```

### 错误注册方法

| 函数 | 描述 |
|------|------|
| `error_info register_const_error(const error_info& info)` | 注册常量错误 |
| `error_info register_const_error(std::string_view name, std::string_view format = {}, error_level level = error_level::error)` | 注册常量错误 |
| `error_info register_error(std::string name, std::string format, error_level level = error_level::error)` | 注册动态错误 |

**示例**:

```cpp
auto& engine = mc::error_engine::get_instance();

// 注册常量错误
engine.register_const_error("file_not_found", "文件 ${path} 不存在");

// 使用宏注册（推荐）
REGISTER_CONST_ERROR(TIMEOUT, "timeout", "操作超时");
```

### 错误查询方法

| 函数 | 描述 |
|------|------|
| `error_info get_error_info(std::string_view name)` | 获取错误信息 |
| `bool is_registered(std::string_view name)` | 检查是否已注册 |

**示例**:

```cpp
auto& engine = mc::error_engine::get_instance();

auto info = engine.get_error_info("timeout");
if (info.is_valid()) {
    std::cout << "错误已注册" << std::endl;
}

bool registered = engine.is_registered("unknown_error");  // false
```

### 错误报告方法

| 函数 | 描述 |
|------|------|
| `error_ptr report_error(std::string_view name, mc::dict args = {})` | 报告错误 |
| `error_ptr report_error(const error_info& info, mc::dict args = {})` | 报告错误 |
| `error_ptr set_last_error(error_ptr new_error)` | 设置最后错误 |
| `error_ptr last_error()` | 获取最后错误 |
| `void reset_error()` | 清除最后错误 |

**示例**:

```cpp
auto& engine = mc::error_engine::get_instance();

// 报告错误
auto err = engine.report_error("file_not_found", {{"path", "/etc/config"}});
std::cout << err->get_message() << std::endl;

// 线程局部错误
engine.set_last_error(err);
auto last = engine.last_error();
engine.reset_error();
```

---

## 辅助函数

### make_error

```cpp
MC_API error_ptr make_error(std::string_view name, std::string_view format);
MC_API error_ptr make_error(const error_info& info);
```

创建错误对象。

**参数**:

- `name` - 错误名称
- `format` - 格式字符串
- `info` - 错误信息

**返回值**: 错误对象指针

**示例**:

```cpp
auto err = mc::make_error("error", "发生错误");
err("code", 100);
```

### is_valid_error_name

```cpp
MC_API bool is_valid_error_name(std::string_view name);
```

检查错误名称是否有效。

**参数**:

- `name` - 错误名称

**返回值**: 有效返回 `true`，否则返回 `false`

### get_error_format_args

```cpp
MC_API bool get_error_format_args(std::string_view format, mc::dict& arg_names);
```

解析格式字符串中的占位符。

**参数**:

- `format` - 格式字符串
- `arg_names` - 输出参数，存储占位符名称

**返回值**: 解析成功返回 `true`，否则返回 `false`

**示例**:

```cpp
mc::dict arg_names;
mc::get_error_format_args("用户 ${name} 的年龄是 ${age}", arg_names);
// arg_names 包含 {"name": true, "age": true}
```

---

## 宏定义

### DECLARE_ERROR

声明错误变量，通常在头文件中使用。

```cpp
// 头文件
DECLARE_ERROR(FILE_NOT_FOUND);
DECLARE_ERROR(TIMEOUT);
```

### REGISTER_CONST_ERROR

注册常量错误，通常在源文件中使用。

```cpp
REGISTER_CONST_ERROR(FILE_NOT_FOUND, "file_not_found", "文件 ${path} 不存在");
REGISTER_CONST_ERROR(TIMEOUT, "timeout", "操作超时", mc::error_level::warning);

// 使用
auto err = mc::make_error(FILE_NOT_FOUND);
err("path", "/etc/config");
```

### REGISTER_ERROR

注册动态错误（持有字符串所有权）。

```cpp
std::string format = "自定义错误: ${msg}";
REGISTER_ERROR(CUSTOM_ERROR, "custom_error", format);
```

---

## 最佳实践

### 选择合适的错误类

```cpp
// 轻量级场景：使用 error_info
void log_error(const mc::error_info& info);

// 常规场景：使用 error
mc::error err("error", "发生错误");
err("param", "value");

// 动态构造：使用 error_with_owner
std::string name = "error_" + code;
mc::error_with_owner err2(name, "错误");
```

### 错误注册策略

```cpp
// 在程序启动时集中注册
void init_errors() {
    auto& engine = mc::error_engine::get_instance();
    engine.register_const_error("file_not_found", "文件 ${path} 不存在");
    engine.register_const_error("timeout", "操作超时");
}
```

### 异常安全使用

```cpp
try {
    if (error_condition) {
        mc::error err("error", "操作失败");
        err.raise();
    }
} catch (const mc::error_exception& e) {
    auto err = mc::error::from_exception(e);
    std::cout << err->get_message() << std::endl;
}
```

### 性能优化

```cpp
// 懒加载：只在首次调用时格式化
mc::error err("error", "${name}");
err("name", "张三");

std::string msg1 = err.get_message();  // 执行格式化
std::string msg2 = err.get_message();  // 返回缓存
```

### 线程安全

```cpp
// 每个线程有独立的 last_error
auto& engine = mc::error_engine::get_instance();
engine.set_last_error(mc::make_error("error", "线程1的错误"));

// 不同线程的 last_error 互不影响
```

### 错误链

```cpp
// 建立错误链
auto io_err = mc::make_error("io_error", "IO 错误");
auto app_err = mc::make_error("app_error", "应用错误");
app_err.prev_error = mc::make_shared<mc::error>(io_err);

// 遍历错误链
for (auto* e = &app_err; e != nullptr; e = e->prev_error.get()) {
    std::cout << e->get_message() << std::endl;
}
```
