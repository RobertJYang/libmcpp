# JSON 包装器测试 (json_wrapper)

## 概述

本测试文件包含 JSON 包装器模块的单元测试，覆盖了 `JsonValue`、`JsonArray`、`JsonObject` 等核心类的功能，包括创建、类型判断、值读取、与 variant 互转、相等比较等。

## 测试文件

- `test_json_wrapper.cpp`: JSON 包装器完整功能测试

**总测试用例数**: 65+ 个

## 测试文件说明

### 1. test_json_wrapper.cpp - JSON 包装器测试

测试 JSON 包装器的核心功能，包括基本操作、类型转换、variant 互转、复杂场景等：

#### JsonValueCreateTest - JSON 值创建测试

测试不同类型 JSON 值的创建：

- **create_null**: 创建 null 值
- **create_bool**: 创建布尔值（true/false）
- **create_int**: 创建整数
- **create_double**: 创建浮点数
- **create_string**: 创建字符串
- **create_string_empty**: 创建空字符串
- **create_array**: 创建空数组
- **create_object**: 创建空对象

#### JsonValueCopyTest - JSON 值拷贝测试

测试 JSON 值的拷贝语义和移动语义：

- **copy_constructor**: 拷贝构造函数
- **copy_assignment**: 拷贝赋值运算符
- **move_constructor**: 移动构造函数
- **move_assignment**: 移动赋值运算符

#### JsonValueTypeTest - JSON 值类型测试

测试类型判断和类型转换：

- **type_check**: 类型判断（is_null, is_bool, is_int 等）
- **invalid_type_cast**: 无效类型转换（应该抛出异常）

#### JsonValueVariantTest - JSON 值与 variant 互转测试

测试 `JsonValue` 与 `mc::variant` 的相互转换：

- **to_variant_basic_types**: 基本类型转 variant
- **to_variant_array**: 数组转 variant
- **to_variant_object**: 对象转 variant
- **from_variant_basic_types**: variant 转基本类型
- **from_variant_array**: variant 转数组
- **from_variant_object**: variant 转对象
- **round_trip**: 往返转换（variant -> JsonValue -> variant）

#### JsonValueEqualTest - JSON 值相等比较测试

测试 JSON 值的相等比较（按内容比较）：

- **equal_basic_types**: 基本类型相等比较
- **not_equal_basic_types**: 基本类型不等比较
- **equal_array**: 数组相等比较
- **not_equal_array**: 数组不等比较
- **equal_object**: 对象相等比较
- **not_equal_object**: 对象不等比较

#### JsonArrayTest - JSON 数组测试

测试 `JsonArray` 的功能：

- **empty_array**: 空数组操作
- **push_back**: 添加元素
- **at_access**: 通过 at() 访问元素
- **operator_access**: 通过 [] 访问元素
- **set_element**: 设置元素值
- **iteration**: 迭代器遍历
- **copy_semantics**: 拷贝语义（引用计数）

#### JsonObjectTest - JSON 对象测试

测试 `JsonObject` 的功能：

- **empty_object**: 空对象操作
- **set_and_get**: 设置和获取键值对
- **operator_access**: 通过 [] 访问键值对
- **erase**: 删除键值对
- **size**: 获取对象大小
- **iteration**: 迭代器遍历
- **copy_semantics**: 拷贝语义（引用计数）

#### JsonComplexTest - 复杂结构测试

测试嵌套和复杂的 JSON 结构：

- **nested_structure**: 嵌套结构（对象中包含数组，数组中包含对象）
- **modify_existing_structure**: 修改现有结构
- **variant_round_trip_complex**: 复杂结构的 variant 往返转换
- **mixed_type_array_output**: 混合类型数组

#### JsonRealWorldTest - 真实场景测试

测试真实使用场景：

- **build_user_list**: 构建用户列表 JSON（演示实际 API 使用）
- **build_and_modify**: 构建并修改 JSON 结构
- **generic_json_processing**: 通用 JSON 处理（遍历和处理各种类型）
- **type_safe_access**: 类型安全访问
- **unified_interface_with_json_value**: 统一接口使用 JsonValue

#### JsonRefCountTest - 引用计数测试

测试引用计数的正确性，确保内存管理正确：

- **basic_reference_counting**: 基本引用计数（JsonValue、JsonArray、JsonObject 的引用计数管理）
- **container_reference_counting**: 容器中的引用计数（数组和对象作为容器时的引用计数）
- **object_child_reference_counting**: 对象包含子对象时的引用计数（测试对象中通过 Quote 引用子对象时的引用计数变化）
- **array_child_reference_counting**: 数组包含子对象时的引用计数（测试数组中通过 Quote 引用子对象时的引用计数变化）
- **circular_reference_counting**: 循环引用时的引用计数（测试两个对象互相引用时的引用计数管理）
- **array_circular_reference_counting**: 数组循环引用时的引用计数（测试两个数组互相引用时的引用计数管理）
- **nested_object_reference_counting**: 多层嵌套对象的引用计数（测试三层或更多层嵌套对象时的引用计数管理）

#### JsonQuoteTest - Quote 类型测试

测试 Quote 类型的解引用和替换：

- **quote_dereference_and_replacement**: Quote 解引用和替换

#### JsonValueCorrectnessTest - 值正确性测试

测试值访问的正确性：

