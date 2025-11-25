# Crypto 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Crypto 模块的测试用例。Crypto 模块提供了加密和校验功能，包括 CRC 校验等。

## 测试文件列表

Crypto 模块包含以下测试文件：

1. `test_crc.cpp` - CRC 校验测试（约 3 个用例）

## 运行测试

### 运行所有 crypto 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

### 运行特定测试文件

```bash
# 测试 CRC 校验（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="*CRC*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例
./builddir/tests/libmcpp_test --gtest_filter="CRC32Test.*"
```

## 测试统计

- **测试文件总数**: 1 个
- **测试用例总数**: 约 3 个
  - `test_crc.cpp`: 约 3 个（CRC8 和 CRC32 测试）

