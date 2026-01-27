# lvalidate Lua API 文档

## 概述

`lvalidate` 是一个 Lua 参数校验库，提供参数的类型和范围验证功能。

## 模块加载

```lua
local lvalidate = require('lvalidate')
```

## 函数

### check_integer

验证一个值是否为整数类型，并且是否在指定的范围内。

#### 语法

```lua
lvalidate.check_integer(name, val, min, max, need_convert)
```

#### 参数

- `name` (string) - 属性名称，用于错误消息中标识属性
- `val` (number) - 要验证的值
- `min` (number) - 允许的最小值（包含）
- `max` (number) - 允许的最大值（包含）
- `need_convert` (boolean) - 是否在错误消息中使用转换格式
  - `true` - 错误消息中的属性名和值会添加 `%` 前缀（如 `%Prop1` 和 `%Prop1:150`）
  - `false` - 使用原始格式（如 `Prop1` 和 `150`）

#### 返回值

- 无返回值。如果验证通过，函数正常返回；如果验证失败，会抛出 Lua 错误。

#### 异常

函数在以下情况下会抛出 Lua 错误：

1. **类型错误** (`PropertyValueTypeError`)：
   - `val` 不是数字类型
   - `val` 是浮点数但有小数部分（如 `3.14`、`5.5`）
   - `val` 是 NaN（Not a Number）

2. **范围错误** (`PropertyValueOutOfRange`)：
   - `val` 小于 `min` 或大于 `max`
   - `min` 或 `max` 不是数字类型

#### 验证规则

1. **类型检查**：
   - `val` 必须是数字类型（`LUA_TNUMBER`）
   - 如果 `val` 是浮点数，必须没有小数部分（即必须是整数值）
   - 例如：`42` ✓、`42.0` ✓、`3.14` ✗、`5.5` ✗

2. **范围检查**：
   - `val` 必须在 `[min, max]` 范围内（包含边界）
   - `min` 和 `max` 必须是数字类型
   - 例如：`val=50, min=0, max=100` ✓、`val=150, min=0, max=100` ✗

3. **边界值**：
   - 边界值（`min` 和 `max`）是包含的
   - 例如：`val=0, min=0, max=100` ✓、`val=100, min=0, max=100` ✓

#### 错误消息格式

**类型错误**：
```
PropertyValueTypeError: The value '<val>' for the property <name> is of a different type than the property can accept.
```

**范围错误**：
```
PropertyValueOutOfRange: The value '<val>' for the property <name> is not in the supported range of acceptable values.
```

当 `need_convert` 为 `true` 时：
- 属性名格式：`%<name>`（如 `%Prop1`）
- 值格式：`%<name>:<val>`（如 `%Prop1:150`）

#### 示例

##### 基本用法

```lua
local lvalidate = require('lvalidate')

-- 验证有效整数
lvalidate.check_integer("Prop1", 42, 0, 100, false)
-- 成功，无返回值

-- 验证边界值
lvalidate.check_integer("Prop1", 0, 0, 100, false)   -- 最小值边界
lvalidate.check_integer("Prop1", 100, 0, 100, false) -- 最大值边界
```

##### 错误处理

```lua
local lvalidate = require('lvalidate')

-- 捕获类型错误
local success, err = pcall(lvalidate.check_integer, "Prop1", 3.14, 0, 100, false)
if not success then
    print("类型错误:", err)
    -- 输出: PropertyValueTypeError: The value '3.14' for the property Prop1 ...
end

-- 捕获范围错误
local success, err = pcall(lvalidate.check_integer, "Prop1", 150, 0, 100, false)
if not success then
    print("范围错误:", err)
    -- 输出: PropertyValueOutOfRange: The value '150' for the property Prop1 ...
end
```

##### 使用 need_convert

```lua
local lvalidate = require('lvalidate')

-- need_convert = false（默认格式）
local success, err = pcall(lvalidate.check_integer, "Prop1", 150, 0, 100, false)
-- 错误消息: PropertyValueOutOfRange: The value '150' for the property Prop1 ...

-- need_convert = true（转换格式）
local success, err = pcall(lvalidate.check_integer, "Prop1", 150, 0, 100, true)
-- 错误消息: PropertyValueOutOfRange: The value '%Prop1:150' for the property %Prop1 ...
```

