# Sync 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Sync 模块的测试用例。Sync 模块提供了同步原语功能，包括互斥锁、共享互斥锁、小锁等。

## 测试文件列表

Sync 模块包含以下测试文件：

1. `test_small_mutex.cpp` - 小锁测试（9 个用例）
2. `test_mutex_box.cpp` - 互斥锁盒子测试（23 个用例，已优化）
3. `test_shared_mutex.cpp` - 共享互斥锁测试（2 个用例）
4. `test_upgrade_mutex.cpp` - 升级互斥锁测试（11 个 TYPED_TEST，每个为 2 种类型运行，共 22 个用例）

## 运行测试

### 运行所有 sync 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

### 运行特定测试文件

```bash
# 测试小锁（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="SmallMutexTest.*"

# 测试互斥锁盒子（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="*MutexBox*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例
./builddir/tests/libmcpp_test --gtest_filter="SmallMutexTest.BasicLockUnlock"
```

## 测试统计

- **测试文件总数**: 4 个
- **测试用例总数**: 94 个（已优化，删除了重复的测试用例）
  - `test_small_mutex.cpp`: 9 个用例
  - `test_mutex_box.cpp`: 23 个用例（已优化，从 24 个减少到 23 个）
  - `test_shared_mutex.cpp`: 40 个用例（19 个 TYPED_TEST × 2 种类型 + 2 个普通 TEST）
  - `test_upgrade_mutex.cpp`: 22 个用例（11 个 TYPED_TEST × 2 种类型）

