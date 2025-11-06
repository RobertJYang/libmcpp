# Runtime 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Runtime 模块的测试用例。Runtime 模块是 MC++ 库的运行时基础模块，提供了运行时上下文管理、执行器抽象、线程池管理、立即执行上下文等核心功能。

## 测试文件列表

Runtime 模块包含以下测试文件：

1. `test_runtime_basic.cpp` - 基本用法示例测试
2. `test_runtime_context.cpp` - 运行时上下文测试
3. `test_executor.cpp` - 执行器包装器测试
4. `test_any_executor.cpp` - 轻量级执行器包装器测试
5. `test_thread_list.cpp` - 线程列表管理测试

## 详细测试用例

### test_runtime_basic.cpp
基本用法示例测试（4 个用例）：
- `BasicUsageExample` - 测试基本用法示例，包括 IO 执行器和 work 执行器的使用
- `ExecutorObjectUsage` - 测试执行器对象的使用方式
- `ExecutorTagUsage` - 测试执行器标签的使用方式
- `EmbeddedSystemScenario` - 测试嵌入式系统场景下的使用

### test_runtime_context.cpp
运行时上下文测试（34 个用例）：
- `BasicInitializeAndStart` - 测试基础初始化和启动
- `DuplicateStart` - 测试重复启动
- `IoExecutorBasicPost` - 测试 IO 执行器基础 post 操作
- `SystemExecutorBasicPost` - 测试系统执行器基础 post 操作
- `DeferOperation` - 测试延迟操作
- `DispatchOperation` - 测试分发操作
- `MultiThreadIoExecutor` - 测试多线程 IO 执行器
- `MixedExecutorUsage` - 测试混合执行器使用
- `ExecutorLifetime` - 测试执行器生命周期
- `DuplicateInitialize` - 测试重复初始化
- `EnsureStartFromUninitialized` - 测试从未初始化状态确保启动
- `EnsureStartFromInitialized` - 测试从已初始化状态确保启动
- `EnsureStartFromStopped` - 测试从已停止状态确保启动（会抛出异常）
- `DuplicateStartImpl` - 测试重复启动实现
- `StopWhenNotRunning` - 测试未运行时停止
- `JoinWhenEmpty` - 测试空线程列表时 join
- `GetThreadCount` - 测试获取线程数
- `ZeroThreadsUsesDefault` - 测试零线程使用默认值
- `ThreadCountExceedsMax` - 测试线程数超过最大值
- `ImmediateExecutorBasic` - 测试立即执行器基础功能
- `ImmediateContextBasic` - 测试立即上下文基础功能
- `PostToContexts` - 测试向上下文投递任务
- `DeferToContexts` - 测试向上下文延迟投递任务
- `DispatchToContexts` - 测试向上下文分发任务
- `MakeStrands` - 测试创建 strand 执行器
- `ContextRestartAfterStop` - 测试停止后上下文重启
- `IsStoppedUninitialized` - 测试未初始化状态的 is_stopped
- `DestructorWithoutStop` - 测试未停止时的析构函数
- `ThreadCountEdgeCases` - 测试线程数边缘情况
- `StartImplWithNonEmptyThreads` - 测试线程列表非空时启动
- `JoinWhenThreadListEmpty` - 测试线程列表为空时 join
- `StartImplBothThreadListsNonEmpty` - 测试两个线程列表都不为空时启动
- `IsStoppedStateStopped` - 测试停止状态的 is_stopped
- `ThreadExceptionHandling` - 测试线程异常处理

### test_executor.cpp
执行器包装器测试（37 个用例）：
- `BasicConstruction` - 测试基础构造
- `DefaultConstruction` - 测试默认构造
- `CopySemantics` - 测试拷贝语义
- `MoveSemantics` - 测试移动语义
- `PostOperation` - 测试 post 操作
- `DeferOperation` - 测试 defer 操作
- `DispatchOperation` - 测试 dispatch 操作
- `ComparisonOperations` - 测试比较操作
- `WrappingDifferentExecutorTypes` - 测试包装不同类型的执行器
- `StrandSerialExecution` - 测试 strand 串行执行
- `ExceptionHandling` - 测试异常处理
- `LifecycleManagement` - 测试生命周期管理
- `SharedSemantics` - 测试共享语义
- `CopyConstructionWithNullptr` - 测试 nullptr 拷贝构造
- `CopyAssignmentSelfAssignment` - 测试自赋值拷贝
- `CopyAssignmentWithNullptr` - 测试 nullptr 拷贝赋值
- `MoveAssignmentSelfAssignment` - 测试自赋值移动
- `EqualitySameImplPointer` - 测试相同 impl 指针的相等性
- `EqualityWithNullptr` - 测试与 nullptr 的相等性
- `InequalityOperator` - 测试不等操作符
- `WorkLifecycleWithNullptr` - 测试 nullptr 的工作生命周期
- `ContextWithNullptrThrows` - 测试 nullptr 的 context 抛出异常
- `DestructorWithMultipleReferences` - 测试多引用的析构函数
- `DestructorWithSingleReference` - 测试单引用的析构函数
- `CopyAssignmentBothNullptr` - 测试两个 nullptr 的拷贝赋值
- `CopyAssignmentReleaseReturnsFalse` - 测试 release 返回 false 的拷贝赋值
- `EqualityEqualReturnsFalse` - 测试 equal 返回 false 的相等性
- `ContextThrowsException` - 测试 context 抛出异常
- `WrapDifferentExecutorTypes` - 测试包装不同执行器类型
- `EqualityOneNullptr` - 测试一个 nullptr 的相等性
- `EqualityBothNonNullptrDifferent` - 测试两个非 nullptr 但不同的相等性
- `CopyAssignmentSelf` - 测试自赋值拷贝
- `CopyAssignmentThisNullptr` - 测试 this 为 nullptr 的拷贝赋值
- `CopyAssignmentReleaseReturnsTrue` - 测试 release 返回 true 的拷贝赋值
- `CopyAssignmentOtherNullptr` - 测试 other 为 nullptr 的拷贝赋值
- `CopyAssignmentOtherNonNullptr` - 测试 other 非 nullptr 的拷贝赋值
- `MoveAssignmentSelf` - 测试自赋值移动
- `MoveAssignmentOtherNullptr` - 测试 other 为 nullptr 的移动赋值

