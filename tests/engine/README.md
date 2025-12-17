# Engine 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Engine 模块的测试用例。Engine 模块是 MC++ 库的核心引擎模块，提供了对象管理、接口系统、属性系统、路径解析、错误处理、上下文管理、服务工具、微组件、执行结果等核心功能。

## 测试文件列表

Engine 模块包含以下测试文件：

1. `test_engine.cpp` - 引擎核心功能测试
2. `test_object.cpp` - 对象管理测试
3. `test_interface.cpp` - 接口系统测试
4. `test_interface_inherit.cpp` - 接口继承测试
5. `test_std_interface.cpp` - 标准 D-Bus 接口测试
6. `test_property.cpp` - 属性系统测试
7. `test_property_processors.cpp` - 属性处理器测试
8. `test_property_concurrent_fix.cpp` - 属性并发修复测试
9. `test_context.cpp` - 上下文管理测试
10. `test_path.cpp` - 路径模型测试
11. `test_path_iterator.cpp` - 路径迭代器测试
12. `test_service_utils.cpp` - 服务工具函数测试
13. `test_error_engine.cpp` - 错误引擎测试
14. `test_result.cpp` - 执行结果测试
15. `test_async_invoke.cpp` - 异步调用测试
16. `test_micro_component.cpp` - 微组件测试
17. `test_utils.cpp` - 工具函数测试

## 详细测试用例

### test_engine.cpp
引擎核心功能测试（14 个用例）：
- `test_engine_dbus_connection` - 测试引擎 D-Bus 连接
- `test_object_property_changed_sig` - 测试对象属性变化信号
- `test_interface_property_changed_sig` - 测试接口属性变化信号
- `test_property_changed_sig` - 测试属性变化信号
- `test_property_changed_sig_use_abstract_object` - 测试使用抽象对象的属性变化信号
- `test_object_reflect` - 测试对象反射
- `test_managed_object` - 测试托管对象
- `test_managed_object_comprehensive` - 测试托管对象综合场景
- `InitInvalidBusName` - 测试无效总线名称初始化
- `ServiceLifecycleHooks` - 测试服务生命周期钩子
- `ServiceTimeoutCalls` - 测试服务超时调用
- `ServiceMatchManagement` - 测试服务匹配规则管理
- `test_object_rewite_method` - 测试对象方法重写

### test_object.cpp
对象管理测试（10 个用例）：
- `test_object_metadata` - 测试对象元数据
- `test_get_interface` - 测试获取接口
- `test_property` - 测试属性访问
- `test_method` - 测试方法调用
- `test_signal_connect` - 测试信号连接
- `test_signal_emit` - 测试信号发射
- `test_connect_by_interface_name` - 测试通过接口名连接
- `test_multiple_connections` - 测试多个连接
- `test_invalid_signal` - 测试无效信号
- `test_has_interface` - 测试接口存在性检查

### test_interface.cpp
接口系统测试（14 个用例）：
- `test_interface` - 测试接口基础功能
- `test_method_invoke` - 测试方法调用
- `test_property` - 测试属性访问
- `test_signal_connection` - 测试信号连接
- `test_multiple_signals` - 测试多个信号
- `test_emit_signal` - 测试信号发射
- `test_signals_with_and_without_return` - 测试有返回值和无返回值信号
- `test_signal_chain` - 测试信号链
- `test_invalid_arguments` - 测试无效参数
- `test_multiple_slots` - 测试多个槽函数
- `test_connection_management` - 测试连接管理
- `test_interface_hierarchy` - 测试接口层次结构
- `test_empty_signal` - 测试空信号
- `test_complex_scenario` - 测试复杂场景

### test_interface_inherit.cpp
接口继承测试（11 个用例）：
- `test_interface_property_inherit` - 测试接口属性继承
- `test_abstract_interface` - 测试抽象接口
- `test_abstract_object` - 测试抽象对象
- `test_get_all_properties` - 测试获取所有属性
- `test_property_existence` - 测试属性存在性
- `test_signals` - 测试信号继承
- `test_method_override` - 测试方法重写
- `test_abstract_interface_method` - 测试抽象接口方法
- `test_abstract_object_method` - 测试抽象对象方法
- `test_signal_override` - 测试信号重写
- `test_abstract_signal_override` - 测试抽象信号重写

