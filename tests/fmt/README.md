# Fmt 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Fmt 模块的测试用例。Fmt 模块是 MC++ 库的格式化模块，提供了格式化核心功能（`format_parser`、`format_context`、`format_dict`、`formatter_chrono` 等）、`mc::variant` 格式化、STL 容器扩展、DICT 占位符、C 风格 `format_v/format_vv`、命名参数和智能占位符等功能。

## 测试文件列表

Fmt 模块包含以下测试文件：

1. `test_format.cpp` - 格式化核心功能测试
2. `test_format_dict.cpp` - DICT 占位符格式化测试
3. `test_format_errors.cpp` - 格式化错误处理测试
4. `test_chrono.cpp` - 时间格式化测试
5. `test_format_std.cpp` - STL 容器格式化测试
6. `test_mc_formatter.cpp` - MC++ 类型格式化测试
7. `test_named_args.cpp` - 命名参数测试
8. `test_smart_placeholder.cpp` - 智能占位符测试
9. `test_format_v.cpp` - C 风格格式化函数测试
10. `compatibility_test.cpp` - 兼容性测试（可选，需要开启选项）

## 详细测试用例

### test_format.cpp
格式化核心功能测试（约 50+ 个用例）：
- `basic_formatting` - 测试基础格式化
- `alignment` - 测试对齐
- `sign_handling` - 测试符号处理
- `special_values` - 测试特殊值
- `zero_padding` - 测试零填充
- `zero_padding_with_alignment` - 测试带对齐的零填充
- `zero_padding_preserves_sign_column` - 测试零填充保留符号列
- `precision` - 测试精度
- `precision_missing_digits_fallback` - 测试精度缺失数字回退
- `rounding_behavior` - 测试舍入行为
- `large_numbers` - 测试大数字
- `remove_trailing_zeros` - 测试移除尾随零
- `complex_formatting` - 测试复杂格式化
- `limits` - 测试边界值
- `nan_inf_combinations` - 测试 NaN 和 Inf 组合
- `negative_zero` - 测试负零
- `scientific` - 测试科学计数法
- `hex_float` - 测试十六进制浮点数
- `default_scientific_for_tiny_values` - 测试极小值的默认科学计数法
- `alternate_g_format_zero_padding` - 测试交替 g 格式零填充
- `hex_float_alternate_removes_prefix` - 测试十六进制浮点数交替格式移除前缀
- `string_precision_with_alignment` - 测试字符串精度和对齐
- `dynamic_width_precision` - 测试动态宽度和精度
- `align_fill` - 测试对齐填充
- `FormatSpecAlignmentAtStringEnd` - 测试格式规范在字符串末尾的对齐
- `invalid_spec` - 测试无效规范
- `multi_args` - 测试多参数
- `no_args` - 测试无参数
- `integer_format` - 测试整数格式化
- `FormatIntegerUppercaseBinary` - 测试整数大写二进制格式
- `integer_format_types` - 测试整数格式类型
- `string_format` - 测试字符串格式化
- `pointer_format` - 测试指针格式化
- `pointer_alignment_and_invalid_type` - 测试指针对齐和无效类型
- `char_format` - 测试字符格式化
- `char_invalid_specifier_throws` - 测试字符无效说明符抛出异常
- `bool_format` - 测试布尔格式化
- `index_args` - 测试索引参数
- `runtim_error_handling` - 测试运行时错误处理
- `compile_check` - 测试编译检查
- `FormatCompileArgVisit` - 测试编译参数访问
- `FormatDirectOutputBufOverflowEof` - 测试直接输出缓冲区溢出 EOF

