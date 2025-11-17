# Variant 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Variant 模块的测试用例。Variant 模块是 MC++ 库的核心数据类型模块，提供了动态类型系统，支持多种数据类型（null、整数、浮点数、布尔值、字符串、数组、对象、二进制数据、扩展类型等）的统一表示和操作。

## 测试文件列表

Variant 模块包含以下测试文件：

1. `test_variant_basic.cpp` - 基本功能测试
2. `test_variant_bool_operator.cpp` - 布尔操作符测试
3. `test_variant_comparison.cpp` - 比较操作符测试
4. `test_variant_containers.cpp` - 容器类型测试
5. `test_variant_deep_copy.cpp` - 深拷贝测试
6. `test_variant_exceptions.cpp` - 边缘情况、边界条件和异常处理测试
7. `test_variant_equality.cpp` - 相等性测试
8. `test_variant_extension.cpp` - 扩展类型测试
9. `test_variant_io.cpp` - 输入输出测试
10. `test_variant_operations.cpp` - 算术和位运算操作符测试
11. `test_variant_reference.cpp` - 引用类型测试
12. `test_variant_standard_containers.cpp` - 标准容器适配测试
13. `test_variant_visitor.cpp` - 访问者模式测试
14. `test_typed_variant.cpp` - 固定类型 variant 测试
15. `test_variant_conversion.cpp` - 转换与复制相关测试
16. `test_variant_base.cpp` - variant_base 核心 API 覆盖测试

## 详细测试用例

### test_variant_basic.cpp
基本功能测试，包括：
- 类型构造和转换
- 类型检查和查询
- 赋值操作
- 复制和移动操作
- 清除操作
- 带默认值的 as 方法

### test_variant_bool_operator.cpp
布尔操作符测试，包括：
- 各种类型的布尔转换
- 严格模式和宽松模式
- null、空容器、零值的布尔转换

### test_variant_comparison.cpp
比较操作符测试（27 个用例），包括：
- 基本类型比较（整数、浮点数、字符串等）
- 跨类型比较
- NaN 处理
- 数组和对象比较
- 边界值比较

### test_variant_containers.cpp
容器类型测试（涵盖 dict 与 variants 的 13 个用例），包括：
- 数组（variants）操作与容量管理
- 对象（dict）操作
- 嵌套容器
- 容器迭代、插入/删除与哈希计算

### test_variant_deep_copy.cpp
深拷贝测试（14 个用例），包括：
- 基本类型深拷贝
- 嵌套结构深拷贝
- 循环引用处理
- 相互引用处理
- 混合类型深拷贝

### test_variant_exceptions.cpp
边缘情况、边界条件和异常处理测试（57 个用例），包括：
- `SizeBlobType` - blob 类型的 size() 方法
- `OperatorIndexExtensionNoRefAccess` - extension 类型不支持引用访问时的 operator[]
- `OperatorIndexTypeError` - operator[] 类型错误
- `OperatorIndexConstExtension` - const operator[] 与 extension
- `GetWithDefaultValue` - get() 方法带默认值
- `ContainsNonObject` - contains() 在非对象类型时
- `OperatorEqualDictNonObject` - operator==(const dict&) 在非对象类型时
- `SetValueExtensionType` - set_value() 与 extension 类型
- `OperatorPlusStringBlob` - 字符串与 blob 相加
- `OperatorPlusBlobString` - blob 与字符串相加
- `OperatorPlusBlobInvalidType` - blob 与无效类型相加
- `OperatorPlusObjectInvalidType` - 对象与无效类型相加
- `OperatorMinusException` - 减法运算异常
- `OperatorMultiplyException` - 乘法运算异常
- `OperatorDivideException` - 除法运算异常
- `OperatorModuloException` - 取模运算异常
- `OperatorBitwiseAndException` - 位与运算异常
- `OperatorBitwiseOrException` - 位或运算异常
- `OperatorBitwiseXorException` - 位异或运算异常
- `ConstructorWithExtensionType` - extension 类型构造
- `MoveConstructorBlobType` - blob 类型移动构造
- `MoveConstructorExtensionType` - extension 类型移动构造
- `SetValueMoveSameType` - set_value() 移动相同类型
- `OperatorLeftShiftLargeAmount` - 左移大数值（>= 64）
- `OperatorRightShiftLargeAmount` - 右移大数值（>= 64）
- `OperatorRightShiftLargeAmountNegative` - 负数右移大数值
- `VisitVariousTypes` - visit() 各种类型
- `VisitExtensionNull` - visit() extension 为 null
- `AsUint64DefaultBranch` - as_uint64() default 分支
- `OperatorAssignNullptrChar` - operator=(const char*) 当参数为 nullptr
- `OperatorAssignStringViewToBlob` - operator=(std::string_view) 赋值给 blob
- `OperatorAssignBlobToBlob` - operator=(const blob&) 赋值给 blob
- `GetStringException` - get_string() 异常情况
- `GetBlobException` - get_blob() 异常情况
- `GetArrayException` - get_array() 异常情况
- `GetObjectException` - get_object() 异常情况
- `AsExtensionException` - as_extension() 异常情况
- `OperatorPlusNumericWithArray` - operator+(detail::numeric_t) 与数组
- `OperatorPlusNumericWithObject` - operator+(detail::numeric_t) 与对象（异常）
- `OperatorPlusNumericWithString` - operator+(detail::numeric_t) 与字符串
- `OperatorMinusNumericUnderflow` - operator-(detail::numeric_t) 下溢
- `OperatorDivideNumericByZero` - operator/(detail::numeric_t) 除零
- `OperatorModuloNumericByZero` - operator%(detail::numeric_t) 除零
- `OperatorEqualWithVariants` - operator==(const variants&) 数组比较
- `OperatorLessWithVariants` - operator<(const variants&) 数组比较
- `OperatorGreaterWithVariants` - operator>(const variants&) 数组比较
- `OperatorLessNumericWithNaN` - operator<(detail::numeric_t) 与 NaN
- `OperatorGreaterNumericWithNaN` - operator>(detail::numeric_t) 与 NaN
- `OperatorLessEqualNumericWithNaN` - operator<=(detail::numeric_t) 与 NaN
- `OperatorGreaterEqualNumericWithNaN` - operator>=(detail::numeric_t) 与 NaN
- `OperatorLessNumericWithString` - operator<(detail::numeric_t) 与字符串
- `OperatorGreaterNumericWithString` - operator>(detail::numeric_t) 与字符串
- `OperatorLessNumericWithBlob` - operator<(detail::numeric_t) 与 blob
- `OperatorGreaterNumericWithBlob` - operator>(detail::numeric_t) 与 blob
- `OperatorEqualNumericWithVariousTypes` - operator==(detail::numeric_t) 各种类型
- `OperatorLessVariantsException` - operator<(const variants&) 异常情况
- `OperatorGreaterVariantsException` - operator>(const variants&) 异常情况

