# DB 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 DB 模块的测试用例。DB 模块提供了数据库操作功能，包括数据库管理、表操作、事务处理、查询、索引等。

## 测试文件列表

DB 模块包含以下测试文件：

1. `test_database.cpp` - 数据库管理测试（约 14 个用例）
2. `test_table.cpp` - 表操作测试（约 9 个用例）
3. `test_transaction.cpp` - 事务处理测试（约 10 个用例）
4. `test_query_advanced.cpp` - 高级查询测试（约 6 个用例）
5. `test_index.cpp` - 索引测试（约 7 个用例）
6. `test_byte_buffer.cpp` - 字节缓冲区测试（约 5 个用例）

## 运行测试

### 运行所有 db 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

### 运行特定测试文件

```bash
# 测试数据库管理（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="*database*"

# 测试表操作（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="*table*"

# 测试事务处理（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="*transaction*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例
./builddir/tests/libmcpp_test --gtest_filter="DatabaseTest.*"
```

## 测试统计

- **测试文件总数**: 6 个
- **测试用例总数**: 约 50 个
  - `test_database.cpp`: 约 13 个（已删除重复的 `index_name` 测试，该测试在 `test_table.cpp` 中更全面）
  - `test_table.cpp`: 约 9 个
  - `test_transaction.cpp`: 约 10 个
  - `test_query_advanced.cpp`: 约 6 个
  - `test_index.cpp`: 约 7 个
  - `test_byte_buffer.cpp`: 约 5 个

## 优化说明

- 已删除 `test_database.cpp` 中的 `index_name` 测试用例，因为 `test_table.cpp` 中的 `index_name` 和 `index_name_composite` 测试更全面，覆盖了更多场景。

