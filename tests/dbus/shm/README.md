# DBus SHM 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 DBus SHM（共享内存）模块的测试用例。SHM 模块提供了 D-Bus 共享内存通信的核心功能，包括本地消息处理、序列化、GVariant 转换、共享内存树等。

## 测试文件列表

SHM 模块包含以下测试文件：

1. `test_serialize.cpp` - 序列化测试（包含基础和安全测试）
2. `test_local_msg.cpp` - 本地消息测试（包含基础、安全和 shm_pending_msgs 测试）
3. `test_gvariant_convert.cpp` - GVariant 转换测试
4. `test_harbor.cpp` - Harbor 测试
5. `test_shm_call.cpp` - 共享内存调用测试（条件编译）

## 详细测试用例

### test_serialize.cpp
序列化测试（23 个用例）：

**基础序列化测试（7 个）**：
- `SerializeArrays` - 测试序列化数组
- `SerializeMsg` - 测试序列化消息
- `SerializeBasicTypes` - 测试序列化基本类型
- `SerializeNestedArrays` - 测试序列化嵌套数组
- `SerializeDicts` - 测试序列化字典
- `SerializeMixedTypes` - 测试序列化混合类型
- `WriteVariantElements` - 测试写入 variant 元素

**边界和覆盖测试（9 个）**：
- `WriteNumberVariousTypes` - 测试写入各种数字类型
- `WriteStringVariousLengths` - 测试写入各种长度字符串
- `WriteLargeArray` - 测试写入大数组
- `WriteInnerCrossBlock` - 测试写入跨块内部数据
- `WriteArgWithSignatureVariousTypes` - 测试使用签名写入各种类型参数
- `WriteArrayOrDictByteType` - 测试写入数组或字典字节类型
- `ReadValueInvalidType` - 测试读取无效类型
- `ReadLongStringInvalidCookie` - 测试读取长字符串无效 cookie
- `ReadTableAsDict` - 测试读取表作为字典

**读取值测试（5 个）**：
- `ReadValueBooleanFalse` - 测试读取布尔值 false
- `ReadValueBooleanTrue` - 测试读取布尔值 true
- `ReadValueNumberReal` - 测试读取实数
- `ReadValueUserdata` - 测试读取用户数据
- `DeserializeInvalidFormat` - 测试反序列化无效格式
- `DeserializeEmptyMessage` - 测试反序列化空消息

**安全性测试（1 个）**：
- `WriteArgDepthExceedsLimit` - 测试写入参数深度超过限制

### test_local_msg.cpp
本地消息测试（23 个用例）：

**基础功能测试（11 个）**：
- `LocalCallFlag` - 测试本地调用标志
- `ErrorInfo` - 测试错误信息
- `Append` - 测试追加参数
- `Pack` - 测试打包消息
- `AppendArgs` - 测试追加参数（多个）
- `NewDBusMsg` - 测试创建 D-Bus 消息
- `ConstructFromVariants` - 测试从 variants 构造
- `ConstructFromVariantsEdgeCases` - 测试从 variants 构造边界情况
- `AppendReturnArgsCases` - 测试追加返回参数情况
- `ParseVariantVariousTypes` - 测试解析各种类型 variant
- `AppendArgsSignatureParseError` - 测试追加参数签名解析错误
- `SetterAndVariantParsingHelpers` - 测试成员设置以及路径与签名解析
- `ConstructFromVariantsParseFallback` - 测试参数解析失败时的回退逻辑

**安全性测试（6 个）**：
- `AppendArgsSizeMismatch` - 测试追加参数大小不匹配
- `NewDBusMsgInvalidType` - 测试创建无效类型的 D-Bus 消息
- `AppendReturnArgsMultipleNonArray` - 测试追加多个非数组返回参数
- `ScenarioConcurrentPackUnpack` - 场景测试：多线程并发 pack/unpack
- `ScenarioConcurrentConstructFromVariants` - 场景测试：多线程并发从 variants 构造
- `ScenarioLargeMessagePackUnpack` - 场景测试：大消息 pack/unpack

**shm_pending_msgs 测试（4 个）**：
- `ShmPendingMsgsSendAndReply` - 测试发送和回复行为
- `ShmPendingMsgsClear` - 测试清除功能
- `ShmPendingMsgsMultipleDestinations` - 测试多个目的地
- `ShmPendingMsgsConcurrentSend` - 场景测试：多线程并发发送

### test_gvariant_convert.cpp
GVariant 转换测试（7 个用例）：
- `ArrayOfVariantsToGVariant` - 测试 variant 数组转 GVariant
- `ArrayToGVariant` - 测试数组转 GVariant
- `ArrayToGVariantWithoutSignature` - 测试无签名的数组转 GVariant
- `BasicTypesToGVariant` - 测试基本类型转 GVariant
- `DictToGVariant` - 测试字典转 GVariant
- `SigUnitLengthAndErrors` - 测试签名长度计算及异常处理
- `GvariantAutoFreeCopyMove` - 测试 gvariant_auto_free 的拷贝/移动语义

