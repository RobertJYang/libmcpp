/*
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* openUBMC is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*         http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

## fmt 模块测试概览
- **覆盖范围**：验证 `src/fmt` 中的字符串格式化、字典占位符、命名参数、chrono 扩展以及 C 风格 `format_v/format_vv` 接口。
- **测试结构**：所有用例位于 `tests/fmt`，统一使用 gtest；可选的 `compatibility_test.cpp` 需通过 `meson configure -Dcompatibility_tests=true` 启用。
- **新增要点**：`test_format_v.cpp` 补充了 `format_v/format_vv` 的成功与失败路径；`test_format_errors.cpp`、`test_format_dict.cpp` 等文件针对字典、命名参数和错误处理提供复杂场景。

## 运行方式
- 构建：`meson setup builddir`，如已存在可跳过。
- 执行：`ninja -C builddir test` 将运行所有单元测试；仅运行 fmt 套件可执行 `ninja -C builddir libmcpp_test && builddir/tests/libmcpp_test --gtest_filter='*fmt*'`。
- 兼容性测试：启用 `compatibility_tests` 选项后，使用 `ninja -C builddir mc_format_test` 与 `std_format_test` 分别验证 `mc::format` 与 `std::format`。

## 目录说明
- `test_format.cpp`：浮点与通用格式说明符。
- `test_format_dict.cpp`：dict 占位符与大小写无关解析。
- `test_format_errors.cpp`：`get_format_args` 以及运行期容错行为。
- `test_format_v.cpp`：C 风格 `format_v/format_vv` 覆盖。
- 其余文件覆盖 chrono、命名参数、容器与 mc::variant 等扩展格式化功能。

