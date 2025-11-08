# DBus Dispatch 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 DBus Dispatch 模块的测试用例。Dispatch 模块提供了 D-Bus 消息分发机制的核心功能，包括 pending_call（待处理调用）、watch（文件描述符监视）、timeout（超时处理）等。

## 测试文件列表

Dispatch 模块包含以下测试文件：

1. `test_dispatch.cpp` - 分发机制测试

## 详细测试用例

### test_dispatch.cpp
分发机制测试（12 个用例）：

**基础分发测试（4 个）**：
- `test_connection_dispatch` - 测试 connection::dispatch 方法
- `test_watch_timeout_via_connection` - 测试通过连接间接使用 watch 和 timeout
- `test_watch_timeout_stop_on_disconnect` - 测试连接断开时 watch 和 timeout 停止
- `test_async_send_with_reply` - 测试异步发送并接收回复

**并发和场景测试（3 个）**：
- `test_multiple_concurrent_calls` - 测试多个并发调用
- `test_dispatch_while_receiving` - 测试在接收消息时进行 dispatch（多线程场景）

**Pending Call 测试（3 个）**：
- `test_pending_call_already_completed` - 测试已完成的 pending call
- `test_pending_call_move_operations` - 测试 pending call 移动操作
- `test_pending_call_stop_before_reply` - 测试在回复前停止 pending call

**Timeout 测试（2 个）**：
- `test_timeout_handling` - 测试超时处理
- `test_timeout_with_long_interval` - 测试长间隔超时

**Match Rules 测试（1 个）**：
- `test_add_and_remove_match_rules` - 测试添加和移除匹配规则

## 运行测试

### 运行所有 dispatch 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

**注意**: 
- meson 构建配置中只有一个测试注册（`libmcpp unit tests`），不能使用 `--suite dispatch` 运行特定模块测试
- 要通过 meson 传递参数给测试可执行文件，使用 `--test-args` 选项
- 推荐直接运行可执行文件，更简单直接

### 运行特定测试文件

```bash
# 测试分发机制（所有用例）- 直接运行可执行文件（推荐）
./builddir/tests/libmcpp_test --gtest_filter="dispatch_test.*"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=dispatch_test.*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例（直接运行可执行文件，推荐）
./builddir/tests/libmcpp_test --gtest_filter="dispatch_test.test_pending_call_via_async_send"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=dispatch_test.test_pending_call_via_async_send"

# 运行多个测试用例（使用通配符）
./builddir/tests/libmcpp_test --gtest_filter="dispatch_test.test_*"
meson test -C builddir --test-args="--gtest_filter=dispatch_test.test_*"
```

## 测试统计

- **测试文件总数**: 1 个
- **测试用例总数**: 12 个
- **测试覆盖的功能模块**:
  - Pending Call（待处理调用）
  - Watch（文件描述符监视）
  - Timeout（超时处理）
  - Connection Dispatch（连接分发）
  - Match Rules（匹配规则）

## 代码覆盖率

当前测试覆盖率：
- **Line Coverage**: 约 71.3%
- **Branch Coverage**: 约 22.2%
- **Function Coverage**: 约 72.4%

### 调试测试

```bash
# 使用 gdb 调试测试
gdb --args ./builddir/tests/libmcpp_test --gtest_filter="dispatch_test.test_pending_call_via_async_send"

# 启用详细输出（直接运行可执行文件）
./builddir/tests/libmcpp_test --gtest_filter="dispatch_test.*" --verbose

# 或通过 meson 启用详细输出
meson test -C builddir --verbose --test-args="--gtest_filter=dispatch_test.*"
```

## 测试注意事项

1. **meson test 命令格式**: meson 构建配置中只有一个测试注册（`libmcpp unit tests`），要传递参数给测试可执行文件，使用 `--test-args` 选项。格式：`meson test -C builddir --test-args="--gtest_filter=..."`。注意不要在 `--test-args` 中使用引号包裹整个参数，gtest_filter 的值可以用引号。
2. **直接运行可执行文件**: 推荐直接运行 `./builddir/tests/libmcpp_test`，这样更简单直接，不需要通过 meson。
3. **内部实现**: pending_call、watch 和 timeout 是内部实现类，主要通过 connection 接口间接使用，测试主要通过 connection 的公共接口验证这些功能。
4. **异步处理**: 分发机制涉及异步消息处理，测试中需要使用适当的同步机制（如条件变量）等待异步操作完成。
5. **D-Bus 守护进程**: 所有测试需要 D-Bus 守护进程运行。测试框架会自动启动测试用的 D-Bus 守护进程。
6. **测试隔离**: 各个测试用例之间相互独立，不会相互影响。

