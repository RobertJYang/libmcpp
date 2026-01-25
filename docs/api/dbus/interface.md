# D-Bus 接口模块文档

本文档介绍了为Lua提供的DBus接口模块，允许在Lua脚本中创建和管理D-Bus接口。

## 概述

D-Bus接口模块提供了用于创建和管理D-Bus接口的Lua API，包括其关联的属性、方法和信号。它遵循D-Bus规范标准，适用于接口命名、成员名称和类型签名。

## 创建接口

### `new(name)`
使用指定名称创建新的D-Bus接口。

**参数：**
- [name](字符串)：D-Bus接口名称。必须遵循D-Bus命名约定（例如："org.freedesktop.DBus.Properties"）。

**返回值：**
- 可用于注册属性、方法和信号的接口对象。

**示例：**
```lua
local ldbus = require('ldbus')
local l_interface = ldbus.interface
local my_interface = l_interface.new("com.example.MyInterface")
```

## 接口方法

### `add_property(name, signature, default_value, readonly, flags)`
向接口添加属性。

**参数：**
- [name] (字符串)：遵循D-Bus成员命名规则的属性名称（必须以字母/下划线开头，只能包含字母数字字符和下划线）。
- [signature] (字符串)：属性值的D-Bus类型签名（例如："s"表示字符串，"i"表示整数）。
- `default_value`：属性的默认值。
- [readonly] (布尔)：属性是否为只读。
- [flags] (数字)：属性的附加标志。

**示例：**
```lua
my_interface:add_property("Status", "s", "unknown", true, 0)  -- 只读字符串属性
my_interface:add_property("Count", "i", 0, false, 0)         -- 可写的整数属性
```

### `add_method(name, input_signature, output_signature, callback, flags)`
向接口添加方法。

**参数：**
- [name] (字符串)：遵循D-Bus成员命名规则的方法名称。
- `input_signature` (字符串)：输入参数的D-Bus类型签名。
- `output_signature` (字符串)：输出参数的D-Bus类型签名。
- `callback` (函数)：调用方法时执行的Lua函数。
- [flags] (数字)：方法的附加标志。

**示例：**
```lua
local function my_method_impl(param1, param2)
    -- 实现代码在这里
    return result
end

my_interface:add_method("DoSomething", "ss", "i", my_method_impl, 0)
```

### `add_signal(name, signature, flags)`
向接口添加信号。

**参数：**
- [name] (字符串)：遵循D-Bus成员命名规则的信号名称。
- [signature] (字符串)：信号参数的D-Bus类型签名。
- [flags] (数字)：信号的附加标志。

**示例：**
```lua
my_interface:add_signal("StateChanged", "s", 0)
```

### [set_property(name, value)]
设置属性的值。

**参数：**
- [name] (字符串)：要设置的属性名称。
- [value]：属性的新值。

**注意：** 尝试设置只读属性会抛出错误。

**示例：**
```lua
my_interface:set_property("Status", "active")
```

### [get_property(name)]
获取属性的值。

**参数：**
- [name] (字符串)：要获取的属性名称。

**返回值：**
- 属性的当前值。

**示例：**
```lua
local current_status = my_interface:get_property("Status")
print("当前状态: " .. current_status)
```

### `get_name()`
获取接口的名称。

**返回值：**
- 包含接口名称的字符串。

**示例：**
```lua
local interface_name = my_interface:get_name()
print("接口名称: " .. interface_name)
```

## 支持的D-Bus签名

该模块支持所有标准的D-Bus类型签名：

| 代码 | 类型 | 描述 |
|------|------|-------------|
| y | BYTE | 8位无符号整数 |
| b | BOOLEAN | 布尔值 |
| n | INT16 | 16位有符号整数 |
| q | UINT16 | 16位无符号整数 |
| i | INT32 | 32位有符号整数 |
| u | UINT32 | 32位无符号整数 |
| x | INT64 | 64位有符号整数 |
| t | UINT64 | 64位无符号整数 |
| d | DOUBLE | 64位浮点数 |
| s | STRING | UTF-8字符串 |
| o | OBJECT_PATH | 对象路径 |
| g | SIGNATURE | 类型签名 |
| a | ARRAY | 项目数组 |
| ( ) | STRUCT | 结构化数据 |
| { } | DICT_ENTRY | 字典条目 |
| v | VARIANT | 变体类型 |

### 复杂类型示例：
- `"as"` - 字符串数组
- `"a{sv}"` - 映射字符串到变体的字典
- `"(ii)"` - 包含两个整数的结构
- `"a(ii)"` - 包含两个整数结构的数组

## 错误处理

当以下情况发生时，模块会抛出Lua错误：
- 使用无效的接口、方法、属性或信号名称
- 提供无效的D-Bus签名
- 尝试设置只读属性
- 尝试访问不存在的属性

## 完整示例

```lua
-- 创建新接口
local my_interface = l_interface.new("com.example.SampleInterface")

-- 添加属性
my_interface:add_property("Version", "s", "1.0.0", true, 0)      -- 只读版本字符串
my_interface:add_property("Temperature", "d", 20.0, false, 0)   -- 可写的温度值

-- 添加方法
local function convert_temp(celsius)
    local fahrenheit = celsius * 9/5 + 32
    return fahrenheit
end

my_interface:add_method("ConvertCelsiusToF", "d", "d", convert_temp, 0)

-- 添加信号
my_interface:add_signal("TemperatureChanged", "d", 0)

-- 使用接口
my_interface:set_property("Temperature", 25.5)
local temp = my_interface:get_property("Temperature")
print("温度: " .. temp)
```

## 验证规则

- 接口名称：必须遵循反向域名约定（例如："com.example.MyInterface"）
- 成员名称：必须以字母或下划线开头，后跟字母、数字或下划线
- 签名：必须符合D-Bus类型签名规范
- 长度限制：接口名称≤255个字符，单个组件≤100个字符