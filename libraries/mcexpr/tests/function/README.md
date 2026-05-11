# 函数相关测试 (function)

## 概述

本目录包含表达式引擎函数相关功能的测试用例，包括函数调用、函数解析、关联属性（relate property）处理等功能。

## 测试文件

- `test_call.cpp`: 函数调用测试 (12个测试用例)
- `test_parser.cpp`: 函数解析测试 (38个测试用例，已优化，删除了重复的测试用例)

**总测试用例数**: 50 个（已优化，删除了重复的测试用例）

## 测试文件说明

### 1. test_call.cpp - 函数调用测试

测试函数调用的核心功能，包括基本函数调用、关联属性处理等：

#### BasicUsage - 基本使用
- 测试函数定义和调用
- 测试函数参数传递
- 测试函数表达式求值

#### GetRelatePropertiesBasic - 获取关联属性基本用法
- 测试从函数参数中提取关联属性（relate property）
- 测试关联属性的基本格式（ref 类型）
- 测试默认参数中的关联属性

#### GetRelatePropertiesWithInterface - 获取关联属性（带接口）
- 测试带接口限定符的关联属性
- 测试传统语法（无接口）和新语法（带接口）的混合使用
- 测试接口字段的处理

#### GetRelatePropertiesMixedSyntax - 获取关联属性（混合语法）
- 测试不同语法格式的关联属性混合使用
- 测试 ref、sync 等不同类型的属性
- 测试接口限定符的不同格式

#### IsRelatePropertyWithInterface - 判断关联属性（带接口）
- 测试判断参数是否为关联属性的功能
- 测试带接口和不带接口的关联属性判断
- 测试非关联属性的判断

#### GetRelatePropertiesNested - 获取关联属性（嵌套）
- 测试嵌套函数调用中的关联属性提取
- 测试多层嵌套场景
- 测试嵌套函数调用的参数传递

#### GetRelatePropertiesMixed - 获取关联属性（混合类型）
- 测试混合类型的关联属性（ref、sync 等）
- 测试不同对象和属性的组合
- 测试复杂场景下的属性提取

#### GetRelatePropertiesEmpty - 获取关联属性（空）
- 测试无关联属性的情况
- 测试空参数列表
- 测试空关联属性集合

#### IsRelatePropertyFunction - 判断关联属性函数
- 测试 `is_relate_property()` 函数的功能
- 测试各种输入类型的判断
- 测试边界情况

#### IsFunctionCallFunction - 判断函数调用函数
- 测试 `is_function_call()` 函数的功能
- 测试函数调用对象的识别
- 测试非函数调用对象的判断

#### ComplexNestedScenario - 复杂嵌套场景
- 测试复杂的嵌套函数调用场景
- 测试多层嵌套和混合参数类型
- 测试复杂表达式的求值

#### ErrorHandlingAndEdgeCases - 错误处理和边界情况
- 测试各种错误输入的处理
- 测试边界值和特殊值
- 测试异常情况的处理

### 2. test_parser.cpp - 函数解析测试

测试函数调用字符串的解析功能，包括属性解析、函数调用解析等：

#### PropertyTest - 属性解析测试
- **ParseBasicProperty**: 解析基本属性格式
- **ParseSyncProperty**: 解析同步属性（sync 类型）
- **ParseRefProperty**: 解析引用属性（ref 类型）
- **InvalidPropertyFormat**: 无效属性格式的错误处理

#### FunctionParserTest - 函数调用解析测试
- **ParseFunctionCallWithStringParam**: 解析字符串参数
- **ParseFunctionCallWithIntParam**: 解析整数参数
- **ParseFunctionCallWithDoubleParam**: 解析浮点数参数
- **ParseFunctionCallWithBoolParam**: 解析布尔值参数
- **ParseFunctionCallWithPropertyParam**: 解析属性参数
- **ParseFunctionCallWithNestedFunctionCall**: 解析嵌套函数调用
- **ParseFunctionCallWithMultipleParams**: 解析多参数函数调用
- **BasicFunctionCall**: 基本函数调用解析
- **ParseNestedFunctionCallWithIdentifiers**: 解析带标识符的嵌套函数调用
- **ParseFunctionCallWithSyncProperty**: 解析带同步属性的函数调用
- **ParseFunctionCallWithRefProperty**: 解析带引用属性的函数调用
- **ParseFunctionCallWithMultiplePropertyTypes**: 解析多种属性类型
- **ParseNestedFunctionCallWithProperties**: 解析带属性的嵌套函数调用
- **ParseFunctionCallWithMixedParameters**: 解析混合参数类型
- **ParseEdgeCases**: 解析边界情况
- **ParseParameterTypes**: 解析各种参数类型
- **ParseNestedDepth**: 解析嵌套深度
- **ParseSpecialCharacters**: 解析特殊字符
- **ParsePropertyWithInterface**: 解析带接口的属性
- **ParsePropertyWithInterfaceNoPrefix**: 解析带接口但无前缀的属性
- **ParseFunctionCallWithInterfaceParameters**: 解析带接口参数