### test_std_interface.cpp
标准 D-Bus 接口测试（8 个用例）：
- `test_properties` - 测试 Properties 接口
- `test_introspect` - 测试 Introspectable 接口
- `TestGetWithContext` - 测试带上下文的 Get 方法
- `TestSetWithContext` - 测试带上下文的 Set 方法
- `PropertiesInterfaceWithoutObject` - 测试无对象的 Properties 接口
- `PropertiesInterfaceSetAndCommonProperties` - 测试 Properties 接口设置和通用属性
- `CommonPropertiesContextHelpers` - 测试通用属性上下文辅助函数
- `CommonPropertiesSetWithoutContextIsSafe` - 测试无上下文时设置通用属性的安全性

### test_property.cpp
属性系统测试（约 70+ 个用例）：
- `BasicType` - 测试基础类型属性
- `CustomType` - 测试自定义类型属性
- `Comparison` - 测试属性比较
- `NoNotify` - 测试无通知属性
- `VariantConversion` - 测试 variant 转换
- `Notify` - 测试属性通知
- `OutsiderGetterSetter` - 测试外部 getter/setter
- `OutsiderGetterSetterStringValidation` - 测试外部 getter/setter 字符串验证
- `OutsiderGetterSetterWithObserver` - 测试带观察者的外部 getter/setter
- `OutsiderGetterSetterComplexType` - 测试复杂类型的外部 getter/setter
- `GetRelateProperty` - 测试获取关联属性
- `SetRelateProperty` - 测试设置关联属性
- `ReferencePropertyBasics` - 测试引用属性基础功能
- `DifferentObjectTypes` - 测试不同对象类型
- `ErrorHandling` - 测试错误处理
- `HookRefProperties` - 测试引用属性钩子
- `HookRefPropertiesWithExpressionCalculation` - 测试带表达式计算的引用属性钩子
- `HookRefPropertiesBasicMultiReference` - 测试基础多引用属性钩子
- `HookRelateProperties` - 测试关联属性钩子
- `HookRelatePropertiesWithFuncCollection` - 测试带函数集合的关联属性钩子
- `ProcessPropertyValue` - 测试属性值处理
- `FromVariantRefProperty` - 测试从 variant 创建引用属性
- `FromVariantNormalValue` - 测试从 variant 创建普通值
- `MultipleRefPropertyCombination` - 测试多个引用属性组合
- `RefPropertyDataTypeConversionAccuracy` - 测试引用属性数据类型转换精度
- `PropertyReferenceErrorHandlingAndEdgeCases` - 测试属性引用错误处理和边界情况
- `RefPropertyRealTimeUpdate` - 测试引用属性实时更新
- `RefPropertyExpressionCalculationVerification` - 测试引用属性表达式计算验证
- `RefPropertyDifferentDataTypes` - 测试引用属性不同数据类型
- `FromVariantSyncProperty` - 测试从 variant 创建同步属性
- `SyncPropertyBasics` - 测试同步属性基础功能
- `SyncPropertyDeferredConnection` - 测试同步属性延迟连接
- `HookSyncProperties` - 测试同步属性钩子
- `SyncPropertyErrorHandling` - 测试同步属性错误处理
- `SyncPropertyRealTimeUpdate` - 测试同步属性实时更新
- `SyncPropertyDataTypeConversion` - 测试同步属性数据类型转换
- `MultipleSyncPropertyConnectionManagement` - 测试多个同步属性连接管理
- `GetRelatePropertyWithInterface` - 测试通过接口获取关联属性
- `SetRelatePropertyWithInterface` - 测试通过接口设置关联属性
- `HookRefPropertyWithInterface` - 测试通过接口的引用属性钩子
- `RefObjectBasicUsage` - 测试引用对象基础用法
- `RefObjectErrorHandling` - 测试引用对象错误处理
- `RefObjectDynamicLookup` - 测试引用对象动态查找
- `RefObjectTypeCompatibility` - 测试引用对象类型兼容性
- `RefObjectInterfaceAccess` - 测试引用对象接口访问
- `RefObjectClone` - 测试引用对象克隆
- `RefObjectVsRefProperty` - 测试引用对象与引用属性对比
- `RefObjectMemoryOptimization` - 测试引用对象内存优化
- `RefObjectBasicInvoke` - 测试引用对象基础调用
- `RefObjectInterfaceInvoke` - 测试引用对象接口调用
- `RefObjectBasicAsyncInvoke` - 测试引用对象基础异步调用
- `RefObjectInterfaceAsyncInvoke` - 测试引用对象接口异步调用
- `RefObjectInvokeNonExistentObject` - 测试调用不存在的对象
- `RefObjectComplexParameterPassing` - 测试引用对象复杂参数传递
- `RefObjectConcurrentInvoke` - 测试引用对象并发调用
- `RefObjectNullObjectFinder` - 测试引用对象空对象查找器
- `RefObjectEdgeCases` - 测试引用对象边界情况
- `RefObjectSetProperty` - 测试引用对象设置属性
- `RefObjectLifecycleManagementExtended` - 测试引用对象生命周期管理扩展
- `RefObjectVariantExtension` - 测试引用对象 variant 扩展
- `RefObjectExtendedErrorHandling` - 测试引用对象扩展错误处理
- `RefObjectPerformanceAndMemoryExtended` - 测试引用对象性能和内存扩展
- `RefObjectConcurrency` - 测试引用对象并发性
- `RefObjectSignalConnect` - 测试引用对象信号连接

