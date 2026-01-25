# D-Bus 对象模块文档

本文档介绍了为Lua提供的D-Bus对象模块，允许在Lua脚本中创建和管理D-Bus对象及其关联的接口。

## 概述

D-Bus对象模块提供了用于创建和管理D-Bus对象的Lua API，这些对象可以托管多个D-Bus接口，并允许跨接口访问属性。该模块遵循D-Bus规范标准，适用于对象路径和接口操作。

## 创建对象

### `new_object(path)`
使用指定的对象路径创建新的D-Bus对象。

**参数：**
- [path] (字符串)：D-Bus对象路径。必须遵循D-Bus对象路径命名约定（例如："/com/example/ObjectPath"）。

**返回值：**
- 可用于注册接口和访问属性的对象实例。

**示例：**
```lua
local ldbus = require('ldbus')
local l_object = ldbus.object
local my_object = mc.dbus.new("/com/example/MyObject")
```

## 对象方法

### `register_interface(interface)`
向对象注册一个接口。

**参数：**
- `interface`：之前创建的接口实例。

**示例：**
```lua
local my_interface = ldbus.interface.new("com.example.MyInterface")
my_object:register_interface(my_interface)
```

### [get_interface(intf_name)]
根据接口名称获取已注册的接口。

**参数：**
- `intf_name` (字符串)：接口的名称。

**返回值：**
- 指定名称的接口实例，如果未找到则返回nil。

**示例：**
```lua
local retrieved_interface = my_object:get_interface("com.example.MyInterface")
if retrieved_interface then
    print("Interface found: " .. retrieved_interface:get_name())
end
```

### `set_property(intf_name, prop_name, value)`
设置指定接口上属性的值。

**参数：**
- `intf_name` (字符串)：接口名称。
- `prop_name` (字符串)：要设置的属性名称。
- [value]：属性的新值。

**注意：** 尝试设置只读属性会抛出错误。

**示例：**
```lua
my_object:set_property("com.example.MyInterface", "Status", "active")
```

### [get_property(intf_name, prop_name)]
获取指定接口上属性的值。

**参数：**
- `intf_name` (字符串)：接口名称。
- `prop_name` (字符串)：要获取的属性名称。

**返回值：**
- 指定属性的当前值。

**示例：**
```lua
local current_status = my_object:get_property("com.example.MyInterface", "Status")
print("当前状态: " .. current_status)
```

## 对象路径验证

对象路径必须遵循D-Bus规范：
- 必须以'/'开头
- 路径组件之间用'/'分隔
- 每个组件必须以字母或下划线开头
- 组件只能包含字母、数字和下划线
- 不能以'/'结尾（除非是根路径"/"）

## 错误处理

当以下情况发生时，模块会抛出Lua错误：
- 使用无效的对象路径
- 尝试访问不存在的接口
- 尝试访问接口上不存在的属性
- 尝试设置只读属性

## 完整示例

```lua
-- 创建对象
local ldbus = require('ldbus')
local l_object = ldbus.object
local l_interface = ldbus.interface
local my_object = l_object.new("/com/example/SampleObject")

-- 创建并注册接口
local interface1 = l_interface.new("com.example.Interface1")
local interface2 = l_interface.new("com.example.Interface2")

-- 为接口添加属性
interface1:add_property("Version", "s", "1.0.0", true, 0)
interface1:add_property("Status", "s", "inactive", false, 0)

interface2:add_property("Temperature", "d", 20.0, false, 0)
interface2:add_property("DeviceID", "s", "DEV001", true, 0)

-- 注册接口到对象
my_object:register_interface(interface1)
my_object:register_interface(interface2)

-- 通过对象访问不同接口的属性
my_object:set_property("com.example.Interface1", "Status", "active")
local status = my_object:get_property("com.example.Interface1", "Status")
print("接口1状态: " .. status)

my_object:set_property("com.example.Interface2", "Temperature", 25.5)
local temp = my_object:get_property("com.example.Interface2", "Temperature")
print("接口2温度: " .. temp)

-- 获取特定接口
local retrieved_interface = my_object:get_interface("com.example.Interface1")
if retrieved_interface then
    print("获取到接口名称: " .. retrieved_interface:get_name())
end
```

## 架构关系

- **对象(Object)**：代表D-Bus上的一个对象路径，可以托管多个接口
- **接口(Interface)**：定义了一组相关的方法、属性和信号
- **属性(Property)**：接口上可读写的值
- **方法(Method)**：可在接口上调用的操作
- **信号(Signal)**：接口发出的通知

## 验证规则

- 对象路径：必须遵循D-Bus对象路径规范，长度限制等
- 接口名称：必须与注册到对象中的接口名称完全匹配
- 属性访问：必须先注册接口才能访问其属性
- 跨接口隔离：不同接口的同名属性相互独立