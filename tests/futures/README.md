# Futures 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Futures 模块的测试用例。Futures 模块提供了异步编程支持，包括 Promise/Future 模式、链式调用、组合操作（all/any）、超时处理、延迟执行、取消机制、异常处理等功能。同时还包括回调列表管理和状态池管理等辅助功能。

## 测试文件列表

Futures 模块包含以下测试文件：

1. `test_futures.cpp` - Future/Promise 核心功能测试
2. `test_state_pool.cpp` - 状态池管理测试

## 详细测试用例

### test_futures.cpp

Future/Promise 核心功能测试（85 个用例），包括：

#### 基础功能测试
- `BasicPromiseFuture` - Promise 和 Future 的基本功能
- `BasicErrorHandling` - catch_error 捕获异常
- `AsyncExecutionPolicy` - 异步执行策略
- `DeferredExecutionPolicy` - 延迟执行策略
- `BasicTimeout` - 超时处理

#### 链式调用测试
- `SimpleChain` - 简单链式调用
- `ChainWithFutureReturn` - 返回 Future 的链式调用
- `CatchErrorWithException` - 带异常的 catch_error
- `CatchErrorWithoutException` - 无异常的 catch_error
- `CatchErrorVoidReturnWithException` - void 返回类型的 catch_error（有异常）
- `CatchErrorVoidReturnWithoutException` - void 返回类型的 catch_error（无异常）
- `ErrorRecoveryWithIntValue` - 错误恢复（返回整数值）
- `ErrorRecoveryChain` - 错误恢复链
- `FinallyWithSuccess` - finally 回调（成功场景）
- `FinallyWithException` - finally 回调（异常场景）
- `TapWithSuccess` - tap 操作（成功场景）
- `TapWithException` - tap 操作（异常场景）

#### 组合操作测试（all）
- `AllWithSuccess` - all 操作（全部成功）
- `AllWithException` - all 操作（有异常）
- `AllCancelPropagation` - all 操作的取消传播
- `ContainerAllWithSuccess` - 容器版本的 all（全部成功）
- `ContainerAllWithException` - 容器版本的 all（有异常）
- `ContainerAllWithAnyFutureCancel` - 容器版本的 all（任意 Future 取消）
- `ContainerAllWithResultFutureCancel` - 容器版本的 all（结果 Future 取消）
- `AllOneChildCancelPropagation` - all 操作中一个子 Future 取消的传播
- `AllPartialSuccessThenException` - all 操作部分成功后异常
- `AllMultipleSimultaneousExceptions` - all 操作多个同时异常
- `AllMixedExceptionAndCancel` - all 操作混合异常和取消
- `AllWithDeferredExecution` - all 操作的延迟执行

#### 组合操作测试（any）
- `AnyWithSuccess` - any 操作（成功场景）
- `AnyCancelPropagation` - any 操作的取消传播
- `AnyAllChildrenCanceled` - any 操作所有子 Future 都被取消
- `AnyFirstSuccessCancel` - any 操作第一个成功后的取消
- `AnySuccessAfterSomeCanceled` - any 操作部分取消后成功
- `AnySuccessAfterSomeExceptions` - any 操作部分异常后成功
- `AnyWithMixedExceptions` - any 操作混合异常
- `ContainerAnyWithSuccess` - 容器版本的 any（成功场景）
- `AnyWithDeferredExecution` - any 操作的延迟执行

#### 工具函数测试
- `MakeReadyFuture` - 创建已完成的 Future
- `MakeExceptionalFuture` - 创建异常 Future
- `TimeoutFunctionSuccess` - timeout 函数（成功场景）
- `TimeoutFunctionTimeout` - timeout 函数（超时场景）
- `TimeoutFunctionCancelOnResultFutureCancel` - timeout 函数（结果 Future 取消）
- `TimeoutFunctionCancelOnOriginalPromiseCancel` - timeout 函数（原始 Promise 取消）
- `TimeoutFunctionCancelOnPromiseAfterTimeout` - timeout 函数（超时后 Promise 取消）
- `TimeoutFunctionWithAlreadyCancelledFuture` - timeout 函数（已取消的 Future）
- `DelayedExecution` - 延迟执行
- `DelayedFuture` - 延迟 Future
- `CancelDelayedFuture` - 取消延迟 Future

#### 取消机制测试
- `CancelChainedFuture` - 取消链式 Future
- `CancelCallbackExecution` - 取消回调执行
- `CancelCallbackNested` - 取消嵌套回调
- `CancelInnerFuture` - 取消内部 Future
- `CancelChainedNested` - 取消链式嵌套 Future
- `CancelWithCacheError` - 取消时缓存错误

#### 异常处理测试
- `CanceledException` - 取消异常
- `TimeoutException` - 超时异常
- `ExceptionInfoPreservationInCatchError` - catch_error 中异常信息保留
- `CancelExceptionHandling` - 取消异常处理
- `StdExceptionHandling` - 标准异常处理
- `FutureCatchErrorOnReadyValue` - catch_error 在已完成时直接返回原值（不执行 handler）
- `FutureCatchErrorHandlesUnknownException` - catch_error 处理未知异常类型，验证未知异常被正确包装为 `mc::exception`，handler 被调用，并返回非零值表示异常被正确处理

