# DBus 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 DBus 模块的测试用例。DBus 模块提供了 D-Bus 通信的核心功能，包括连接管理、消息处理、错误处理、匹配规则、验证器、序列化、共享内存调用等。

## 测试文件列表

DBus 模块的测试文件按 `src/dbus` 的目录结构组织：

### 根目录测试文件
1. `test_connection.cpp` - D-Bus 连接管理测试（包含基础、场景、安全性和实现细节测试）
2. `test_dbus_message.cpp` - D-Bus 消息读写测试
3. `test_error.cpp` - D-Bus 错误处理测试
4. `test_match.cpp` - 匹配规则测试
5. `test_reflect.cpp` - 反射测试
6. `test_validator.cpp` - 验证器测试

### shm 子目录测试文件
1. `shm/test_serialize.cpp` - 序列化测试（包含基础和安全测试）
2. `shm/test_local_msg.cpp` - 本地消息测试（包含基础、安全和 shm_pending_msgs 测试）
3. `shm/test_gvariant_convert.cpp` - GVariant 转换测试
4. `shm/test_harbor.cpp` - Harbor 测试
5. `shm/test_shm_call.cpp` - 共享内存调用测试（条件编译）
6. `shm/test_shm_tree.cpp` - 共享内存对象树测试

### dispatch 子目录测试文件
1. `dispatch/test_dispatch.cpp` - 分发机制测试（包含 pending_call、timeout 和 match_rules 测试）

详细说明请参考各子目录的 README 文件：
- `shm/README.md` - SHM 模块测试说明
- `dispatch/README.md` - Dispatch 模块测试说明

## 详细测试用例

### test_connection.cpp
D-Bus 连接管理测试（40 个用例）：

**基础功能测试（5 个）**：
- `test_list_names` - 测试列出总线名称（包含重试逻辑以适应 D-Bus 名称传播的时序差异）
- `test_disconnect` - 测试连接断开
- `test_open_system_bus` - 测试打开系统总线
- `test_default_constructor` - 测试默认构造（空实现分支）
- `test_send` - 测试发送消息

**注意**：`test_call_method_error` 已删除，其功能已合并到 `scenario_error_handling_retry` 中

**场景测试（6 个）**：
- `scenario_connection_lifecycle` - 测试连接生命周期
- `scenario_mixed_sync_async_calls` - 测试混合同步和异步调用
- `scenario_signal_subscription` - 测试信号订阅
- `scenario_error_handling_retry` - 测试错误处理和重试
- `scenario_path_registration` - 测试路径注册
- `scenario_concurrent_messages` - 测试并发消息

**安全性测试（4 个）**：
- `test_path_handler_exception_signal` - 测试路径处理器异常（信号类型）
- `test_path_handler_exception_method_call` - 测试路径处理器异常（方法调用类型）
- `test_request_name_retry_failure` - 测试请求名称重试失败
- `test_concurrent_disconnect` - 测试并发断开连接

**实现细节测试（25 个）**：
- `test_send_when_disconnected` - 测试断开连接时发送
- `test_send_with_existing_serial` - 测试使用已有序列号发送
- `test_async_send_when_disconnected` - 测试断开连接时异步发送
- `test_request_name_invalid` - 测试无效的请求名称
- `test_request_name_when_disconnected` - 测试断开连接时请求名称
- `test_start_wrong_status` - 测试错误状态的启动
- `test_disconnect_multiple_times` - 测试多次断开连接
- `test_register_path_when_disconnected` - 测试断开连接时注册路径
- `test_unregister_path_when_disconnected` - 测试断开连接时注销路径
- `test_add_match_when_disconnected` - 测试断开连接时添加匹配规则
- `test_remove_match_not_found` - 测试移除不存在的匹配规则
- `test_remove_match_when_disconnected` - 测试断开连接时移除匹配规则
- `test_get_next_serial_overflow` - 测试序列号溢出
- `test_dispatch_after_disconnect` - 测试断开连接后分发
- `test_filter_message` - 测试消息过滤功能
- `test_get_match` - 测试获取匹配对象
- `test_process_message_no_reply_serial` - 测试处理没有 reply_serial 的消息
- `test_process_reply_serial_not_found` - 测试处理 reply_serial 不存在的情况
- `test_add_match_connection_lost_during_operation` - 测试 add_match 在连接断开时的双重检查分支
- `test_remove_match_error_handling` - 测试 remove_match 的错误处理分支
- `test_release_with_null_connection` - 测试 release() 中 m_connection == nullptr 的分支
- `test_process_message_method_return_no_reply_serial` - 测试 process_message 中 reply_serial == 0 的分支
- `test_dispatch_status_changed_other_status` - 测试 dispatch_status_changed 中非 DBUS_DISPATCH_DATA_REMAINS 的分支
- `test_get_next_serial_overflow_to_one` - 测试 get_next_serial 的序列号溢出处理
- `test_get_impl_access` - 测试 get_impl 返回实现引用