### test_format_dict.cpp
DICT 占位符格式化测试（9 个用例）：
- `named_format_use_dict` - 测试使用 dict 的命名格式
- `FormatWithDictTest` - 测试使用 dict 格式化
- `FormatIcaseTest` - 测试大小写无关格式化
- `DynamicWidthAndPrecisionFromDict` - 测试从 dict 获取动态宽度和精度
- `DynamicParameterTypeError` - 测试动态参数类型错误
- `AppendIcaseOverload` - 测试追加大小写无关重载
- `DynamicWidthFromFloatingEntry` - 测试从浮点条目获取动态宽度
- `RuntimeStoreFallbacks` - 测试运行时存储回退
- `DictNonStringKeyFallback` - 测试 dict 非字符串键回退

### test_format_errors.cpp
格式化错误处理测试（13 个用例）：
- `missing_closing_brace` - 测试缺少闭合大括号
- `invalid_named_parameter` - 测试无效命名参数
- `invalid_index_parameter` - 测试无效索引参数
- `stray_closing_brace` - 测试多余闭合大括号
- `missing_named_argument` - 测试缺少命名参数
- `missing_positional_argument` - 测试缺少位置参数
- `invalid_format_spec` - 测试无效格式规范
- `invalid_dynamic_parameter` - 测试无效动态参数
- `collect_named_arguments` - 测试收集命名参数
- `collect_named_arguments_failed` - 测试收集命名参数失败
- `RuntimeParserErrors` - 测试运行时解析器错误
- `FormatParserPlaceholderContentInvalidStart` - 测试格式解析器占位符内容无效起始
- `FormatCompileArgParseCustomFailure` - 测试格式编译参数解析自定义失败

### test_chrono.cpp
时间格式化测试（20 个用例）：
- `duration_basic` - 测试 duration 基础功能
- `duration_precision` - 测试 duration 精度
- `duration_negative` - 测试负 duration
- `duration_time_format` - 测试 duration 时间格式
- `duration_value_unit` - 测试 duration 值和单位
- `duration_subsecond` - 测试 duration 亚秒
- `time_point_basic` - 测试 time_point 基础功能
- `time_point_custom_format` - 测试 time_point 自定义格式
- `time_point_time_format` - 测试 time_point 时间格式
- `time_point_subsecond` - 测试 time_point 亚秒
- `format_parse_errors` - 测试格式解析错误
- `padding_and_modifiers` - 测试填充和修饰符
- `edge_cases` - 测试边界情况
- `container_formatting` - 测试容器格式化
- `timezone_formatting` - 测试时区格式化
- `timezone_name_formatting` - 测试时区名称格式化
- `duration_uncommon_fields` - 测试 duration 不常见字段
- `Duration12HourFormat` - 测试 12 小时格式 duration
- `TimePointTimezoneTokens` - 测试 time_point 时区标记
- `TimePoint12HourFormat` - 测试 12 小时格式 time_point

### test_format_std.cpp
STL 容器格式化测试（约 35 个用例）：
- `vector_basic` - 测试 vector 基础功能
- `list_basic` - 测试 list 基础功能
- `set_basic` - 测试 set 基础功能
- `map_basic` - 测试 map 基础功能
- `nested_vector` - 测试嵌套 vector
- `nested_map_vector` - 测试嵌套 map vector
- `vector_int` - 测试 int vector
- `list_string` - 测试 string list
- `deque_double` - 测试 double deque
- `set_int` - 测试 int set
- `unordered_set_int` - 测试 int unordered_set
- `map_nested_container` - 测试嵌套容器 map
- `unordered_map_str_int` - 测试 string-int unordered_map
- `array_int` - 测试 int array
- `pair_int_str` - 测试 int-string pair
- `tuple_mixed` - 测试混合类型 tuple
- `optional_int` - 测试 int optional
- `variant_basic` - 测试 variant 基础功能
- `bitset_basic` - 测试 bitset 基础功能
- `unique_ptr_basic` - 测试 unique_ptr 基础功能
- `shared_ptr_basic` - 测试 shared_ptr 基础功能
- `weak_ptr_basic` - 测试 weak_ptr 基础功能
- `string_view_basic` - 测试 string_view 基础功能
- `pair_char_int` - 测试 char-int pair
- `tuple_char_string` - 测试 char-string tuple
- `vector_char` - 测试 char vector
- `vector_string` - 测试 string vector
- `set_string` - 测试 string set
- `optional_string` - 测试 string optional
- `optional_char` - 测试 char optional
- `variant_char_string` - 测试 char-string variant
- `pair_string_view_int` - 测试 string_view-int pair
- `tuple_unsigned_char` - 测试 unsigned char tuple
- `signed_char_test` - 测试 signed char