#### Promise/Future 边界场景测试
- `PromiseSetValueFromFuture` - Promise 接收 Future 结果（成功与失败）
- `PromiseSetValueAlreadySatisfiedThrows` - Promise 重复设置值抛出异常
- `PromiseSetExceptionAlreadySatisfiedThrows` - Promise 重复设置异常抛出异常
- `PromiseSetExceptionIgnoredAfterCancel` - Promise 取消后忽略异常设置
- `PromiseGetFutureTwiceThrows` - Promise 重复获取 Future 抛出异常
- `FutureGetForReady` - get_for 在就绪状态下返回结果
- `FutureWaitForReadyStatus` - wait_for 在就绪状态下返回 ready
- `FutureThenDispatchOnReadyState` - then 在就绪状态下使用 dispatch 策略
- `FutureThenDeferredOnReadyState` - then 在就绪状态下使用 deferred 策略
- `FutureAsyncGetDispatchOnReadyState` - async_get 在就绪状态下使用 dispatch 策略
- `FutureFinallyOnReadyState` - finally 在就绪状态下执行清理
- `FutureTapOnReadyState` - tap 在就绪状态下查看结果

#### 回调列表和状态池测试
- `CallbackListBasicOperations` - 回调列表基本操作（empty、push_back、execute_and_clear）
- `CallbackListClear` - 回调列表清空（不执行回调）
- `CallbackListSwap` - 回调列表交换
- `CallbackListMoveSemantics` - 回调列表移动语义
- `CallbackPoolGetStats` - 回调池统计信息获取
- `CallbackPoolSetMaxPoolSize` - 回调池最大大小设置
- `CallbackPoolClear` - 回调池清空
- `CallbackPoolReleaseNullptr` - 回调池释放 nullptr 节点
- `CallbackPoolReleaseNodeMaxSize` - 回调池释放节点达到最大大小
- `CallbackPoolAcquireReuseNode` - 回调池复用缓存节点
- `SafeInvokeExceptionHandling` - safe_invoke 异常处理

### test_state_pool.cpp

状态池管理测试（10 个用例），包括：

- `basic_pool_functionality` - 基本池功能测试
- `pool_reuse` - 池重用测试
- `pool_size_limit` - 池大小限制测试
- `different_size_types` - 不同大小类型测试
- `same_size_types_share_pool` - 相同大小类型共享池测试
- `pool_config` - 池配置测试
- `pool_clear` - 池清空测试
- `large_state_not_cached` - 大状态不缓存测试
- `pool_count_limit` - 池数量限制测试
- `concurrent_access` - 并发访问测试

> ⚠️ 说明：`state_pool::try_release_to_pool(nullptr, …)` 早退分支会在 `state_deleter::deallocate` 中继续对空指针执行 `state_base` 析构，属于未定义行为，真实业务也不会传入空指针。为保证测试稳定性，我们仅验证公开 API，可安全触达的分支通过上述用例覆盖，空指针路径以文档方式记录而不在单测中强制触发。

## 测试统计

- **测试文件总数**: 2
- **测试用例总数**: 94（已优化，删除了重复的测试用例）
  - `test_futures.cpp`: 84 个用例（已优化，删除了重复的 `CancelChainedFuture`，功能已合并到 `CancelChainedNested`）
  - `test_state_pool.cpp`: 10 个用例

## 测试优化说明

### 已优化的测试文件

1. **test_futures.cpp**
   - **删除的重复测试用例**：
     - `CancelChainedFuture` - 功能已合并到 `CancelChainedNested` 中
   - **优化结果**：从 85 个测试用例减少到 84 个测试用例

## 运行测试

### 运行所有测试

```bash
cd builddir
ninja test
```

### 运行特定测试文件

```bash
# 运行 futures 测试
./tests/libmcpp_test --gtest_filter="FuturesTest.*"

# 运行 state_pool 测试
./tests/libmcpp_test --gtest_filter="state_pool_test.*"
```

### 运行特定测试用例

```bash
# 运行特定测试用例
./tests/libmcpp_test --gtest_filter="FuturesTest.BasicPromiseFuture"
./tests/libmcpp_test --gtest_filter="FuturesTest.AllWithSuccess"
./tests/libmcpp_test --gtest_filter="state_pool_test.basic_pool_functionality"
```

## 测试注意事项

1. **异步执行**: 部分测试涉及异步执行，可能需要等待一定时间才能完成
2. **超时测试**: 超时相关的测试会使用较短的超时时间，确保测试能够及时完成
3. **并发测试**: state_pool 的并发访问测试会创建多个线程，确保线程安全性
4. **资源清理**: 所有测试都会在完成后清理创建的资源，避免资源泄漏
5. **回调池**: callback_pool 是单例，测试之间可能会共享状态，需要注意测试隔离
6. **异常处理**: 未知异常类型会被自动包装为 `mc::exception`，测试会验证异常被正确捕获和处理
7. **就绪状态测试**: 部分测试验证在 Future 已就绪状态下的行为，确保链式调用和异常处理在就绪状态下也能正常工作

