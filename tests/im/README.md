# IM 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 IM 模块的测试用例。IM 模块提供了索引内存（Indexed Memory）功能，包括基数树（Radix Tree）、节点管理、事务处理、自定义分配器等。

## 测试文件列表

IM 模块包含以下测试文件：

1. `test_radix_tree.cpp` - 基数树测试（约 11 个用例）
2. `test_radix_tree_iterator.cpp` - 基数树迭代器测试（约 15 个用例）
3. `test_node.cpp` - 节点测试（约 11 个用例）
4. `test_node_list.cpp` - 节点列表测试（约 14 个用例）
5. `test_simple_node.cpp` - 简单节点测试（约 4 个用例）
6. `test_transaction.cpp` - 事务处理测试（约 20 个用例）
7. `test_custom_allocator.cpp` - 自定义分配器测试（约 4 个用例）

## 运行测试

### 运行所有 im 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

### 运行特定测试文件

```bash
# 测试基数树（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="*RadixTree*"

# 测试节点（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="*Node*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例
./builddir/tests/libmcpp_test --gtest_filter="RadixTreeTest.*"
```

## 测试统计

- **测试文件总数**: 7 个
- **测试用例总数**: 约 79 个（已优化，删除了重复的测试用例）
  - `test_radix_tree.cpp`: 约 11 个（已优化，删除了重复的 `EmptyTreeIterator`，功能已合并到 `test_radix_tree_iterator.cpp` 的 `EmptyTreeIterator`）
  - `test_radix_tree_iterator.cpp`: 约 15 个
  - `test_node.cpp`: 约 11 个
  - `test_node_list.cpp`: 约 14 个
  - `test_simple_node.cpp`: 约 4 个
  - `test_transaction.cpp`: 约 20 个
  - `test_custom_allocator.cpp`: 约 4 个

## 测试优化说明

### 已优化的测试文件

1. **test_radix_tree.cpp**
   - **删除的重复测试用例**：
     - `EmptyTreeIterator` - 功能已合并到 `test_radix_tree_iterator.cpp` 的 `EmptyTreeIterator` 中
   - **优化结果**：从 12 个测试用例减少到 11 个测试用例