##### 负数和跨零范围

```lua
local lvalidate = require('lvalidate')

-- 负数范围
lvalidate.check_integer("Temperature", -5, -10, 0, false)  -- ✓ 通过

-- 跨零范围
lvalidate.check_integer("Offset", 0, -10, 10, false)     -- ✓ 通过
lvalidate.check_integer("Offset", -5, -10, 10, false)     -- ✓ 通过
lvalidate.check_integer("Offset", 5, -10, 10, false)      -- ✓ 通过
```

##### 相等边界

```lua
local lvalidate = require('lvalidate')

-- min == max == val
lvalidate.check_integer("FixedValue", 5, 5, 5, false)  -- ✓ 通过
```

### lens

验证字符串长度是否在指定的范围内。

#### 语法

```lua
lvalidate.lens(name, val, min, max, need_convert)
```

#### 参数

- `name` (string) - 属性名称，用于错误消息中标识属性
- `val` (string) - 要验证的字符串
- `min` (number) - 允许的最小长度（包含）
- `max` (number) - 允许的最大长度（包含）
- `need_convert` (boolean) - 是否在错误消息中使用转换格式
  - `true` - 错误消息中的属性名和值会添加 `%` 前缀（如 `%Prop1` 和 `%Prop1:abc`）
  - `false` - 使用原始格式（如 `Prop1` 和 `abc`）

#### 返回值

- 无返回值。如果验证通过，函数正常返回；如果验证失败，会抛出 Lua 错误。

#### 异常

函数在以下情况下会抛出 Lua 错误：

1. **类型错误** (`PropertyValueTypeError`)：
   - `val` 不是字符串类型

2. **长度小于下限** (`StringValueTooShort`)：
   - `val` 长度小于 `min`

3. **长度大于上限** (`StringValueTooLong`)：
   - `val` 长度大于 `max`

### len_or_none

验证字符串长度是否在指定的范围内，允许入参值为 `nil`。

#### 语法

```lua
lvalidate.len_or_none(name, val, min, max, need_convert)
```

#### 参数

- `name` (string) - 属性名称，用于错误消息中标识属性
- `val` (string | nil) - 要验证的字符串；为 `nil` 时跳过检查
- `min` (number) - 允许的最小长度（包含）
- `max` (number) - 允许的最大长度（包含）
- `need_convert` (boolean) - 是否在错误消息中使用转换格式
  - `true` - 错误消息中的属性名和值会添加 `%` 前缀（如 `%Prop1` 和 `%Prop1:abc`）
  - `false` - 使用原始格式（如 `Prop1` 和 `abc`）

#### 返回值

- 无返回值。如果验证通过，函数正常返回；如果验证失败，会抛出 Lua 错误。

#### 行为说明

- 当 `val == nil` 时，直接返回，不抛出错误
- 当 `val ~= nil` 时，行为与 `lens` 相同

#### 异常

函数在以下情况下会抛出 Lua 错误：

1. **类型错误** (`PropertyValueTypeError`)：
   - `val` 不是nil，并且不是字符串类型

2. **长度小于下限** (`StringValueTooShort`)：
   - `val` 长度小于 `min`

3. **长度大于上限** (`StringValueTooLong`)：
   - `val` 长度大于 `max`

### ranges

验证一个数值是否在指定的范围内（允许浮点数）。

#### 语法

```lua
lvalidate.ranges(name, val, min, max, need_convert)
```

#### 参数

- `name` (string) - 属性名称，用于错误消息中标识属性
- `val` (number) - 要验证的值（整数或浮点数）
- `min` (number) - 允许的最小值（包含）
- `max` (number) - 允许的最大值（包含）
- `need_convert` (boolean) - 是否在错误消息中使用转换格式
  - `true` - 错误消息中的属性名和值会添加 `%` 前缀（如 `%Prop1` 和 `%Prop1:1.23`）
  - `false` - 使用原始格式（如 `Prop1` 和 `1.23`）

#### 返回值

- 无返回值。如果验证通过，函数正常返回；如果验证失败，会抛出 Lua 错误。

#### 行为说明

- `val` 必须是 Lua 数字类型
- `val` 必须在 `[min, max]` 范围内（包含边界）
- `min` 和 `max` 必须是数字类型，否则会抛出错误