### test_mc_formatter.cpp
MC++ 类型格式化测试（13 个用例）：
- `VariantFormatting` - 测试 variant 格式化
- `VariantFormatWithSpec` - 测试带规范的 variant 格式化
- `FormatCharFromVariant` - 测试从 variant 格式化字符
- `BlobFormatterOutputsSize` - 测试 Blob 格式化器输出大小
- `ExtensionFormatterUsesTypeName` - 测试扩展格式化器使用类型名
- `VariantFormatFallback` - 测试 variant 格式化回退
- `VariantsFormatting` - 测试 variants 格式化
- `DictFormatting` - 测试 dict 格式化
- `MutableDictFormatting` - 测试 mutable_dict 格式化
- `NestedStructureFormatting` - 测试嵌套结构格式化
- `VariantWithArrayFormatting` - 测试带数组的 variant 格式化
- `VariantWithDictFormatting` - 测试带 dict 的 variant 格式化
- `ComplexNestedFormatting` - 测试复杂嵌套格式化

### test_named_args.cpp
命名参数测试（10 个用例）：
- `basic_named_args` - 测试基础命名参数
- `error_handling` - 测试错误处理
- `nested_braces` - 测试嵌套大括号
- `test_named_args_with_format_spec` - 测试带格式规范的命名参数
- `test_various_types` - 测试各种类型
- `test_string_types` - 测试字符串类型
- `test_char_type` - 测试字符类型
- `test_escaped_braces` - 测试转义大括号
- `test_nocopyable` - 测试不可复制类型
- `test_mixed_format` - 测试混合格式

### test_smart_placeholder.cpp
智能占位符测试（9 个用例）：
- `basic_smart_placeholder` - 测试基础智能占位符
- `mixed_placeholder_types` - 测试混合占位符类型
- `format_specifications` - 测试格式规范
- `dynamic_format_parameters` - 测试动态格式参数
- `error_handling` - 测试错误处理
- `SmartPlaceholderEmptyIndexString` - 测试智能占位符空索引字符串
- `edge_cases` - 测试边界情况
- `nested_braces` - 测试嵌套大括号
- `escaped_braces` - 测试转义大括号

### test_format_v.cpp
C 风格格式化函数测试（4 个用例）：
- `c_style_variadic_string` - 测试 C 风格可变参数字符串
- `c_style_variadic_to_buffer` - 测试 C 风格可变参数到缓冲区
- `format_vv_string_and_buffer` - 测试 format_vv 字符串和缓冲区
- `format_vv_empty_format_returns_false` - 测试 format_vv 空格式返回 false

### compatibility_test.cpp
兼容性测试（约 30 个用例，可选，需要开启选项）：
- `basic_formatting` - 测试基础格式化
- `alignment` - 测试对齐
- `sign_handling` - 测试符号处理
- `special_values` - 测试特殊值
- `zero_padding` - 测试零填充
- `zero_padding_with_alignment` - 测试带对齐的零填充
- `precision` - 测试精度
- `rounding_behavior` - 测试舍入行为
- `large_numbers` - 测试大数字
- `remove_trailing_zeros` - 测试移除尾随零
- `complex_formatting` - 测试复杂格式化
- `limits` - 测试边界值
- `nan_inf_combinations` - 测试 NaN 和 Inf 组合
- `zero_values` - 测试零值
- `scientific` - 测试科学计数法
- `hex_float` - 测试十六进制浮点数
- `dynamic_width_precision` - 测试动态宽度和精度
- `align_fill` - 测试对齐填充
- `integer_format` - 测试整数格式化
- `integer_format_types` - 测试整数格式类型
- `string_format` - 测试字符串格式化
- `pointer_format` - 测试指针格式化
- `char_format` - 测试字符格式化
- `sign_handling_integers` - 测试整数符号处理
- `zero_padding_integers` - 测试整数零填充
- `alternate_form` - 测试交替形式
- `bool_format` - 测试布尔格式化

