# Array 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Array 模块的测试用例。Array 模块提供了动态数组容器类型，支持多种数据类型的存储和操作。

## 测试文件列表

Array 模块包含以下测试文件：

1. `test_array.cpp` - 数组容器测试（约 56 个用例）
2. `test_variants.cpp` - variants 容器测试（约 42 个用例）

## 运行测试

### 运行所有 array 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

### 运行特定测试文件

```bash
# 测试数组容器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="array_test.*"

# 测试 variants 容器（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="variants_test.*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例
./builddir/tests/libmcpp_test --gtest_filter="array_test.default_constructor"
```

## 测试统计

- **测试文件总数**: 2 个
- **测试用例总数**: 约 98 个
  - `test_array.cpp`: 约 56 个
  - `test_variants.cpp`: 约 42 个