### test_property_processors.cpp
属性处理器测试（11 个用例）：
- `SyncPropertyProcessor` - 测试同步属性处理器
- `RefPropertyProcessor` - 测试引用属性处理器
- `RefObjectProcessor` - 测试引用对象处理器
- `FunctionCallProcessor` - 测试函数调用处理器
- `ProcessorFactory` - 测试处理器工厂
- `ProcessorRegistration` - 测试处理器注册
- `ErrorHandling` - 测试错误处理
- `EdgeCases` - 测试边界情况
- `RefObjectProcessorSetterThrows` - 测试引用对象处理器 setter 抛出异常
- `RefPropertyProcessorSetterDelegates` - 测试引用属性处理器 setter 委托
- `RefPropertyProcessorGetterFailsWhenNull` - 测试引用属性处理器 getter 在 null 时失败

### test_property_concurrent_fix.cpp
属性并发修复测试（2 个用例）：
- `GetObjectNameConcurrentSafety` - 测试获取对象名称的并发安全性
- `HighConcurrencyStressTest` - 测试高并发压力测试

### test_context.cpp
上下文管理测试（6 个用例）：
- `ArgsAndStatusManagement` - 测试参数和状态管理（包括 const 版本的 `get_args()` 和 `get_service()`）
- `CallInfoInspection` - 测试调用信息检查
- `CurrentContextAccessAndErrorHandling` - 测试当前上下文访问和错误处理
- `AbstractObjectContextAccess` - 测试抽象对象上下文访问
- `AbstractInterfaceContextAccess` - 测试抽象接口上下文访问
- `ContextSetArgs` - 测试批量设置参数

### test_path.cpp
路径模型测试（6 个用例）：
- `constructor` - 测试路径构造函数
- `invalid_path` - 测试无效路径
- `comparison_operators` - 测试路径比较运算符
- `path_joining` - 测试路径拼接
- `parent_and_basename` - 测试父路径和基名
- `validation` - 测试路径验证