### test_variant_equality.cpp
相等性测试（5 个用例），包括：
- 相同类型相等性
- 跨类型相等性
- 浮点数相等性（考虑精度）
- 数组和对象相等性

### test_variant_extension.cpp
扩展类型测试（23 个用例），包括：
- 扩展类型构造与基本操作
- 零开销引用访问验证
- 扩展类型拷贝和移动
- 扩展类型哈希与链式访问
- 扩展类型相等性验证（覆盖 equals 覆写路径）

### test_variant_io.cpp
输入输出测试（6 个用例），包括：
- 流输出
- JSON 序列化
- JSON 反序列化
- 格式化输出

### test_variant_operations.cpp
算术和位运算操作符测试，包括：
- 整数运算（加法、减法、乘法、除法、取模）
- 浮点数运算
- 字符串连接
- 数组拼接
- 对象合并
- blob 操作
- 位运算（与、或、异或、左移、右移）
- 复合赋值运算
- 自增自减运算
- 一元运算（正负号、逻辑非）

### test_variant_reference.cpp
引用类型测试（44 个用例），包括：
- variant_reference 构造和访问
- 引用赋值
- 引用比较
- 引用操作符（[]、*、->）
- 复合赋值操作符
- 自增自减操作符
- 一元操作符
- 类型转换
- extension 访问（缓存与零开销场景）

### test_variant_standard_containers.cpp
标准容器适配测试（11 个用例），包括：
- std::vector 适配
- std::map 适配
- std::unordered_map 适配
- 容器序列化和反序列化

### test_variant_visitor.cpp
访问者模式测试（20 个用例），包括：
- visit() 方法各种类型
- visit_with() 模板方法
- 自定义访问者实现
- 访问者返回值处理

### test_typed_variant.cpp
固定类型 variant 测试（13 个用例），包括：
- 固定类型锁定
- 类型不匹配异常
- 各种类型的固定 variant
- 边界值测试

## 运行测试

### 运行所有 variant 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

**注意**: 
- meson 构建配置中只有一个测试注册（`libmcpp unit tests`），不能使用 `--suite variant` 运行特定模块测试
- 要通过 meson 传递参数给测试可执行文件，使用 `--test-args` 选项
- 推荐直接运行可执行文件，更简单直接

### test_variant_conversion.cpp
转换与复制相关测试，包括：
- `CopyOnVariousTypes` - variant_base::copy() 针对数字、字符串、数组、对象的等价性
- `CopyBlobShouldBeEqual` - blob 类型的 copy() 等价性
- `ToVariantOverloads` - to_variant 重载：bool、const char*、char*、variants、nullptr
- `FromVariantOverloads` - from_variant 重载：const char*&、char*&、bool&、variants&、dict&
- `StreamOutputFormatting` - operator<< 针对各种类型的流输出格式化

### test_variant_base.cpp
variant_base 核心 API 场景测试（精简重复后保留关键覆盖），包括：
- 对象、array 与 extension 的引用访问、get/contains、clear 等组合验证
- set_value（含移动分支）、set_fixed_type、swap 与 visit/visit_with 的状态管理行为
- 数值/字符串/布尔/blob 的转换路径以及 detail::numeric_t 运算、比较
- copy/deep_copy/move、hash 计算和 get_type_name 等基础能力
- to_variant/from_variant 辅助函数及 variants/dict 互操作场景
- 补充覆盖 extension 空指针、array 越界、dict 等场景的异常路径

