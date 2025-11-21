# Exception 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Exception 模块的测试用例。Exception 模块提供了异常处理机制，包括异常类型、异常代码、异常消息等。

## 测试文件列表

Exception 模块包含以下测试文件：

1. `test_exception.cpp` - 异常处理测试（约 26 个用例）

## 运行测试

### 运行所有 exception 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

### 运行特定测试文件

```bash
# 测试异常处理（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="ExceptionTest.*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例
./builddir/tests/libmcpp_test --gtest_filter="ExceptionTest.BasicExceptionTest"
```

## 测试统计

- **测试文件总数**: 1 个
- **测试用例总数**: 约 26 个
  - `test_exception.cpp`: 约 26 个