### test_path_iterator.cpp
路径迭代器测试（13 个用例）：
- `basic_navigation` - 测试基础导航
- `different_paths` - 测试不同路径
- `special_cases` - 测试特殊情况
- `reset_functionality` - 测试重置功能
- `bidirectional_navigation` - 测试双向导航
- `special_characters` - 测试特殊字符
- `unicode_paths` - 测试 Unicode 路径
- `boundary_conditions` - 测试边界条件
- `BasicFunctionality` - 测试基础功能
- `ReverseTraversal` - 测试反向遍历
- `Reset` - 测试重置
- `DISABLED_PerformanceBenchmark` - 性能基准测试（已禁用）
- `EdgeCases` - 测试边界情况

### test_service_utils.cpp
服务工具函数测试（2 个用例）：
- `ResolveObjectPathWithFallback` - 测试对象路径解析（带回退）
- `ResolveObjectPathWithCustomResolver` - 测试对象路径解析（自定义解析器）

### test_error_engine.cpp
错误引擎测试（23 个用例）：
- `RegisterAndGetError` - 测试注册和获取错误
- `RegisterConstErrorWithErrorInfo` - 测试使用错误信息注册常量错误
- `RegisterConstErrorDuplicate` - 测试重复注册常量错误
- `RegisterErrorSuccess` - 测试成功注册错误
- `RegisterErrorDuplicate` - 测试重复注册错误
- `GetFormatArgs` - 测试获取格式化参数
- `MakeError` - 测试创建错误
- `ErrorInfo` - 测试错误信息
- `ComplexFormat` - 测试复杂格式化
- `ReportError` - 测试报告错误
- `ReportErrorReusesUnsetLastError` - 测试报告错误重用未设置的最后一个错误
- `ReportErrorByName` - 测试按名称报告错误
- `SetLastErrorReturnsPrevious` - 测试设置最后一个错误返回前一个
- `LastError` - 测试最后一个错误
- `ValidateErrorName` - 测试验证错误名称
- `ComplexErrorChainLifecycle` - 测试复杂错误链生命周期
- `ComplexErrorReportAndSetInteraction` - 测试复杂错误报告和设置交互
- `ComplexErrorLevelAndFormatting` - 测试复杂错误级别和格式化
- `RegisterConstErrorWithErrorInfoOverload` - 测试使用错误信息注册常量错误重载
- `RegisterConstErrorDuplicatePath` - 测试重复路径注册常量错误
- `RegisterErrorDuplicatePath` - 测试重复路径注册错误
- `GetErrorInfoNotFound` - 测试获取不存在的错误信息
- `ReportErrorWithUnsetLastError` - 测试使用未设置的最后一个错误报告错误
- `MakeErrorWithErrorInfo` - 测试使用错误信息创建错误

### test_result.cpp
执行结果测试（15 个用例）：
- `test_basic_construction` - 测试基础构造
- `test_error_construction` - 测试错误构造
- `test_move_operations` - 测试移动操作
- `test_chaining` - 测试链式调用
- `test_error_handling` - 测试错误处理
- `test_timeout` - 测试超时
- `test_cancellation_result_then` - 测试取消结果 then
- `test_cancellation_result_catch_error` - 测试取消结果 catch_error
- `test_combined_operations` - 测试组合操作
- `test_type_conversion` - 测试类型转换
- `test_void_specialization` - 测试 void 特化
- `test_make_result` - 测试创建结果
- `test_default_error_selection` - 测试默认错误选择
- `test_throw_and_make_method_call_exception` - 测试抛出和创建方法调用异常
- `test_make_method_call_exception_uses_last_error` - 测试创建方法调用异常使用最后一个错误

### test_async_invoke.cpp
异步调用测试（11 个用例）：
- `test_sync_method` - 测试同步方法
- `test_async_method` - 测试异步方法
- `test_hybrid_method` - 测试混合方法
- `test_error_handling` - 测试错误处理
- `test_void_result` - 测试 void 结果
- `test_cancellation` - 测试取消
- `test_chain_operations` - 测试链式操作
- `test_parallel_operations` - 测试并行操作
- `test_timeout` - 测试超时
- `test_combined_operations` - 测试组合操作
- `test_signal_with_async` - 测试带异步的信号

