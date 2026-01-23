# Dict 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Dict 模块的测试用例。Dict 模块提供了字典（dict）和可变字典（mutable_dict）类型，支持键值对存储、查找、迭代、深拷贝、浅拷贝、数据共享等功能。Dict 使用共享指针实现，支持高效的复制和共享语义。

## 测试文件列表

Dict 模块包含以下测试文件：

1. `test_dict_construction.cpp` - 字典构造和赋值测试
2. `test_dict_operations.cpp` - 字典操作测试
3. `test_dict_iteration.cpp` - 字典迭代测试
4. `test_dict_conversion.cpp` - 字典转换测试
5. `test_dict_string_view.cpp` - string_view 支持测试
6. `test_dict_performance.cpp` - 性能测试（DISABLED）

## 详细测试用例

### test_dict_construction.cpp

字典构造和赋值测试（17 个用例），包括：

#### dict 构造测试
- `DefaultConstructor` - 默认构造函数
- `VectorEntryConstructor` - 向量条目构造函数
- `InitializerListConstructor` - 初始化列表构造函数
- `CopyConstructor` - 复制构造函数
- `MoveConstructor` - 移动构造函数
- `AssignmentOperator` - 赋值操作符
- `MoveAssignmentOperator` - 移动赋值操作符

#### mutable_dict 构造测试
- `MutableDictDefaultConstructor` - mutable_dict 默认构造函数
- `MutableDictVectorEntryConstructor` - mutable_dict 向量条目构造函数
- `MutableDictInitializerListConstructor` - mutable_dict 初始化列表构造函数
- `MutableDictFromDictConstructor` - 从 dict 构造 mutable_dict
- `MutableDictCopyConstructor` - mutable_dict 复制构造函数
- `MutableDictMoveConstructor` - mutable_dict 移动构造函数
- `MutableDictAssignmentOperator` - mutable_dict 赋值操作符
- `MutableDictMoveAssignmentOperator` - mutable_dict 移动赋值操作符
- `MutableDictAssignmentFromDict` - 从 dict 赋值给 mutable_dict
- `MutableDictChainedConstruction` - mutable_dict 链式构造

### test_dict_operations.cpp

字典操作测试（13 个用例），包括：

#### dict 基本操作
- `DictBasicAccess` - dict 基本访问操作
  - operator[] 访问
  - at() 方法
  - get() 方法（带默认值）
  - contains() 方法

- `DictIterators` - dict 迭代器操作
  - 前向迭代器
  - 反向迭代器
  - 迭代器遍历

- `DictFind` - dict 查找操作（已合并 `DictFindEntryWithStringKey` 和 `DictFindWithStringKey`）
  - find() 方法（支持 std::string、std::string_view、const char* 多种键类型）
  - 查找存在的键
  - 查找不存在的键

- `DictKeysAndValues` - dict 键和值获取
  - keys() 方法
  - values() 方法

- `DictComparison` - dict 比较操作
  - operator==
  - operator!=
  - 嵌套 dict 比较

#### mutable_dict 基本操作
- `MutableDictBasicModification` - mutable_dict 基本修改操作
  - operator[] 修改和添加
  - operator() 修改和添加
  - 链式调用 operator()
  - 特殊值识别测试（0 被识别为整数而不是 null，nullptr 被识别为 null）
  - 整数类型 0 的正确识别
  - 浮点数 0.0 的正确识别
  - bool 类型的正确识别

- `MutableDictEraseAndClear` - mutable_dict 删除和清空
  - erase() 方法
  - clear() 方法

- `MutableDictIterators` - mutable_dict 迭代器操作
  - 前向迭代器
  - 反向迭代器
  - 迭代过程中修改

- `MutableDictAt` - mutable_dict at() 方法
  - at() 方法访问
  - at() 方法修改
  - 越界异常

#### 数据共享和拷贝测试
- `DataSharing` - 数据共享测试
  - dict 复制共享数据
  - 修改共享数据的影响

- `ShallowCopy` - 浅拷贝测试
  - 浅拷贝行为
  - 共享数据修改

- `DeepCopy` - 深拷贝测试
  - 深拷贝行为
  - 独立数据修改
  - 嵌套结构深拷贝

- `DeepCopyCycleHandling` - 深拷贝循环引用处理
  - 循环引用检测
  - 循环引用处理
  - 相互引用处理