#### 异常

函数在以下情况下会抛出 Lua 错误：

1. **类型错误** (`PropertyValueTypeError`)：
   - `val` 不是数字类型

2. **范围错误** (`PropertyValueOutOfRange`)：
   - `val` 小于 `min` 或大于 `max`

### range_or_none

验证一个数值是否在指定的范围内，允许入参为 `nil`（此时不做范围检查）。

#### 语法

```lua
lvalidate.range_or_none(name, val, min, max, need_convert)
```

#### 参数

- `name` (string) - 属性名称，用于错误消息中标识属性
- `val` (number | nil) - 要验证的值；为 `nil` 时跳过检查
- `min` (number) - 允许的最小值（包含）
- `max` (number) - 允许的最大值（包含）
- `need_convert` (boolean) - 是否在错误消息中使用转换格式

#### 返回值

- 无返回值。如果验证通过，函数正常返回；如果验证失败，会抛出 Lua 错误。

#### 行为说明

- 当 `val == nil` 时，直接返回，不抛出错误
- 当 `val ~= nil` 时，行为同 `ranges`

#### 异常

函数在以下情况下会抛出 Lua 错误：

1. **类型错误** (`PropertyValueTypeError`)：
   - `val` 不是nil，并且不是数字类型

2. **范围错误** (`PropertyValueOutOfRange`)：
   - `val` 不是nil，并且小于 `min` 或大于 `max`

### regex

验证字符串是否匹配指定的标准正则表达式。

#### 语法

```lua
lvalidate.regex(name, val, pattern, need_convert)
```

#### 参数

- `name` (string) - 属性名称，用于错误消息中标识属性
- `val` (string) - 要验证的字符串
- `pattern` (string) - 标准正则表达式
- `need_convert` (boolean) - 是否在错误消息中使用转换格式

#### 返回值

- 无返回值。如果验证通过，函数正常返回；如果验证失败，会抛出 Lua 错误。

#### 行为说明

- 当 `val` 不是字符串时，抛出类型相关错误
- 当 `val` 不匹配给定的 `pattern` 时，抛出匹配失败的错误

#### 异常

函数在以下情况下会抛出 Lua 错误：

1. **类型错误** (`PropertyValueTypeError`)：
   - `val` 不是字符串类型

2. **正则不匹配** (`PropertyValueFormatError`)：
   - `val` 不匹配给定的 `pattern`

### regex_or_none

验证字符串是否匹配指定的标准正则表达式，允许入参为 `nil`。

#### 语法

```lua
lvalidate.regex_or_none(name, val, pattern, need_convert)
```

#### 参数

- `name` (string) - 属性名称，用于错误消息中标识属性
- `val` (string | nil) - 要验证的字符串；为 `nil` 时跳过检查
- `pattern` (string) - 标准正则表达式
- `need_convert` (boolean) - 是否在错误消息中使用转换格式

#### 返回值

- 无返回值。如果验证通过，函数正常返回；如果验证失败，会抛出 Lua 错误。

#### 行为说明

- 当 `val == nil` 时，直接返回，不抛出错误
- 当 `val ~= nil` 时，行为同 `regex`

#### 异常

函数在以下情况下会抛出 Lua 错误：

1. **类型错误** (`PropertyValueTypeError`)：
   - `val` 不是nil，并且不是字符串类型

2. **正则不匹配** (`PropertyValueFormatError`)：
   - `val` 不是nil，并且不匹配给定的 `pattern`

### Json

验证一个字符串是否是合法的 JSON 文本。

#### 语法

```lua
lvalidate.Json(val)
```

#### 参数

- `val` (string) - 要验证的字符串

#### 返回值

- 无返回值。如果验证通过，函数正常返回；如果验证失败，会抛出 Lua 错误。

#### 行为说明

- 当 `val` 不是字符串时，抛出 `MalformedJSON` 错误
- 使用 JSON 解析库对 `val` 进行反序列化
- 如果反序列化失败（不是合法的 JSON），抛出 `MalformedJSON` 错误

#### 异常

函数在以下情况下会抛出 Lua 错误：

1. **不合法JSON** (`MalformedJSON`)：
   - `val` 不是字符串类型，或
   - `val` 不是合法的 JSON 字符串