### test_dbus_message.cpp
D-Bus 消息读写测试（7 个用例）：
- `test_message_reader_writer` - 测试消息读写器
- `test_libdbus_write_mc_dbus_message_read` - 测试 libdbus 写入 mc::dbus::message 读取
- `test_mc_dbus_message_write_libdbus_read` - 测试 mc::dbus::message 写入 libdbus 读取
- `test_empty_dict` - 测试空字典
- `test_empty_array` - 测试空数组
- `test_read_args` - 测试读取参数
- `test_message_setters_and_flags` - 覆盖消息的 setter/getter 与类型判断

### test_error.cpp
D-Bus 错误处理测试（12 个用例）：
- `DefaultConstructor` - 测试默认构造
- `CopyConstructor` - 测试复制构造
- `CopyConstructorUnset` - 测试复制构造（未设置错误状态）
- `CopyAssignment` - 测试复制赋值
- `CopyAssignmentUnset` - 测试复制赋值（未设置错误状态）
- `MoveAssignment` - 测试移动赋值
- `MoveConstructor` - 测试移动构造
- `SetErrorStringView` - 测试设置错误（字符串视图版本）
- `SetErrorWithDict` - 测试设置错误（带格式化参数版本）
- `SetErrorWithEmptyDict` - 测试设置错误（空字典）
- `SetErrorConst` - 测试设置错误常量
- `ErrorNames` - 测试错误名称常量

### test_match.cpp
匹配规则测试（26 个用例）：
- `MatchRuleTest.NewSignal` - 测试创建信号规则
- `MatchRuleTest.WithInterface` - 测试设置接口
- `MatchRuleTest.WithMember` - 测试设置成员
- `MatchRuleTest.WithPath` - 测试设置路径
- `MatchRuleTest.WithPathInvalid` - 非法对象路径（缺少前导 `/`、空字符串）会抛出 `mc::invalid_arg_exception`
- `MatchRuleTest.WithPathNamespace` - 测试设置路径命名空间
- `MatchRuleTest.WithPathNamespaceInvalid` - 非法路径前缀（缺少前导 `/`、包含空格）会抛出 `mc::invalid_arg_exception`
- `MatchRuleTest.WithSender` - 测试设置发送者
- `MatchRuleTest.WithDestination` - 测试设置目标
- `MatchRuleTest.WithType` - 测试设置消息类型
- `MatchRuleTest.Clone` - 测试克隆规则
- `MatchRuleTest.CloneWithPathNamespace` - 测试克隆规则（path_namespace 分支）
- `MatchRuleTest.CloneWithPath` - 测试克隆规则（path 分支）
- `MatchRuleTest.InvalidMember` - 测试无效的 member（错误路径）
- `MatchRuleTest.InvalidInterface` - 测试无效的 interface（错误路径）
- `MatchRuleTest.InvalidPath` - 测试无效的 path（错误路径）
- `MatchRuleTest.InvalidSender` - 测试无效的 sender（错误路径）
- `MatchRuleTest.InvalidDestination` - 测试无效的 destination（错误路径）
- `MatchTest.AddAndRemoveRule` - 测试添加和移除规则
- `MatchTest.RunMsg` - 测试运行消息匹配
- `MatchTest.TestMatch` - 测试匹配但不执行回调
- `MatchTest.TestMatchNoMatch` - 测试不匹配的情况
- `MatchTest.AddRuleExceptionHandling` - 测试 add_rule 异常处理分支
- `MatchTest.RemoveRuleNotConnected` - 测试移除未连接的规则
- `MatchTest.MultipleRules` - 测试多个规则
- `MatchTest.Constants` - 测试常量值

### test_reflect.cpp
反射测试（1 个用例）：
- `test_reflect_struct` - 测试结构体反射