#### RelatePropertyTest - 关联属性测试
- **VariantConversion**: variant 类型转换
- **EmptyVariantConversion**: 空 variant 转换
- **PartialVariantConversion**: 部分 variant 转换
- **InvalidVariantConversion**: 无效 variant 转换

#### PropertyParserTest - 属性解析器测试
- **ParseDifferentPropertyTypes**: 解析不同属性类型
- **ParseComplexPropertyNames**: 解析复杂属性名
- **ParseErrorCases**: 解析错误情况
- **ParseRefObject**: 解析引用对象
- **ParseRefObjectErrors**: 解析引用对象错误
- **ParseFunctionCallWithRefObjectAndRefProperty**: 解析带引用对象和引用属性的函数调用

#### RelateObjectTest - 关联对象测试
- **VariantConversion**: variant 类型转换
- **PartialVariantConversion**: 部分 variant 转换
- **InvalidVariantConversion**: 无效 variant 转换

## 测试覆盖范围

### 功能覆盖
- ✅ 函数定义和注册
- ✅ 函数调用执行
- ✅ 函数参数传递
- ✅ 关联属性（relate property）处理
- ✅ 函数调用字符串解析
- ✅ 属性字符串解析
- ✅ 嵌套函数调用
- ✅ 接口限定符支持

### 属性类型覆盖
- ✅ 基本属性（basic）
- ✅ 同步属性（sync）
- ✅ 引用属性（ref）
- ✅ 引用对象（ref object）

### 参数类型覆盖
- ✅ 字符串参数
- ✅ 整数参数
- ✅ 浮点数参数
- ✅ 布尔值参数
- ✅ 属性参数
- ✅ 函数调用参数
- ✅ 混合参数类型

### 边界情况覆盖
- ✅ 无参数函数调用
- ✅ 单参数函数调用
- ✅ 多参数函数调用
- ✅ 深层嵌套函数调用
- ✅ 空参数列表
- ✅ 特殊字符处理

### 错误处理覆盖
- ✅ 无效函数名
- ✅ 无效参数格式
- ✅ 无效属性格式
- ✅ 类型转换错误
- ✅ 嵌套深度限制

## 关键概念

### 关联属性 (Relate Property)

关联属性是一种特殊的函数参数类型，用于引用其他对象的属性。支持以下格式：

1. **传统语法**: `#/ObjectName.PropertyName`
2. **新语法（带接口）**: `#/ObjectName[InterfaceName].PropertyName`
3. **引用对象**: `#/ObjectName`

### 函数调用格式

函数调用支持以下格式：

1. **基本格式**: `FunctionName(param1, param2, ...)`
2. **嵌套调用**: `OuterFunction(InnerFunction(param))`
3. **属性参数**: `FunctionName(#/Object.Property)`
4. **混合参数**: `FunctionName('string', 42, #/Object.Property)`

### 接口限定符

接口限定符用于明确指定要访问的接口：

- **格式**: `[InterfaceName]`
- **位置**: 在对象名和属性名之间
- **示例**: `#/Device[bmc.dev.TestInterface].Temperature`

## 运行测试

### 使用 Meson 运行

```bash
# 在项目根目录下
cd builddir
meson test expr/function
```

### 运行特定测试文件

```bash
# 运行所有函数相关测试
meson test expr/function

# 运行特定测试用例
meson test expr/function --gtest_filter="FunctionCallTest.BasicUsage"
```

### 使用 GTest 直接运行

```bash
# 编译后直接运行测试程序
./builddir/tests/libmcpp_test --gtest_filter="FunctionCallTest.*"
./builddir/tests/libmcpp_test --gtest_filter="FunctionParserTest.*"
```

## 测试原则

1. **全面性**: 覆盖所有函数调用和解析场景
2. **类型安全**: 测试各种参数类型的处理
3. **嵌套支持**: 测试深层嵌套函数调用
4. **错误处理**: 测试各种错误输入的处理
5. **兼容性**: 测试新旧语法格式的兼容性

## 注意事项

1. 函数集合（`func_collection`）在测试前需要清理，使用 `func_collection::get_instance().clear()`
2. 关联属性的格式必须符合规范，否则解析会失败
3. 接口限定符是可选的，但使用后可以明确指定接口
4. 嵌套函数调用的深度可能有限制，取决于实现
5. 属性参数和函数调用参数可以混合使用

## 相关文档

- [表达式引擎模块文档](../../../src/expr/README.md)
- [表达式测试主文档](../README.md)
- [内置函数测试文档](../builtin/README.md)

