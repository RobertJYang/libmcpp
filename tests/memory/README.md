# Memory 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Memory 模块的测试用例。Memory 模块提供了内存管理功能，包括智能指针（shared_ptr、weak_ptr）、共享对象、线程安全等。

## 测试文件列表

Memory 模块包含以下测试文件：

1. `test_shared_ptr_basic.cpp` - 基础 shared_ptr 测试（约 19 个用例）
2. `test_shared_ptr_thread_safety.cpp` - shared_ptr 线程安全测试（约 3 个用例）
3. `test_weak_ptr.cpp` - weak_ptr 测试（约 20 个用例）
4. `test_shared_object_ctor.cpp` - 共享对象构造函数测试（约 12 个用例）
5. `test_shared_base.cpp` - 共享基类测试（约 13 个用例）

## 运行测试

### 运行所有 memory 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

### 运行特定测试文件

```bash
# 测试 shared_ptr（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="*SharedPtr*"

# 测试 weak_ptr（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="*WeakPtr*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例
./builddir/tests/libmcpp_test --gtest_filter="SharedPtrTest.*"
```

## 测试统计

- **测试文件总数**: 5 个
- **测试用例总数**: 约 67 个
  - `test_shared_ptr_basic.cpp`: 约 19 个
  - `test_shared_ptr_thread_safety.cpp`: 约 3 个
  - `test_weak_ptr.cpp`: 约 20 个
  - `test_shared_object_ctor.cpp`: 约 12 个
  - `test_shared_base.cpp`: 约 13 个