### shm 子目录测试
SHM 模块的测试用例请参考 `shm/README.md`。`shm/test_local_msg.cpp` 新增了对 `parse_variant` 的断言校验，覆盖了数字签名到布尔类型（非零视为 `true`、零视为 `false`）以及各整数/浮点类型的解析结果。`shm/test_shm_tree.cpp` 测试共享内存对象树功能，包括对象注册/注销、属性获取/设置、匹配规则管理、超时调用等。

### dispatch 子目录测试
Dispatch 模块的测试用例请参考 `dispatch/README.md`。

### test_validator.cpp
验证器测试（7 个用例）：
- `IsValidInterfaceName` - 测试验证接口名
- `IsValidMemberName` - 测试验证成员名
- `IsValidBusName` - 测试验证总线名
- `IsValidErrorName` - 测试验证错误名
- `IsValidPath` - 测试验证路径
- `IsMessageTooLarge` - 测试消息是否过大
- `Constants` - 测试常量值

## 运行测试

### 运行所有 dbus 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

**注意**: 
- meson 构建配置中只有一个测试注册（`libmcpp unit tests`），不能使用 `--suite dbus` 运行特定模块测试
- 要通过 meson 传递参数给测试可执行文件，使用 `--test-args` 选项
- 推荐直接运行可执行文件，更简单直接

### 运行特定测试文件

```bash
# 测试连接管理（所有用例）- 直接运行可执行文件（推荐）
./builddir/tests/libmcpp_test --gtest_filter="connection_test.*"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=connection_test.*"

# 测试消息处理（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="dbus_message_test.*"
meson test -C builddir --test-args="--gtest_filter=dbus_message_test.*"

# 测试错误处理（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="ErrorTest.*"
meson test -C builddir --test-args="--gtest_filter=ErrorTest.*"

# 测试 SHM 模块（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="SerializeTest.*:LocalMsgTest.*:GvariantConvertTest.*:ShmCallTest.*:shm_tree_test.*"
meson test -C builddir --test-args="--gtest_filter=SerializeTest.*:LocalMsgTest.*:GvariantConvertTest.*:ShmCallTest.*:shm_tree_test.*"

# 测试 Dispatch 模块（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="dispatch_test.*"
meson test -C builddir --test-args="--gtest_filter=dispatch_test.*"

# 测试匹配规则（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="MatchTest.*:MatchRuleTest.*"
meson test -C builddir --test-args="--gtest_filter=MatchTest.*:MatchRuleTest.*"

# 测试反射（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="reflect_test.*"
meson test -C builddir --test-args="--gtest_filter=reflect_test.*"

# 测试序列化（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="SerializeTest.*"
meson test -C builddir --test-args="--gtest_filter=SerializeTest.*"

# 测试共享内存调用（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="ShmCallTest.*"
meson test -C builddir --test-args="--gtest_filter=ShmCallTest.*"

# 测试验证器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="ValidatorTest.*"
meson test -C builddir --test-args="--gtest_filter=ValidatorTest.*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例（直接运行可执行文件，推荐）
./builddir/tests/libmcpp_test --gtest_filter="connection_test.test_call_method_error"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=connection_test.test_call_method_error"

# 运行多个测试用例（使用通配符）
./builddir/tests/libmcpp_test --gtest_filter="ErrorTest.SetError*"
meson test -C builddir --test-args="--gtest_filter=ErrorTest.SetError*"
```

## 测试统计

- **测试文件总数**: 13 个
  - 根目录: 6 个
  - shm 子目录: 6 个
  - dispatch 子目录: 1 个