## 运行测试

### 运行所有 fmt 模块测试

```bash
# 运行所有模块测试
meson test -C builddir

# 或者使用 ninja
ninja -C builddir test
```

**注意**: 
- meson 构建配置中只有一个测试注册（`libmcpp unit tests`），不能使用 `--suite fmt` 运行特定模块测试
- 要通过 meson 传递参数给测试可执行文件，使用 `--test-args` 选项
- 推荐直接运行可执行文件，更简单直接

### 运行特定测试文件

```bash
# 测试格式化核心功能（所有用例）- 直接运行可执行文件（推荐）
./builddir/tests/libmcpp_test --gtest_filter="format_test.*"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=format_test.*"

# 测试 DICT 占位符格式化（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="format_dict_test.*"
meson test -C builddir --test-args="--gtest_filter=format_dict_test.*"

# 测试格式化错误处理（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="format_error_test.*"
meson test -C builddir --test-args="--gtest_filter=format_error_test.*"

# 测试时间格式化（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="chrono_format_test.*"
meson test -C builddir --test-args="--gtest_filter=chrono_format_test.*"

# 测试 STL 容器格式化（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="format_std_test.*"
meson test -C builddir --test-args="--gtest_filter=format_std_test.*"

# 测试 MC++ 类型格式化（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="mc_formatter_test.*"
meson test -C builddir --test-args="--gtest_filter=mc_formatter_test.*"

# 测试命名参数（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="named_args.*:named_args_test.*"
meson test -C builddir --test-args="--gtest_filter=named_args.*:named_args_test.*"

# 测试智能占位符（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="smart_placeholder_test.*"
meson test -C builddir --test-args="--gtest_filter=smart_placeholder_test.*"

# 测试 C 风格格式化函数（所有用例）
./builddir/tests/libmcpp_test --gtest_filter="format_v_test.*"
meson test -C builddir --test-args="--gtest_filter=format_v_test.*"

# 测试兼容性（所有用例，需要开启选项）
meson configure builddir -Dcompatibility_tests=true
ninja -C builddir mc_format_test std_format_test
./builddir/tests/mc_format_test --gtest_filter="compatibility_test.*"
./builddir/tests/std_format_test --gtest_filter="compatibility_test.*"
```

### 运行特定测试用例

```bash
# 使用 gtest 过滤器运行特定用例（直接运行可执行文件，推荐）
./builddir/tests/libmcpp_test --gtest_filter="format_test.basic_formatting"

# 或通过 meson 传递参数
meson test -C builddir --test-args="--gtest_filter=format_test.basic_formatting"

# 运行多个测试用例（使用通配符）
./builddir/tests/libmcpp_test --gtest_filter="format_test.*formatting"
meson test -C builddir --test-args="--gtest_filter=format_test.*formatting"
```

## 测试统计

- **测试文件总数**: 10 个（包含 1 个可选的兼容性测试文件）
- **测试用例总数**: 约 180+ 个
  - `test_format.cpp`: 约 50+ 个
  - `test_format_dict.cpp`: 9 个
  - `test_format_errors.cpp`: 13 个
  - `test_chrono.cpp`: 20 个
  - `test_format_std.cpp`: 约 35 个
  - `test_mc_formatter.cpp`: 13 个
  - `test_named_args.cpp`: 10 个
  - `test_smart_placeholder.cpp`: 9 个
  - `test_format_v.cpp`: 4 个
  - `compatibility_test.cpp`: 约 30 个（可选，需要开启选项）
