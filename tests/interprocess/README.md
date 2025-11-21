# Interprocess 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Interprocess 模块的测试用例。Interprocess 模块提供了进程间通信的基础设施，包括共享内存管理、共享内存分配器和进程间互斥锁/读写锁，用于支持多进程应用程序的同步和资源共享。

## 目录结构

Interprocess 模块的测试目录结构对应源代码目录结构：

```
tests/interprocess/
├── README.md                          # 本文档
├── meson.build                        # 构建配置
├── test_shared_memory.cpp             # 共享内存测试
├── test_shared_memory_manager.cpp     # 共享内存管理器测试
├── test_shared_memory_allocator.cpp   # 共享内存分配器测试
└── mutex/                             # 互斥锁测试目录
    ├── README.md                      # 互斥锁测试文档
    ├── test_ipc_mutex.cpp             # IPC 互斥锁测试
    └── test_shared_mutex.cpp          # IPC 读写锁和包装类测试
```

## 测试文件列表

Interprocess 模块包含以下测试文件：

1. `test_shared_memory.cpp` - 共享内存测试
2. `test_shared_memory_manager.cpp` - 共享内存管理器测试
3. `test_shared_memory_allocator.cpp` - 共享内存分配器测试
4. `mutex/test_ipc_mutex.cpp` - IPC 互斥锁测试（详见 `mutex/README.md`）
5. `mutex/test_shared_mutex.cpp` - IPC 读写锁和包装类测试（详见 `mutex/README.md`）

## 详细测试用例

### test_shared_memory.cpp
共享内存测试（14 个用例，已优化）：
- `create_shared_memory` - 测试创建共享内存
- `format_shm_name` - 测试共享内存名称格式化
- `get_properties` - 测试获取共享内存属性
- `offset_calculation` - 测试偏移量计算和指针恢复
- `get_allocator` - 测试获取分配器
- `minimum_size` - 测试最小内存大小
- `open_existing` - 测试打开已存在的共享内存
- `is_valid` - 测试共享内存有效性检查
- `offset_boundary_cases` - 测试偏移量边界情况
- `data_address_and_size` - 测试数据地址和大小
- `CreateWithEmptyName` - 测试 create() 传入空名称（已合并 `create_with_empty_name` 的功能）
- `CreateWithInsufficientSize` - 测试打开现有共享内存但大小不足
- `CreateWithInitMemoryFailure` - 测试 init_memory() 失败的情况
- `CreateWithRegisterProcessFailure` - 测试 register_process() 失败（进程槽位已满）

### test_shared_memory_manager.cpp
共享内存管理器测试（10 个用例，已优化）：
- `create_manager` - 测试创建共享内存管理器
- `get_shared_memory` - 测试获取共享内存对象
- `remove_on_exit` - 测试 REMOVE_ON_EXIT 选项
- `manual_cleanup` - 测试手动清理
- `set_remove_on_exit` - 测试设置自动清理选项
- `static_remove` - 测试静态 remove_shared_memory 方法
- `format_name` - 测试格式化名称
- `default_size` - 测试默认大小
- `RemoveIfExistsWithExistingShm` - 测试 REMOVE_IF_EXISTS 选项在共享内存存在时删除并输出日志（已合并 `remove_if_exists` 的功能）
- `RemoveSharedMemoryEmptyName` - 测试 remove_shared_memory() 传入空名称

### test_shared_memory_allocator.cpp
共享内存分配器测试（14 个用例）：
- `basic_allocate_deallocate` - 测试基础内存分配和释放
- `typed_allocate` - 测试类型安全的分配
- `allocate_array` - 测试数组分配
- `construct_destroy` - 测试对象构造和析构
- `create` - 测试 create 方法（分配并构造）
- `create_array` - 测试 create_array 方法
- `multiple_allocations` - 测试多次分配和释放
- `memory_statistics` - 测试内存统计信息
- `allocation_failure` - 测试内存不足时的分配
- `get_base_address` - 测试获取基地址
- `get_total_size` - 测试获取总大小
- `deallocate_nullptr` - 测试释放空指针
- `aligned_allocation` - 测试对齐分配
- `block_split_and_merge` - 测试块分割和合并

    ### mutex/test_ipc_mutex.cpp
    IPC 互斥锁测试（22 个用例），详见 `mutex/README.md`。

    ### mutex/test_shared_mutex.cpp
    IPC 读写锁和包装类测试（66 个用例），详见 `mutex/README.md`。

## 运行测试

### 运行所有 interprocess 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