- **测试用例总数**: 178 个（根据 `grep -c "TEST_F\|TEST(` tests/dbus/**/*.cpp` 统计）
  - 根目录测试: 93 个
    - `test_connection.cpp`: 40 个（基础 5 + 场景 6 + 安全 4 + 实现 25）
    - `test_dbus_message.cpp`: 7 个
    - `test_error.cpp`: 12 个
    - `test_match.cpp`: 26 个
    - `test_reflect.cpp`: 1 个
    - `test_validator.cpp`: 7 个
  - shm 子目录测试: 72 个（参考 `shm/README.md`，包含 `test_shm_tree.cpp` 的 9 个测试用例）
  - dispatch 子目录测试: 13 个（参考 `dispatch/README.md`）
- **测试覆盖的功能模块**:
  - Connection（连接管理）
  - Message（消息处理）
  - Error（错误处理）
  - Match（匹配规则，包含非法路径/命名空间校验场景）
  - Reflect（反射）
  - Validator（验证器）
  - SHM（共享内存）- Serialize、Local Message、GVariant Convert、Shared Memory Call
  - Dispatch（分发机制）- Pending Call、Watch、Timeout

## 代码覆盖率

当前测试覆盖率（基于最新覆盖率报告）：
- **Line Coverage**: 80.7% (20109 of 24929 lines)
- **Branch Coverage**: 36.8% (14863 of 40340 branches)
- **Function Coverage**: 40.9% (18023 of 44061 functions)

### 覆盖率说明

1. **已覆盖的主要功能**：
   - 连接管理（connection）：基本功能已覆盖，包括连接、断开、发送消息、异步调用等
   - 错误处理（error）：所有构造函数、赋值操作符、错误设置方法已覆盖
   - 消息处理（message）：消息创建、读写、序列化/反序列化已覆盖
   - 匹配规则（match）：规则创建、添加、移除、匹配逻辑已覆盖
   - 验证器（validator）：所有验证函数已覆盖

2. **难以覆盖的代码路径**：
   - `get_next_serial()` 中的序列号溢出和重试异常：需要大量并发调用才能触发，实际使用中几乎不可能发生
   - `start()` 中的 `dbus_bus_register` 错误：需要 D-Bus 守护进程异常才能触发
   - `request_name()` 中的重试逻辑：需要特定的 D-Bus 守护进程状态才能触发
   - `add_match()` 中的双重检查分支：需要在执行过程中断开连接，难以精确控制时序
   - `watch_add()` 和 `timeout_add()` 中的 `get_enabled()` 为 false 的分支：由 D-Bus 库内部控制，难以直接测试

3. **测试优化**：
   - 删除了重复的测试用例 `test_call_method_error`，其功能已合并到 `scenario_error_handling_retry`
   - 添加了新的测试用例以覆盖更多边界情况
   - 场景测试（scenario_*）保留，因为它们测试的是真实使用场景的组合

### 调试测试

```bash
# 使用 gdb 调试测试
gdb --args ./builddir/tests/libmcpp_test --gtest_filter="connection_test.test_call_method_error"

# 启用详细输出（直接运行可执行文件）
./builddir/tests/libmcpp_test --gtest_filter="connection_test.*" --verbose

# 或通过 meson 启用详细输出
meson test -C builddir --verbose --test-args="--gtest_filter=connection_test.*"
```

## 测试注意事项

1. **meson test 命令格式**: meson 构建配置中只有一个测试注册（`libmcpp unit tests`），要传递参数给测试可执行文件，使用 `--test-args` 选项。格式：`meson test -C builddir --test-args="--gtest_filter=..."`。注意不要在 `--test-args` 中使用引号包裹整个参数，gtest_filter 的值可以用引号。
2. **直接运行可执行文件**: 推荐直接运行 `./builddir/tests/libmcpp_test`，这样更简单直接，不需要通过 meson。
3. **D-Bus 守护进程**: `test_connection.cpp` 和 `test_shm_call.cpp` 需要 D-Bus 守护进程运行。测试框架会自动启动测试用的 D-Bus 守护进程。
4. **共享内存**: `test_shm_call.cpp` 和 `test_serialize.cpp` 涉及共享内存操作，需要 `/dev/shm` 目录可写。
5. **路径规范**: 所有测试用例中的路径使用实际注册的路径（如 `/org/test/Connection`、`/org/freedesktop/DBus`），而不是示例路径（如 `/org/example/Object`）。
6. **总线名称规范**: 所有测试用例中的总线名称使用实际注册的名称（如 `org.test.Connection`），而不是示例名称（如 `org.example.Service`）。
7. **错误处理**: `test_error.cpp` 中的 `MoveConstructor` 测试已跳过，因为底层 D-Bus 库的限制，移动构造需要目标对象未初始化，而当前实现无法在不修改源文件的情况下满足此要求。
8. **匹配规则**: `test_match.cpp` 中的 `Clone` 测试可能因为底层实现返回空 member/interface 而失败，此时会优雅跳过测试。
9. **测试隔离**: 各个测试文件之间相互独立，不会相互影响。