### test_micro_component.cpp
微组件测试（7 个用例）：
- `TestMicroComponentInterface` - 测试微组件接口
- `TestMicroComponentConfigManageInterface` - 测试微组件配置管理接口
- `TestMicroComponentDebugInterface` - 测试微组件调试接口
- `TestMicroComponentRebootInterface` - 测试微组件重启接口
- `TestMicroComponentRebootInterfaceProcess` - 测试微组件重启接口 Process 方法
- `TestMicroComponentResetInterface` - 测试微组件重置接口
- `TestMicroComponentMaintenanceInterface` - 测试微组件维护接口

### test_utils.cpp
工具函数测试（1 个用例）：
- `test_is_valid_interface_name` - 测试验证接口名称

## 运行测试

### 运行所有 engine 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

**注意**: 
- meson 构建配置中只有一个测试注册（`libmcpp unit tests`），不能使用 `--suite engine` 运行特定模块测试
- 要通过 meson 传递参数给测试可执行文件，使用 `--test-args` 选项
- 推荐直接运行可执行文件，更简单直接

### 运行特定测试文件

```bash
# 测试引擎核心功能（所有用例）- 直接运行可执行文件（推荐）
./builddir/tests/libmcpp_test --gtest_filter="engine_test.*"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=engine_test.*"

# 测试对象管理（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="object_test.*"
meson test -C builddir --test-args="--gtest_filter=object_test.*"

# 测试接口系统（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="interface_test.*"
meson test -C builddir --test-args="--gtest_filter=interface_test.*"

# 测试接口继承（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="interface_inherit_test.*"
meson test -C builddir --test-args="--gtest_filter=interface_inherit_test.*"

# 测试标准接口（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="std_interface_test.*"
meson test -C builddir --test-args="--gtest_filter=std_interface_test.*"

# 测试属性系统（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="PropertyTest.*:PropertyRelateTest.*"
meson test -C builddir --test-args="--gtest_filter=PropertyTest.*:PropertyRelateTest.*"

# 测试属性处理器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="PropertyProcessorTest.*"
meson test -C builddir --test-args="--gtest_filter=PropertyProcessorTest.*"

# 测试属性并发修复（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="PropertyConcurrentFixTest.*"
meson test -C builddir --test-args="--gtest_filter=PropertyConcurrentFixTest.*"

# 测试上下文管理（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="ContextTest.*"
meson test -C builddir --test-args="--gtest_filter=ContextTest.*"

# 测试路径模型（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="path_test.*"
meson test -C builddir --test-args="--gtest_filter=path_test.*"

# 测试路径迭代器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="path_iterator_test.*:PathIterator.*"
meson test -C builddir --test-args="--gtest_filter=path_iterator_test.*:PathIterator.*"

# 测试服务工具函数（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="ServiceUtilsTest.*"
meson test -C builddir --test-args="--gtest_filter=ServiceUtilsTest.*"

# 测试错误引擎（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="ErrorEngineTest.*"
meson test -C builddir --test-args="--gtest_filter=ErrorEngineTest.*"

# 测试执行结果（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="result_test.*"
meson test -C builddir --test-args="--gtest_filter=result_test.*"

# 测试异步调用（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="async_invoke_test.*"
meson test -C builddir --test-args="--gtest_filter=async_invoke_test.*"

# 测试微组件（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="MicroComponentTest.*"
meson test -C builddir --test-args="--gtest_filter=MicroComponentTest.*"

# 测试工具函数（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="utils_test.*"
meson test -C builddir --test-args="--gtest_filter=utils_test.*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例（直接运行可执行文件，推荐）
./builddir/tests/libmcpp_test --gtest_filter="engine_test.test_engine_dbus_connection"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=engine_test.test_engine_dbus_connection"

# 运行多个测试用例（使用通配符）
./builddir/tests/libmcpp_test --gtest_filter="ErrorEngineTest.Register*"
meson test -C builddir --test-args="--gtest_filter=ErrorEngineTest.Register*"
```