**注意**: 
- meson 构建配置中只有一个测试注册（`libmcpp unit tests`），不能使用 `--suite interprocess` 运行特定模块测试
- 要通过 meson 传递参数给测试可执行文件，使用 `--test-args` 选项
- 推荐直接运行可执行文件，更简单直接

### 运行特定测试文件

```bash
# 测试共享内存（所有用例）- 直接运行可执行文件（推荐）
./builddir/tests/libmcpp_test --gtest_filter="shared_memory_test.*"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=shared_memory_test.*"

# 测试共享内存管理器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="shared_memory_manager_test.*"
meson test -C builddir --test-args="--gtest_filter=shared_memory_manager_test.*"

# 测试共享内存分配器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="shared_memory_allocator_test.*"
meson test -C builddir --test-args="--gtest_filter=shared_memory_allocator_test.*"

# 测试 IPC 互斥锁（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="IpcMutexTestFixture.*"
meson test -C builddir --test-args="--gtest_filter=IpcMutexTestFixture.*"

# 测试 IPC 读写锁和包装类（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="SharedMutexTestFixture.*"
meson test -C builddir --test-args="--gtest_filter=SharedMutexTestFixture.*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例（直接运行可执行文件，推荐）
./builddir/tests/libmcpp_test --gtest_filter="shared_memory_test.create_shared_memory"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=shared_memory_test.create_shared_memory"

# 运行多个测试用例（使用通配符）
./builddir/tests/libmcpp_test --gtest_filter="SharedMutexTestFixture.IpcMutex*"
meson test -C builddir --test-args="--gtest_filter=SharedMutexTestFixture.IpcMutex*"
```

    ## 测试统计

    - **测试文件总数**: 5 个
    - **测试用例总数**: 132 个（已优化，删除了重复的测试用例）
      - `test_shared_memory.cpp`: 14 个用例（已优化，删除了重复的 `create_with_empty_name`，功能已合并到 `CreateWithEmptyName`）
      - `test_shared_memory_manager.cpp`: 10 个用例（已优化，删除了重复的 `remove_if_exists`，功能已合并到 `RemoveIfExistsWithExistingShm`）
      - `test_shared_memory_allocator.cpp`: 14 个用例
      - `mutex/test_ipc_mutex.cpp`: 21 个用例
      - `mutex/test_shared_mutex.cpp`: 73 个用例
- **测试覆盖的功能模块**:
  - shared_memory（共享内存）
  - shared_memory_manager（共享内存管理器）
  - shared_memory_allocator（共享内存分配器）
  - ipc_mutex（进程间互斥锁）
  - ipc_shared_mutex（进程间读写锁）
  - mutex（互斥锁包装类）
  - shared_mutex（读写锁包装类）

### 调试测试

```bash
# 使用 gdb 调试测试
gdb --args ./builddir/tests/libmcpp_test --gtest_filter="shared_memory_test.create_shared_memory"

# 启用详细输出（直接运行可执行文件）
./builddir/tests/libmcpp_test --gtest_filter="SharedMutexTestFixture.*" --verbose

# 或通过 meson 启用详细输出
meson test -C builddir --verbose --test-args="--gtest_filter=SharedMutexTestFixture.*"
```

## 测试注意事项

1. **meson test 命令格式**: meson 构建配置中只有一个测试注册（`libmcpp unit tests`），要传递参数给测试可执行文件，使用 `--test-args` 选项。格式：`meson test -C builddir --test-args="--gtest_filter=..."`。注意不要在 `--test-args` 中使用引号包裹整个参数，gtest_filter 的值可以用引号。
2. **直接运行可执行文件**: 推荐直接运行 `./builddir/tests/libmcpp_test`，这样更简单直接，不需要通过 meson。
3. **测试隔离性**: 所有测试用例都是独立的，不会相互影响。每个测试用例都会创建新的共享内存和锁实例。
4. **共享内存清理**: 测试使用 `REMOVE_ON_EXIT` 选项，确保测试后自动清理共享内存，避免资源泄漏。
5. **进程间锁**: `ipc_mutex` 和 `ipc_shared_mutex` 基于进程 ID，同一进程的不同线程会被认为是"重复获取"。测试中使用包装后的 `mutex` 和 `shared_mutex` 类来支持单进程多线程场景。
6. **并发测试**: 大量测试用例涉及多线程并发，测试了线程安全性和竞争条件。
7. **分支覆盖率**: 测试用例重点覆盖了各种条件分支，包括错误处理、边界条件和状态转换路径。
8. **超时测试**: `IpcMutexTryLockForTimeout` 测试需要保持锁超过超时时间，确保正确验证超时行为。

