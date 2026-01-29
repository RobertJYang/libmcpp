# Validate API

## 概述

`mc::validate::validator` 提供一组面向 C++ 的通用资源协作接口参数校验能力，并且其异常消息格式与 Lua 模块 `lvalidate` 保持一致，便于在 C++ 与 Lua 两侧复用同一套规则与报错文案。

## 头文件与命名空间

```cpp
#include <mc/validate/validator.h>
```

命名空间：

```cpp
namespace mc::validate {
    class validator;
}
```

## 异常类型

所有校验失败都会抛出异常（均继承自 `mc::validate::validation_exception`，并最终继承 `std::runtime_error`）：

- `property_value_type_error`：类型不符合（例如期望整数但传入 `3.14`，或传入 NaN）
- `property_value_out_of_range`：数值超出范围
- `string_length_error`：字符串长度不满足约束（过短/过长）
- `format_error`：格式不匹配（例如正则不匹配，或正则编译失败）
- `json_error`：JSON 解析失败

## API 说明

### format_name_and_value

```cpp
static std::pair<std::string, std::string> format_name_and_value(
    std::string_view name,
    std::string_view val_str,
    bool need_convert
);
```

用于生成与 `lvalidate` 一致的错误消息占位字符串：

- `need_convert == false`：返回 `(name, val_str)`
- `need_convert == true`：
  - name 变为 `%<name>`
  - val 变为 `%<name>:<val_str>`

### check_integer

```cpp
static void check_integer(
    std::string_view name,
    double val,
    double min,
    double max,
    bool need_convert
);
```

校验规则：

- `val` 必须为“整数值”（例如 `42`、`42.0` 通过；`3.14` 不通过；NaN 不通过）
- `val` 必须在 `[min, max]` 之间（包含边界）

失败时抛出：

- `property_value_type_error`
- `property_value_out_of_range`

### ranges

```cpp
static void ranges(
    std::string_view name,
    double val,
    double min,
    double max,
    bool need_convert,
    bool allow_nil = false
);
```

校验 `val` 是否在 `[min, max]` 之间（包含边界），失败时抛出 `property_value_out_of_range`。

说明：

- `allow_nil` 用于与 Lua `range_or_none` 的语义对齐（Lua 侧 `val == nil` 直接通过）。
- C++ 侧没有 `nil` 的概念，建议调用方使用 `std::optional<T>` 或在调用前自行判断是否需要跳过校验。

### lens

```cpp
static void lens(
    std::string_view name,
    std::string_view val,
    int min,
    int max,
    bool need_convert,
    bool allow_nil = false
);
```

校验字符串长度是否在 `[min, max]` 之间（包含边界），失败时抛出 `string_length_error`。

说明：

- 长度按 `std::string_view::length()`（字节数）计算。
- `allow_nil` 语义同 `ranges`。

### regex

```cpp
static void regex(
    std::string_view name,
    std::string_view val,
    std::string_view pattern,
    bool need_convert,
    bool allow_nil = false
);
```

使用 GLib 的正则（`GRegex`）校验 `val` 是否匹配 `pattern`：

- 正则编译失败：抛出 `format_error`
- 正则不匹配：抛出 `format_error`

说明：

- `allow_nil` 语义同 `ranges`。

### json

```cpp
static void json(std::string_view val);
```

使用 `mc::json::json_decode` 校验 JSON 字符串，失败时抛出 `json_error`。

## 基本用法示例

```cpp
#include <mc/validate/validator.h>

void validate_request(std::string_view user, double age, std::string_view payload) {
    // 整数 + 范围
    mc::validate::validator::check_integer("Age", age, 0, 200, false);

    // 字符串长度
    mc::validate::validator::lens("User", user, 1, 64, false);

    // JSON
    mc::validate::validator::json(payload);
}
```

异常处理示例：

```cpp
try {
    mc::validate::validator::check_integer("Prop1", 3.14, 0, 100, false);
} catch (const mc::validate::validation_exception& e) {
    // e.what() 的文案与 Lua lvalidate 报错保持一致
}
```

## 与 Lua `lvalidate` 的关系

- Lua 模块 `lvalidate` 的 C 接口会调用 `mc::validate::validator` 完成核心校验。
- 当 C++ 抛出 `mc::validate::validation_exception` 时，Lua 侧会把 `e.what()` 直接作为 Lua error 抛出，从而保证 Lua 的错误消息与历史行为一致。