- **access_correctness**: 访问正确性（确保访问的是实际值而不是 Quote）

#### JsonMemorySafetyTest - 内存安全测试

测试内存管理和生命周期：

- **lifecycle_management**: 生命周期管理
- **iterator_and_move_safety**: 迭代器和移动语义的安全性

#### JsonDoubleFreeTest - 双重释放测试

测试移动语义，确保不会双重释放：

- **move_safety**: 移动安全性

#### JsonRefChainTest - 引用链测试

测试引用链的完整性：

- **reference_chain**: 引用链（确保引用链中的对象不会被提前释放）

#### JsonStressTest - 压力测试

综合压力测试：

- **comprehensive_stress**: 综合压力测试（大量数据、复杂结构、频繁操作）

## 测试覆盖范围

### 功能覆盖

- ✅ JSON 值创建（null、bool、int、double、string、array、object）
- ✅ 类型判断（is_null、is_bool、is_int 等）
- ✅ 值读取（as_bool、as_int、as_double 等）
- ✅ 拷贝和移动语义
- ✅ 数组操作（push_back、at、set、迭代）
- ✅ 对象操作（set、get、erase、迭代）
- ✅ 与 variant 相互转换
- ✅ 相等比较（按内容比较）
- ✅ 引用计数管理
- ✅ Quote 类型处理

### 类型覆盖

- ✅ null
- ✅ bool（true/false）
- ✅ int（整数）
- ✅ double（浮点数）
- ✅ string（字符串、空字符串）
- ✅ array（空数组、包含元素的数组）
- ✅ object（空对象、包含键值对的对象）

### 场景覆盖

- ✅ 基本操作（创建、读取、修改）
- ✅ 嵌套结构（对象中包含数组、数组中包含对象）
- ✅ 复杂结构（多层嵌套、混合类型）
- ✅ variant 互转（往返转换、复杂结构转换）
- ✅ 引用计数（基本计数、容器中的计数、子对象引用、循环引用、嵌套对象引用）
- ✅ 内存安全（生命周期、移动语义、迭代器安全）

### 错误处理覆盖

- ✅ 无效类型转换（抛出 bad_cast_exception）
- ✅ 访问越界（数组索引越界）
- ✅ 空值访问（空对象、空数组）

### 边界情况覆盖

- ✅ 空值（null）
- ✅ 空字符串
- ✅ 空数组
- ✅ 空对象
- ✅ 大整数
- ✅ 浮点数精度
- ✅ 深层嵌套结构

## 关键概念

### 引用计数

`JsonValue`、`JsonArray`、`JsonObject` 使用引用计数管理底层 JSON 对象的生命周期。拷贝是浅拷贝，多个实例共享同一个底层对象。当最后一个实例被销毁时，底层对象才会被释放。

### Quote 类型

Quote 类型是底层 JSON 库中的一种特殊类型，用于在数组或对象中引用另一个 JSON 对象。`json_wrapper` 会自动处理 Quote 类型的解引用，确保返回的是实际值。

### 与 variant 互转

`JsonValue` 可以与 `mc::variant` 相互转换：

- `to_variant()`: 将 `JsonValue` 转换为 `mc::variant`
- `from_variant()`: 将 `mc::variant` 转换为 `JsonValue`

这种转换是递归的，支持嵌套结构。

### 相等比较

`JsonValue` 的 `operator==` 进行的是内容比较，而不是指针比较。即使两个 `JsonValue` 指向不同的底层 JSON 对象，只要内容相同，`operator==` 就会返回 `true`。

## 运行测试

### 使用 Meson 运行

```bash
# 在项目根目录下
cd builddir
meson test json_wrapper
```

### 运行特定测试用例

```bash
# 运行所有 JSON 包装器测试
meson test json_wrapper

# 运行特定测试用例
meson test json_wrapper --gtest_filter="JsonValueCreateTest.*"
meson test json_wrapper --gtest_filter="JsonArrayTest.*"
meson test json_wrapper --gtest_filter="JsonObjectTest.*"
```

### 使用 GTest 直接运行

```bash
# 编译后直接运行测试程序
./builddir/tests/libmcpp_test --gtest_filter="JsonValueCreateTest.*"
./builddir/tests/libmcpp_test --gtest_filter="JsonArrayTest.*"
./builddir/tests/libmcpp_test --gtest_filter="JsonObjectTest.*"
```

## 测试原则

1. **全面性**: 覆盖所有 API 接口和常用场景
2. **类型安全**: 测试各种类型的创建、判断、转换
3. **内存安全**: 测试引用计数、生命周期管理、移动语义
4. **错误处理**: 测试各种错误输入的处理和异常抛出
5. **实际场景**: 测试真实使用场景和复杂结构

## 注意事项

1. JSON 包装器使用引用计数，拷贝是浅拷贝，修改会影响所有共享实例
2. 类型转换失败时会抛出 `mc::bad_cast_exception`，测试中需要验证异常抛出
3. 数组索引越界时会抛出 `mc::out_of_range_exception`
4. Quote 类型会自动解引用，测试中需要验证返回的是实际值
5. 与 variant 的转换是递归的，支持嵌套结构

## 相关文档

- [JSON 包装器 API 文档](../../docs/api/json_wrapper.md) - JSON 包装器的使用说明
- [mc::variant 文档](../../docs/api/variant.md) - variant 类型系统