#### mutable_dict 高级操作
- `MutableDictFind` - mutable_dict 查找操作（已合并 `MutableDictFindEntryWithStringKey`）
  - find() 方法（支持 std::string、std::string_view、const char* 多种键类型）
  - 查找和修改
  - const 和 non-const 版本

- `MutableDictInsert` - mutable_dict 插入操作
  - insert() 方法
  - 插入新键
  - 更新现有键

- `MutableDictInsertInteraction` - mutable_dict 插入交互测试
  - 插入操作与迭代器交互
  - 插入操作与查找交互

#### 边界情况和异常处理测试
- `DictFindEntryWithNullPointerKey` - 测试 find_entry(nullptr) 异常
- `DictOperatorBracketWithNullPointerKey` - 测试 operator[](nullptr) 异常
- `DictOperatorBracketWithVariantKeyNotFound` - 测试 operator[](variant) 键不存在异常
- `DictGetWithStringKey` - 测试 get(const std::string&, const variant&)
- `DictGetWithNullPointerKey` - 测试 get(nullptr, default_value) 异常
- `DictGetWithVariantKey` - 测试 get(const variant&, const variant&)
- `DictAtIndexOutOfRangeWithEmptyDict` - 测试空字典的 at_index 异常
- `DictAtWithStringKey` - 测试 at(const std::string&)
- `DictAtWithNonExistentKey` - 测试 at 键不存在异常
- `DictAtWithNullPointerKey` - 测试 at(nullptr) 异常
- `DictAtWithVariantKey` - 测试 at(const variant&)
- `DictFindIndexWithNullPointerKey` - 测试 find_index(nullptr) 异常
- `DictFindIndexWithVariantKey` - 测试 find_index(const variant&)
- `DictContainsWithNullPointerKey` - 测试 contains(nullptr) 异常
- `DictHash` - 测试 hash() 方法
- `MutableDictFindEntryWithNullPointerKey` - 测试非 const find_entry(nullptr) 异常
- `MutableDictOperatorBracketWithNullPointerKey` - 测试非 const operator[](nullptr) 异常
- `MutableDictOperatorBracketWithNullVariantKey` - 测试非 const operator[](null variant) 异常
- `MutableDictEraseWithVariantKey` - 测试 erase(const variant&)
- `MutableDictAtWithStringKey` - 测试非 const at(const std::string&)
- `MutableDictAtWithNullPointerKey` - 测试非 const at(nullptr) 异常
- `MutableDictAtWithVariantKey` - 测试非 const at(const variant&)
- `MutableDictInsertWithHint` - 测试 insert(const_iterator hint, variant key, variant value)

### test_dict_iteration.cpp

字典迭代测试（6 个用例），包括：

- `CorrectIteration` - 正确迭代测试
  - 使用迭代器遍历
  - 迭代顺序验证

- `StructuredBindingNotSupported` - 结构化绑定不支持测试
  - 验证不支持结构化绑定
  - 使用迭代器替代

- `MutableDictIteration` - mutable_dict 迭代测试
  - mutable_dict 迭代器
  - 迭代过程中修改

- `ConstDictIteration` - const dict 迭代测试
  - const dict 迭代器
  - 只读迭代

- `ComplexObjectIteration` - 复杂对象迭代测试
  - 嵌套 dict 迭代
  - 复杂值类型迭代

- `EmptyDictIteration` - 空字典迭代测试
  - 空 dict 迭代
  - 空 mutable_dict 迭代

- `IteratorPerformance` - 迭代器性能测试
  - 迭代器性能基准
  - 与索引访问性能对比

- `IterationWithLookup` - 迭代与查找结合测试
  - 迭代过程中查找
  - 查找与迭代交互

- `DictFindWithNullPointerKey` - 测试 find(nullptr) 异常
- `DictFindWithVariantKey` - 测试 find(const variant&)
- `MutableDictFindWithNullPointerKey` - 测试非 const find(nullptr) 异常
- `MutableDictFindWithVariantKey` - 测试非 const find(const variant&)
- `MutableDictFindWithNonExistentKey` - 测试非 const find 键不存在返回 end()

### test_dict_conversion.cpp

字典转换测试（9 个用例），包括：

- `DictToVariant` - dict 转 variant
- `MutableDictToVariant` - mutable_dict 转 variant
- `VariantToDict` - variant 转 dict
- `VariantToMutableDict` - variant 转 mutable_dict
- `StdMapToDict` - std::map 转 dict
- `StdUnorderedMapToDict` - std::unordered_map 转 dict
- `DictToStdMap` - dict 转 std::map
- `DictToStdUnorderedMap` - dict 转 std::unordered_map
- `NestedDictConversion` - 嵌套 dict 转换