### test_any_executor.cpp
轻量级执行器包装器测试（26 个用例）：
- `BasicConstruction` - 测试基础构造
- `DefaultConstruction` - 测试默认构造
- `CopySemantics` - 测试拷贝语义
- `MoveSemantics` - 测试移动语义
- `PostOperation` - 测试 post 操作
- `DeferOperation` - 测试 defer 操作
- `DispatchOperation` - 测试 dispatch 操作
- `ComparisonOperations` - 测试比较操作
- `StrandWrapping` - 测试 strand 包装
- `ValidWithInvalidRuntimeExecutor` - 测试无效 runtime executor 的 valid
- `EqualitySameTypeSameValue` - 测试相同类型相同值的相等性
- `InequalityOperator` - 测试不等操作符
- `WorkLifecycleWithMonostate` - 测试 monostate 的工作生命周期
- `ContextWithMonostateThrows` - 测试 monostate 的 context 抛出异常
- `OperationsWithMonostateThrows` - 测试 monostate 的操作抛出异常
- `ConstructionFromSystemExecutor` - 测试从 system executor 构造
- `ConstructionFromRuntimeExecutor` - 测试从 runtime executor 构造
- `EqualitySameTypeDifferentValue` - 测试相同类型不同值的相等性
- `EqualityMonostateBoth` - 测试两个 monostate 的相等性
- `EqualitySameExecutor` - 测试相同执行器的相等性
- `ValidBoostAsioExecutor` - 测试 boost::asio 执行器的 valid
- `ConstructionFromArbitraryExecutor` - 测试从任意执行器构造
- `ValidRuntimeExecutorReturnsFalse` - 测试 runtime executor valid 返回 false
- `ValidRuntimeExecutorReturnsTrue` - 测试 runtime executor valid 返回 true
- `EqualitySameIndexDifferentExec` - 测试相同 index 不同执行器的相等性
- `EqualitySameIndexSameExec` - 测试相同 index 相同执行器的相等性

### test_thread_list.cpp
线程列表管理测试（35 个用例）：
- `ThreadNodeBasic` - 测试线程节点基础功能
- `ThreadNodeMove` - 测试线程节点移动
- `ThreadNodeJoinable` - 测试线程节点可 join 性
- `ThreadNodeGetId` - 测试线程节点获取 ID
- `ThreadNodeJoinWhenNotJoinable` - 测试不可 join 时 join
- `ThreadListBasic` - 测试线程列表基础功能
- `StartThreadsWithoutIndex` - 测试不带索引启动线程
- `StartThreadsWithIndex` - 测试带索引启动线程
- `AddThread` - 测试添加线程
- `RemoveThreadSuccess` - 测试成功移除线程
- `RemoveThreadWithNullptr` - 测试移除 nullptr 线程
- `RemoveThreadNotInList` - 测试移除不在列表中的线程
- `JoinAll` - 测试等待所有线程
- `Clear` - 测试清空线程列表
- `GetThreadCount` - 测试获取线程数
- `Empty` - 测试检查线程列表是否为空
- `VisitThreads` - 测试访问线程
- `VisitThreadsConst` - 测试常量访问线程
- `JoinAllWhenEmpty` - 测试空列表时 join all
- `ClearWhenEmpty` - 测试空列表时 clear
- `MixedAddRemove` - 测试混合添加和移除
- `DestructorWaitsForThreads` - 测试析构函数等待线程
- `ThreadNodeDestructorNotJoinable` - 测试不可 join 的线程节点析构
- `StartThreadsZeroCount` - 测试零线程数启动
- `StartThreadsZeroCountWithIndex` - 测试零线程数带索引启动
- `ClearThenReuse` - 测试清空后重用
- `VisitThreadsEmpty` - 测试访问空线程列表
- `VisitThreadsConstEmpty` - 测试常量访问空线程列表
- `RemoveThreadNotFound` - 测试移除未找到的线程
- `RemoveThreadFound` - 测试移除找到的线程
- `ThreadNodeDestructorJoinable` - 测试可 join 的线程节点析构
- `ThreadNodeJoinWhenJoinable` - 测试可 join 时 join
- `RemoveThreadFoundInLoop` - 测试在循环中找到并移除线程
- `RemoveThreadNotFoundAfterLoop` - 测试循环后未找到线程
- `RemoveThreadNotFoundReturnsFalse` - 测试移除未找到返回 false