- **测试覆盖的功能模块**:
  - Format Parser（格式解析器）
  - Format Context（格式上下文）
  - Format Dict（格式字典）
  - Formatter Chrono（时间格式化器）
  - MC++ Type Formatters（MC++ 类型格式化器）
  - Named Arguments（命名参数）
  - Smart Placeholder（智能占位符）
  - C-style Format Functions（C 风格格式化函数）
  - STL Container Formatters（STL 容器格式化器）

## 代码覆盖率

当前测试覆盖率持续改进中。新增测试用例时，重点关注以下未覆盖的代码路径：

1. **动态参数解析**：`test_format.cpp` 新增"动态宽度来源为浮点"与"动态参数类型非法"场景，直接覆盖 `resolve_dynamic_param` 的成功/失败路径。
2. **DICT 占位符**：`test_format_dict.cpp` 补充 `format_dict_icase` 追加重载、dict 提供浮点宽度、`runtime_arg_store` 大小写匹配/缺失参数分支。
3. **时间格式化**：`test_chrono.cpp` 通过 `sformat_unsafe` 构造包含 `%Y/%y/%m/%d/%z/%Z` 的 duration 模式，触发 `formatter_chrono` 里先前未命中的兜底逻辑。

### 调试测试

```bash
# 使用 gdb 调试测试
gdb --args ./builddir/tests/libmcpp_test --gtest_filter="format_test.basic_formatting"

# 启用详细输出（直接运行可执行文件）
./builddir/tests/libmcpp_test --gtest_filter="format_test.*" --verbose

# 或通过 meson 启用详细输出
meson test -C builddir --verbose --test-args="--gtest_filter=format_test.*"
```

## 测试注意事项

1. **meson test 命令格式**: meson 构建配置中只有一个测试注册（`libmcpp unit tests`），要传递参数给测试可执行文件，使用 `--test-args` 选项。格式：`meson test -C builddir --test-args="--gtest_filter=..."`。注意不要在 `--test-args` 中使用引号包裹整个参数，gtest_filter 的值可以用引号。
2. **直接运行可执行文件**: 推荐直接运行 `./builddir/tests/libmcpp_test`，这样更简单直接，不需要通过 meson。
3. **兼容性测试**: `compatibility_test.cpp` 是可选测试，需要开启 `-Dcompatibility_tests=true` 选项才能编译和运行。该测试用于对比 MC++ 格式化与 `std::format` 的行为一致性。
4. **测试隔离**: 测试用例之间相互独立，每个测试用例都会正确初始化和清理资源。
5. **格式化规范**: 测试用例覆盖了各种格式化规范，包括对齐、填充、精度、符号处理等。
6. **错误处理**: `test_format_errors.cpp` 专门测试各种错误场景，包括语法错误、参数缺失、类型错误等。
7. **时间格式化**: `test_chrono.cpp` 测试了 duration、time_point 和时区的格式化，包括各种时间格式标记。
8. **STL 容器**: `test_format_std.cpp` 测试了各种 STL 容器的格式化，包括 vector、list、set、map、optional、variant 等。
9. **MC++ 类型**: `test_mc_formatter.cpp` 测试了 MC++ 特有类型的格式化，包括 variant、dict、mutable_dict 等。
10. **命名参数和智能占位符**: `test_named_args.cpp` 和 `test_smart_placeholder.cpp` 测试了高级格式化功能。
11. **新增用例前先查重**: 若覆盖的函数/场景已在文档中列出，优先扩展原文件而非新建。
12. **复杂场景优先**: 结合多参数、动态宽度、dict 与 chrono 的交叉使用，最大化代码路径覆盖。
13. **文档同步**: 每次补充新的"未命中"分支测试时，请在文档中记录，便于后续审核。