### test_dict_string_view.cpp

string_view 支持测试（5 个用例），包括：

- `DictStringViewSupport` - dict 的 string_view 支持
  - 使用 string_view 作为键
  - string_view 键的查找和访问

- `DictConstCharSupport` - dict 的 const char* 支持
  - 使用 const char* 作为键
  - const char* 键的查找和访问

- `MutableDictStringViewSupport` - mutable_dict 的 string_view 支持
  - 使用 string_view 作为键
  - string_view 键的修改

- `MutableDictConstCharSupport` - mutable_dict 的 const char* 支持
  - 使用 const char* 作为键
  - const char* 键的修改

- `MixedStringTypes` - 混合字符串类型测试
  - string、string_view、const char* 混合使用
  - 类型转换和兼容性

### test_dict_performance.cpp

性能测试（5 个用例，DISABLED），包括：

- `DISABLED_InsertionPerformance` - 插入性能测试
- `DISABLED_LookupPerformance` - 查找性能测试
- `DISABLED_IterationPerformance` - 迭代性能测试
- `DISABLED_ErasurePerformance` - 删除性能测试
- `DISABLED_IteratorVsIndexPerformance` - 迭代器与索引性能对比

**注意**: 性能测试默认被禁用，需要手动启用才能运行。

## 测试统计

- **测试文件总数**: 6
- **测试用例总数**: 55
  - `test_dict_construction.cpp`: 17 个用例
  - `test_dict_operations.cpp`: 13 个用例（已删除重复的 `DictFindEntryWithStringKey` 和 `MutableDictFindEntryWithStringKey`，功能已合并到 `DictFind` 和 `MutableDictFind`；已删除 `MutableDictTestNullValue`，功能已融合到 `MutableDictBasicModification`）
  - `test_dict_iteration.cpp`: 6 个用例（已删除重复的 `DictFindWithStringKey` 和 `MutableDictFindWithStringKey`，功能已合并到 `DictOperationsTest.DictFind` 和 `DictOperationsTest.MutableDictFind`）
  - `test_dict_conversion.cpp`: 9 个用例
  - `test_dict_string_view.cpp`: 5 个用例
  - `test_dict_performance.cpp`: 5 个用例（DISABLED）

## 运行测试

### 运行所有测试

```bash
cd builddir
ninja test
```

### 运行特定测试文件

```bash
# 运行 dict 构造测试
./tests/libmcpp_test --gtest_filter="DictConstructionTest.*"

# 运行 dict 操作测试
./tests/libmcpp_test --gtest_filter="DictOperationsTest.*"

# 运行 dict 迭代测试
./tests/libmcpp_test --gtest_filter="DictIterationTest.*"

# 运行 dict 转换测试
./tests/libmcpp_test --gtest_filter="DictConversionTest.*"

# 运行 dict string_view 测试
./tests/libmcpp_test --gtest_filter="DictStringViewTest.*"
```

### 运行特定测试用例

```bash
# 运行特定测试用例
./tests/libmcpp_test --gtest_filter="DictOperationsTest.DictBasicAccess"
./tests/libmcpp_test --gtest_filter="DictOperationsTest.DeepCopy"
./tests/libmcpp_test --gtest_filter="DictIterationTest.CorrectIteration"
```

### 运行性能测试（需要手动启用）

性能测试默认被禁用，如需运行性能测试，需要：

1. 修改 `test_dict_performance.cpp`，移除 `DISABLED_` 前缀
2. 重新编译
3. 运行测试

```bash
# 运行性能测试（启用后）
./tests/libmcpp_test --gtest_filter="DictPerformanceTest.*"
```

## 测试注意事项

1. **数据共享**: dict 使用共享指针实现，复制操作会共享底层数据，修改共享数据会影响所有引用
2. **深拷贝**: 使用 `deep_copy()` 方法创建独立的数据副本，修改深拷贝不会影响原始数据
3. **循环引用**: 深拷贝会检测并处理循环引用，避免无限递归
4. **string_view 生命周期**: 使用 `string_view` 作为键时，需要确保底层字符串的生命周期足够长
5. **迭代器稳定性**: 在迭代过程中修改 mutable_dict 可能会使迭代器失效，需要注意
6. **性能测试**: 性能测试默认被禁用，仅在需要性能分析时启用

