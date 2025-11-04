# Debounce 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Debounce 模块的测试用例。Debounce 模块提供了多种防抖算法，包括平均值防抖、中位数防抖、连续计数防抖和二进制连续计数防抖，用于消除输入信号中的噪声和抖动。

## 测试文件列表

Debounce 模块包含以下测试文件：

1. `test_average.cpp` - 平均值防抖测试
2. `test_median.cpp` - 中位数防抖测试
3. `test_continue.cpp` - 连续计数防抖测试
4. `test_binary_continue.cpp` - 二进制连续计数防抖测试

## 详细测试用例

### test_average.cpp
平均值防抖测试（7 个用例）：
- `basic_window_unsigned` - 测试基础窗口平均功能（无符号模式，窗口=3）
- `trim_min_max_then_average` - 测试窗口裁剪最小最大值后再平均（窗口=5，窗口 >= 3）
- `even_window_size_and_sliding` - 测试偶数窗口（窗口=4）和窗口滑动行为
- `signed_input_and_adjust_output` - 测试有符号输入与输出调整（窗口=3）
- `signed_input_multiple_negatives` - 测试有符号输入完整场景（窗口=5，多个负数）
- `clear_state` - 测试清除状态功能
- `invalid_size_throw` - 测试构造参数校验

### test_median.cpp
中位数防抖测试（5 个用例）：
- `even_window_size` - 测试偶数窗口大小（窗口=5）
- `odd_window_size_and_sliding` - 测试奇数窗口大小（窗口=4）和窗口滑动行为
- `signed_input_and_adjust_output` - 测试有符号输入（>127 按补码视为负数）与输出调整（窗口=5）
- `clear_state` - 测试清除状态功能
- `invalid_size_throw` - 测试构造参数校验

### test_continue.cpp
连续计数防抖测试（3 个用例）：
- `get_value` - 完整功能测试：连续相同值稳定、稳定后继续输入相同值、稳定后输入不同值需重新累积
- `clear_state` - 测试清除状态功能
- `invalid_count_throw` - 测试构造参数校验

### test_binary_continue.cpp
二进制连续计数防抖测试（3 个用例）：
- `become_stable_high_low` - 测试针对二值输入，分别对 0/1 达到计数后稳定（1 需要 2 次，0 需要 3 次）
- `clear_state` - 测试清除状态功能
- `invalid_count_throw` - 测试构造参数校验

## 运行测试

### 运行所有 debounce 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

**注意**: 
- meson 构建配置中只有一个测试注册（`libmcpp unit tests`），不能使用 `--suite debounce` 运行特定模块测试
- 要通过 meson 传递参数给测试可执行文件，使用 `--test-args` 选项
- 推荐直接运行可执行文件，更简单直接

### 运行特定测试文件

```bash
# 测试平均值防抖（所有用例）- 直接运行可执行文件（推荐）
./builddir/tests/libmcpp_test --gtest_filter="debounce_average.*"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=debounce_average.*"

# 测试中位数防抖（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="debounce_median.*"
meson test -C builddir --test-args="--gtest_filter=debounce_median.*"

# 测试连续计数防抖（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="debounce_continue.*"
meson test -C builddir --test-args="--gtest_filter=debounce_continue.*"

# 测试二进制连续计数防抖（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="debounce_binary_continue.*"
meson test -C builddir --test-args="--gtest_filter=debounce_binary_continue.*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例（直接运行可执行文件，推荐）
./builddir/tests/libmcpp_test --gtest_filter="debounce_average.basic_window_unsigned"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=debounce_average.basic_window_unsigned"

# 运行多个测试用例（使用通配符）
./builddir/tests/libmcpp_test --gtest_filter="debounce_*.clear_state"
meson test -C builddir --test-args="--gtest_filter=debounce_*.clear_state"
```

## 测试统计

- **测试文件总数**: 4 个
- **测试用例总数**: 18 个
  - test_average.cpp: 7 个用例
  - test_median.cpp: 5 个用例
  - test_continue.cpp: 3 个用例
  - test_binary_continue.cpp: 3 个用例
- **测试覆盖的功能模块**:
  - Average（平均值防抖）
  - Median（中位数防抖）
  - Continue（连续计数防抖）
  - BinaryContinue（二进制连续计数防抖）

### 调试测试

```bash
# 使用 gdb 调试测试
gdb --args ./builddir/tests/libmcpp_test --gtest_filter="debounce_average.basic_window_unsigned"

# 启用详细输出（直接运行可执行文件）
./builddir/tests/libmcpp_test --gtest_filter="debounce_average.*" --verbose

# 或通过 meson 启用详细输出
meson test -C builddir --verbose --test-args="--gtest_filter=debounce_average.*"
```

## 测试注意事项

1. **meson test 命令格式**: meson 构建配置中只有一个测试注册（`libmcpp unit tests`），要传递参数给测试可执行文件，使用 `--test-args` 选项。格式：`meson test -C builddir --test-args="--gtest_filter=..."`。注意不要在 `--test-args` 中使用引号包裹整个参数，gtest_filter 的值可以用引号。
2. **直接运行可执行文件**: 推荐直接运行 `./builddir/tests/libmcpp_test`，这样更简单直接，不需要通过 meson。
3. **测试隔离性**: 所有测试用例都是独立的，不会相互影响。每个测试用例都会创建新的防抖器实例。
4. **异常处理**: 所有无效参数测试都会验证抛出正确的异常类型（`std::runtime_error`）。
5. **有符号模式**: 在测试有符号模式时，需要注意输入值 > 127 会被视为负数（按补码处理）。
6. **窗口大小**: 平均值和中位数防抖在窗口大小 < 3 时，不会去掉极值，直接计算平均值。
7. **状态清除**: `clear_debounce_val()` 方法会清除所有积累的数据，清除后需要重新积累数据才能返回结果。