### test_shm_call.cpp
共享内存调用测试（6 个用例）：
- `TestRegisterProperties` - 测试注册属性
- `TestIncrement` - 测试增量操作
- `TestNumAndStr` - 测试数字和字符串
- `TestAdd` - 测试加法操作
- `TestSubscribePropertiesChanged` - 测试订阅属性变更
- `TestDBusCall` - 测试 D-Bus 调用

## 运行测试

### 运行所有 shm 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

**注意**: 
- meson 构建配置中只有一个测试注册（`libmcpp unit tests`），不能使用 `--suite shm` 运行特定模块测试
- 要通过 meson 传递参数给测试可执行文件，使用 `--test-args` 选项
- 推荐直接运行可执行文件，更简单直接

### 运行特定测试文件

```bash
# 测试序列化（所有用例）- 直接运行可执行文件（推荐）
./builddir/tests/libmcpp_test --gtest_filter="SerializeTest.*"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=SerializeTest.*"

# 测试本地消息（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="LocalMsgTest.*"
meson test -C builddir --test-args="--gtest_filter=LocalMsgTest.*"

# 测试 GVariant 转换（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="GvariantConvertTest.*"
meson test -C builddir --test-args="--gtest_filter=GvariantConvertTest.*"

# 测试共享内存调用（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="ShmCallTest.*"
meson test -C builddir --test-args="--gtest_filter=ShmCallTest.*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例（直接运行可执行文件，推荐）
./builddir/tests/libmcpp_test --gtest_filter="SerializeTest.SerializeArrays"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=SerializeTest.SerializeArrays"

# 运行多个测试用例（使用通配符）
./builddir/tests/libmcpp_test --gtest_filter="SerializeTest.Serialize*"
meson test -C builddir --test-args="--gtest_filter=SerializeTest.Serialize*"
```

## 测试统计

- **测试文件总数**: 5 个
- **测试用例总数**: 55 个
  - `test_serialize.cpp`: 23 个
  - `test_local_msg.cpp`: 23 个（包含 local_msg 19 个 + shm_pending_msgs 4 个）
  - `test_gvariant_convert.cpp`: 7 个
  - `test_harbor.cpp`: 4 个
  - `test_shm_call.cpp`: 6 个（条件编译）
- **测试覆盖的功能模块**:
  - Serialize（序列化）
  - Local Message（本地消息）
  - Shm Pending Messages（共享内存待处理消息）
  - GVariant Convert（GVariant 转换）
  - Harbor（共享内存管理）
  - Shared Memory Call（共享内存调用）

## 代码覆盖率

当前测试覆盖率：
- **Line Coverage**: 约 62.8%
- **Branch Coverage**: 约 21.8%
- **Function Coverage**: 约 45.6%

### 调试测试

```bash
# 使用 gdb 调试测试
gdb --args ./builddir/tests/libmcpp_test --gtest_filter="SerializeTest.SerializeArrays"

# 启用详细输出（直接运行可执行文件）
./builddir/tests/libmcpp_test --gtest_filter="SerializeTest.*" --verbose

# 或通过 meson 启用详细输出
meson test -C builddir --verbose --test-args="--gtest_filter=SerializeTest.*"
```

## 测试注意事项

1. **meson test 命令格式**: meson 构建配置中只有一个测试注册（`libmcpp unit tests`），要传递参数给测试可执行文件，使用 `--test-args` 选项。格式：`meson test -C builddir --test-args="--gtest_filter=..."`。注意不要在 `--test-args` 中使用引号包裹整个参数，gtest_filter 的值可以用引号。
2. **直接运行可执行文件**: 推荐直接运行 `./builddir/tests/libmcpp_test`，这样更简单直接，不需要通过 meson。
3. **共享内存**: `test_shm_call.cpp` 和 `test_serialize.cpp` 涉及共享内存操作，需要 `/dev/shm` 目录可写。
4. **GVariant 依赖**: `test_gvariant_convert.cpp` 需要 GLib 库支持，确保系统已安装 GLib 开发库。
5. **条件编译**: `test_shm_call.cpp` 只在 `enable_conan_compile` 选项启用时编译，因为它依赖于共享内存树的完整实现。
6. **异步处理**: 共享内存调用涉及异步消息处理，测试中可能需要适当的等待时间。
7. **测试隔离**: 各个测试文件之间相互独立，不会相互影响。

## 未覆盖场景说明

- `mc::dbus::harbor` 与 `mc::dbus::shm_tree` 中依赖真实共享内存 Daemon 与 D-Bus 服务的流程（如 `harbor::start`、`harbor::process_dbus_message`、`shm_tree::register_object`）需要外部环境配合，当前 CI 环境难以稳定构建，暂以说明方式标记。
- `mc::dbus::serialize::read_pointer` 及与运行期指针有关的分支需要真实的用户数据句柄，在单元测试中无法安全伪造，保持未覆盖。
- `mc::dbus::message_queue` 深层并发路径需多个后台线程与共享内存队列协作，现阶段未在单元测试中模拟，建议在集成测试环境中补充。