## 测试统计

- **测试文件总数**: 17 个
- **测试用例总数**: 约 200+ 个
  - `test_engine.cpp`: 14 个
  - `test_object.cpp`: 10 个
  - `test_interface.cpp`: 14 个
  - `test_interface_inherit.cpp`: 11 个
  - `test_std_interface.cpp`: 8 个
  - `test_property.cpp`: 约 70+ 个
  - `test_property_processors.cpp`: 11 个
  - `test_property_concurrent_fix.cpp`: 2 个
  - `test_context.cpp`: 6 个
  - `test_path.cpp`: 6 个
  - `test_path_iterator.cpp`: 13 个（包含 1 个已禁用的性能测试）
  - `test_service_utils.cpp`: 2 个
  - `test_error_engine.cpp`: 23 个
  - `test_result.cpp`: 15 个
  - `test_async_invoke.cpp`: 11 个
  - `test_micro_component.cpp`: 7 个
  - `test_utils.cpp`: 1 个
- **测试覆盖的功能模块**:
  - Engine（引擎核心）
  - Object（对象管理）
  - Interface（接口系统）
  - Property（属性系统）
  - Context（上下文管理）
  - Path（路径模型）
  - Error Engine（错误引擎）
  - Result（执行结果）
  - Async Invoke（异步调用）
  - Micro Component（微组件）
  - Utils（工具函数）

## 代码覆盖率

当前测试覆盖率信息请参考 `UNTESTABLE_CASES.md` 文件，该文件记录了无法测试的代码部分及其原因。

### 调试测试

```bash
# 使用 gdb 调试测试
gdb --args ./builddir/tests/libmcpp_test --gtest_filter="engine_test.test_engine_dbus_connection"

# 启用详细输出（直接运行可执行文件）
./builddir/tests/libmcpp_test --gtest_filter="engine_test.*" --verbose

# 或通过 meson 启用详细输出
meson test -C builddir --verbose --test-args="--gtest_filter=engine_test.*"
```

## 测试注意事项

1. **meson test 命令格式**: meson 构建配置中只有一个测试注册（`libmcpp unit tests`），要传递参数给测试可执行文件，使用 `--test-args` 选项。格式：`meson test -C builddir --test-args="--gtest_filter=..."`。注意不要在 `--test-args` 中使用引号包裹整个参数，gtest_filter 的值可以用引号。
2. **直接运行可执行文件**: 推荐直接运行 `./builddir/tests/libmcpp_test`，这样更简单直接，不需要通过 meson。
3. **D-Bus 环境**: 部分测试用例（如 `test_engine.cpp`、`test_micro_component.cpp`、`test_std_interface.cpp`）需要 D-Bus 环境支持。测试会自动检测 D-Bus 是否可用，如果不可用会跳过相关测试。
4. **测试隔离**: 测试用例之间相互独立，每个测试用例都会正确初始化和清理资源。
5. **性能测试**: `test_path_iterator.cpp` 中包含一个已禁用的性能基准测试（`DISABLED_PerformanceBenchmark`），如需启用请确保添加适当的断言和运行时监控。
6. **并发测试**: `test_property_concurrent_fix.cpp` 包含并发安全性测试，需要多线程环境支持。
7. **属性系统测试**: `test_property.cpp` 包含大量属性系统测试用例，覆盖了引用属性、同步属性、引用对象等多种场景。
8. **上下文管理**: `test_context.cpp` 测试了上下文管理的各个方面，包括参数管理、调用信息、错误报告、处理器状态等。
9. **错误引擎**: `test_error_engine.cpp` 测试了错误注册、报告、格式化等完整功能。
10. **微组件测试**: `test_micro_component.cpp` 测试了微组件的各种接口，包括配置管理、调试、重启、重置、维护等。
11. **未测试用例**: 详细的未测试用例信息请参考 `UNTESTABLE_CASES.md` 文件。
