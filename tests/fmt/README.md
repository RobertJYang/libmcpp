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

# fmt 模块测试概览

- **覆盖范围**：`src/fmt` 下的格式化核心（`format_parser/format_context/format_dict/formatter_chrono` 等）以及 `mc::variant`、STL 容器扩展、DICT 占位符、C 风格 `format_v/format_vv`、命名参数和智能占位符。
- **重复检查**：已重新整理测试文件，确认 `test_format.cpp`、`test_format_dict.cpp`、`test_format_errors.cpp` 等文件之间无重复场景；未来新增用例前需在 README 中登记关联模块，避免再出现重复。
- **新增要点（2025-11）**：
  - `test_format.cpp` 新增“动态宽度来源为浮点”与“动态参数类型非法”场景，直接覆盖 `resolve_dynamic_param` 的成功/失败路径。
  - `test_format_dict.cpp` 补充 `format_dict_icase` 追加重载、dict 提供浮点宽度、`runtime_arg_store` 大小写匹配/缺失参数分支。
  - `test_chrono.cpp` 通过 `sformat_unsafe` 构造包含 `%Y/%y/%m/%d/%z/%Z` 的 duration 模式，触发 `formatter_chrono` 里先前未命中的兜底逻辑。

## 运行方式

```bash
# 一次性构建
meson setup builddir

# 仅运行 fmt 相关用例
meson test -C builddir --test-args="--gtest_filter=*fmt*"

# 兼容性测试（需要开启选项）
meson configure builddir -Dcompatibility_tests=true
ninja -C builddir mc_format_test std_format_test
```

## 文件说明

| 文件 | 主要覆盖点 |
|------|------------|
| `test_format.cpp` | 浮点/整数/指针/布尔/动态宽度精度、`sformat_unsafe` 回退、异常路径 |
| `test_format_dict.cpp` | dict 占位符、大小写无关匹配、动态参数、`runtime_arg_store` 私有逻辑 |
| `test_format_errors.cpp` | `get_format_args`、宏级静态检查、运行期容错 |
| `test_chrono.cpp` | duration/time_point/时区、chrono 解析错误、兜底分支 |
| `test_format_std.cpp` | STL 容器、可选类型、智能指针 |
| `test_mc_formatter.cpp` | `mc::variant` / `mc::dict` / 嵌套结构格式化 |
| `test_named_args.cpp` / `test_smart_placeholder.cpp` | 命名参数、智能占位符语法 |
| `test_format_v.cpp` | `format_v` / `format_vv` 的成功/失败分支 |
| `compatibility_test.cpp` | 可选的 `std::format` 对齐对比（默认关闭） |

## 维护建议

1. **新增用例前先查重**：若覆盖的函数/场景已在表格列出，优先扩展原文件而非新建。
2. **复杂场景优先**：结合多参数、动态宽度、dict 与 chrono 的交叉使用，最大化代码路径覆盖。
3. **文档同步**：每次补充新的“未命中”分支测试时，请在“新增要点”中记录，便于后续审核。