## 运行测试

### 运行所有 runtime 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

**注意**: 
- meson 构建配置中只有一个测试注册（`libmcpp unit tests`），不能使用 `--suite runtime` 运行特定模块测试
- 要通过 meson 传递参数给测试可执行文件，使用 `--test-args` 选项
- 推荐直接运行可执行文件，更简单直接

### 运行特定测试文件

```bash
# 测试基本用法（所有用例）- 直接运行可执行文件（推荐）
./builddir/tests/libmcpp_test --gtest_filter="RuntimeBasicTest.*"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=RuntimeBasicTest.*"

# 测试运行时上下文（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="RuntimeContextTest.*"
meson test -C builddir --test-args="--gtest_filter=RuntimeContextTest.*"

# 测试执行器包装器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="ExecutorTest.*"
meson test -C builddir --test-args="--gtest_filter=ExecutorTest.*"

# 测试轻量级执行器包装器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="AnyExecutorTest.*"
meson test -C builddir --test-args="--gtest_filter=AnyExecutorTest.*"

# 测试线程列表管理（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="ThreadListTest.*"
meson test -C builddir --test-args="--gtest_filter=ThreadListTest.*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例（直接运行可执行文件，推荐）
./builddir/tests/libmcpp_test --gtest_filter="RuntimeContextTest.BasicInitializeAndStart"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=RuntimeContextTest.BasicInitializeAndStart"

# 运行多个测试用例（使用通配符）
./builddir/tests/libmcpp_test --gtest_filter="ExecutorTest.Copy*"
meson test -C builddir --test-args="--gtest_filter=ExecutorTest.Copy*"
```

## 测试统计

- **测试文件总数**: 5 个
- **测试用例总数**: 约 136 个
- **测试覆盖的功能模块**:
  - Runtime Context（运行时上下文）
  - Executor（执行器包装器）
  - Any Executor（轻量级执行器包装器）
  - Thread List（线程列表管理）
  - Immediate Context/Executor（立即执行上下文和执行器）

## 代码覆盖率

当前测试覆盖率：
- **Line Coverage**: 约 78.2%
- **Branch Coverage**: 约 33.5%
- **Function Coverage**: 约 93.6%

### 调试测试

```bash
# 使用 gdb 调试测试
gdb --args ./builddir/tests/libmcpp_test --gtest_filter="RuntimeContextTest.BasicInitializeAndStart"

# 启用详细输出（直接运行可执行文件）
./builddir/tests/libmcpp_test --gtest_filter="RuntimeContextTest.*" --verbose

# 或通过 meson 启用详细输出
meson test -C builddir --verbose --test-args="--gtest_filter=RuntimeContextTest.*"
```

## 测试注意事项

1. **meson test 命令格式**: meson 构建配置中只有一个测试注册（`libmcpp unit tests`），要传递参数给测试可执行文件，使用 `--test-args` 选项。格式：`meson test -C builddir --test-args="--gtest_filter=..."`。注意不要在 `--test-args` 中使用引号包裹整个参数，gtest_filter 的值可以用引号。
2. **直接运行可执行文件**: 推荐直接运行 `./builddir/tests/libmcpp_test`，这样更简单直接，不需要通过 meson。
3. **并发测试**: `test_thread_list.cpp` 包含并发安全性测试，需要多线程环境支持。
4. **runtime_context 生命周期**: `test_runtime_context.cpp` 测试 runtime_context 的完整生命周期，包括初始化、启动、停止和清理。测试使用 `TestWithRuntime` 基类，在测试前后自动管理 runtime_context 的状态。
5. **执行器引用计数**: `test_executor.cpp` 测试执行器的引用计数机制，包括拷贝、移动和析构时的引用计数管理。
6. **variant 类型执行器**: `test_any_executor.cpp` 测试使用 `std::variant` 实现的轻量级执行器包装器，避免虚函数开销。
7. **线程异常处理**: `test_runtime_context.cpp` 中的 `ThreadExceptionHandling` 测试验证线程中的异常处理机制，确保异常不会导致程序崩溃。
8. **测试隔离**: runtime 模块的测试使用 `TestWithRuntime` 基类，在测试前后自动重置 runtime_context 状态，确保测试之间的独立性。
9. **线程池管理**: `test_thread_list.cpp` 测试线程池的生命周期管理，包括启动、停止、清理和并发操作。
10. **执行器类型**: runtime 模块支持多种执行器类型，包括 boost::asio 执行器、strand 执行器、immediate 执行器等，测试覆盖了各种执行器的使用场景。