> 未覆盖说明：`variant_base::set_value` 内部的 `throw_unknow_type_error` 分支依赖非法类型编号，
> 正常 API 无法触发，故仅在 README 备注，未通过测试构造覆盖。

### 运行特定测试文件

```bash
# 测试基本功能（所有用例）- 直接运行可执行文件（推荐）
./builddir/tests/libmcpp_test --gtest_filter="VariantBasicTest.*"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=VariantBasicTest.*"

# 测试异常/边界情况（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="VariantExceptionsTest.*"
meson test -C builddir --test-args="--gtest_filter=VariantExceptionsTest.*"

# 测试转换与复制（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="VariantConversionTest.*"
meson test -C builddir --test-args="--gtest_filter=VariantConversionTest.*"

# 测试操作符（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="VariantOperationsTest.*"
meson test -C builddir --test-args="--gtest_filter=VariantOperationsTest.*"

# 测试引用类型（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="VariantReferenceTest.*"
meson test -C builddir --test-args="--gtest_filter=VariantReferenceTest.*"

# 测试访问者模式（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="VariantVisitorTest.*"
meson test -C builddir --test-args="--gtest_filter=VariantVisitorTest.*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例（直接运行可执行文件，推荐）
./builddir/tests/libmcpp_test --gtest_filter="VariantEdgeCasesTest.OperatorAssignNullptrChar"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=VariantEdgeCasesTest.OperatorAssignNullptrChar"

# 运行多个测试用例（使用通配符）
./builddir/tests/libmcpp_test --gtest_filter="VariantEdgeCasesTest.Operator*"
meson test -C builddir --test-args="--gtest_filter=VariantEdgeCasesTest.Operator*"
```

## 测试统计

- **测试文件总数**: 15 个
- **测试用例总数**: 约 276 个
- **测试覆盖的功能模块**:
  - Variant Base（基础类型系统）
  - Variant Operations（算术和位运算）
  - Variant Comparison（比较操作）
  - Variant Reference（引用类型）
  - Variant Extension（扩展类型）
  - Variant Visitor（访问者模式）
  - Typed Variant（固定类型 variant）
  - Deep Copy（深拷贝）
  - IO Operations（输入输出）

## 代码覆盖率

当前测试覆盖率（持续更新中）：
- **Line Coverage**: 持续提升中
- **Branch Coverage**: 持续提升中
- **Function Coverage**: 持续提升中

测试用例持续增加，特别是 `test_variant_exceptions.cpp` 文件，专门用于覆盖边缘情况、边界条件和异常处理，以提高代码覆盖率。

### 调试测试

```bash
# 使用 gdb 调试测试
gdb --args ./builddir/tests/libmcpp_test --gtest_filter="VariantEdgeCasesTest.OperatorAssignNullptrChar"

# 启用详细输出（直接运行可执行文件）
./builddir/tests/libmcpp_test --gtest_filter="VariantEdgeCasesTest.*" --verbose

# 或通过 meson 启用详细输出
meson test -C builddir --verbose --test-args="--gtest_filter=VariantEdgeCasesTest.*"
```

## 测试注意事项

1. **meson test 命令格式**: meson 构建配置中只有一个测试注册（`libmcpp unit tests`），要传递参数给测试可执行文件，使用 `--test-args` 选项。格式：`meson test -C builddir --test-args="--gtest_filter=..."`。注意不要在 `--test-args` 中使用引号包裹整个参数，gtest_filter 的值可以用引号。
2. **直接运行可执行文件**: 推荐直接运行 `./builddir/tests/libmcpp_test`，这样更简单直接，不需要通过 meson。
3. **类型转换**: variant 支持灵活的类型转换，测试用例覆盖了各种类型转换场景，包括成功和失败的情况。
4. **异常处理**: variant 在类型不匹配或操作不支持时会抛出异常，测试用例验证了各种异常情况。
5. **深拷贝**: variant 支持深拷贝，可以处理嵌套结构和循环引用，测试用例验证了这些复杂场景。
6. **扩展类型**: variant 支持自定义扩展类型，测试用例展示了如何实现和使用扩展类型。
7. **访问者模式**: variant 实现了访问者模式，支持类型安全的值访问，测试用例覆盖了各种访问场景。
8. **固定类型 variant**: typed_variant 确保类型不会被改变，测试用例验证了类型锁定机制。
9. **边界值测试**: `test_variant_exceptions.cpp` 专门测试边界条件和边缘情况，包括 NaN、nullptr、空容器、大数值等。
10. **数值运算**: variant 支持各种数值运算，包括整数、浮点数、字符串、数组、对象等类型的运算，测试用例覆盖了各种运算场景和异常情况。
11. **比较操作**: variant 支持跨类型比较，包括数值比较、字符串比较、数组比较等，测试用例验证了各种比较场景。
12. **引用类型**: variant_reference 提供了统一的引用访问接口，支持数组、对象、扩展类型的统一访问，测试用例验证了引用操作的各种场景。

