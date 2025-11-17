# 表达式引擎模块测试 (expr)

## 概述

本目录包含表达式引擎模块 (`expr`) 的完整测试套件，覆盖了从词法分析、语法分析、AST 节点求值到表达式引擎整体功能的各个方面。

## 测试统计

- **总测试用例数**: 206 个
- **测试文件数**: 12 个
- **测试覆盖范围**: 词法分析、语法分析、AST 节点、上下文管理、内置函数、函数调用、对象表达式、GVariant 求值

## 测试文件结构

```
tests/expr/
├── test_expr.cpp              # 表达式引擎整体功能测试 (18个测试用例)
├── test_lexer.cpp             # 词法分析器测试 (15个测试用例)
├── test_parser.cpp            # 语法分析器测试 (14个测试用例)
├── test_node.cpp              # AST 节点测试 (63个测试用例)
├── test_context.cpp           # 上下文管理测试 (20个测试用例)
├── test_object_expr.cpp       # 对象表达式测试 (5个测试用例)
├── test_gvariant_evaluate.cpp # GVariant 求值测试 (3个测试用例)
├── builtin/                   # 内置函数测试
│   ├── test_math.cpp          # 数学函数测试 (6个测试用例)
│   ├── test_string.cpp        # 字符串函数测试 (6个测试用例)
│   └── test_conversion.cpp    # 类型转换函数测试 (4个测试用例)
└── function/                  # 函数相关测试
    ├── test_call.cpp          # 函数调用测试 (12个测试用例)
    └── test_parser.cpp        # 函数解析测试 (40个测试用例)
```

## 测试文件说明

### 1. test_expr.cpp - 表达式引擎整体功能测试

测试表达式引擎的核心功能，包括：

- **基本算术运算** (`BasicArithmetic`): 整数和浮点数的加减乘除、取模运算
- **比较运算** (`Comparison`): 各种比较运算符和复合比较
- **字符串操作** (`StringOperations`): 字符串连接和比较
- **变量** (`Variables`): 变量注册、使用和从 dict 导入
- **函数** (`Functions`): 内置函数和自定义函数调用
- **错误处理** (`Errors`): 各种错误场景
- **位运算** (`BitOperations`): 按位与、或、异或、取反、移位
- **模板字符串** (`TemplateStrings`): 模板字符串的求值
- **一元运算符** (`UnaryOperators`): 负号、逻辑非、按位取反
- **运算符结合性** (`operator_associativity`): 运算符的左右结合性
- **浮点精度** (`floating_point_precision`): 浮点数精度处理
- **字符串转义** (`string_escaping`): 字符串转义字符处理
- **大数处理** (`large_numbers`): 大整数边界值处理
- **边界值** (`boundary_values`): 各种数据类型的边界值
- **类型转换** (`type_conversion`): 类型自动转换
- **复杂嵌套表达式** (`complex_nested_expressions`): 复杂表达式的求值
- **更多错误场景** (`more_error_scenarios`): 额外的错误处理测试
- **条件表达式** (`conditional_expression`): 三元运算符

### 2. test_lexer.cpp - 词法分析器测试

测试词法分析器将表达式字符串转换为 token 序列的功能：

- **十进制数字** (`DecimalNumbers`): 整数和浮点数字面值
- **十六进制数字** (`HexadecimalNumbers`): 0x 前缀的十六进制数
- **二进制数字** (`BinaryNumbers`): 0b 前缀的二进制数
- **八进制数字** (`OctalNumbers`): 0 前缀的八进制数
- **错误情况** (`ErrorCases`): 无效的数字格式、未闭合的字符串等
- **模板字符串** (`TemplateString`): 模板字符串的 token 化
- **更多数字格式** (`MoreNumberFormats`): 科学计数法等
- **标识符** (`Identifiers`): 变量名和函数名
- **布尔字面值** (`BooleanLiterals`): true 和 false
- **字符串字面值** (`StringLiterals`): 单引号和双引号字符串
- **运算符** (`Operators`): 各种运算符的识别
- **分隔符** (`Delimiters`): 括号、逗号等分隔符
- **空白字符** (`Whitespace`): 空格、制表符、换行符的处理
- **更多错误情况** (`MoreErrorCases`): 额外的词法错误
- **更多模板字符串场景** (`MoreTemplateStringScenarios`): 复杂的模板字符串场景

### 3. test_parser.cpp - 语法分析器测试

测试语法分析器将 token 序列解析为 AST 的功能：

- **条件表达式** (`conditional_expression`): 三元运算符解析
- **逻辑运算符** (`logical_operators`): && 和 || 的解析
- **位运算符** (`bit_operators`): 位运算符的解析
- **比较运算符** (`comparison_operators`): 比较运算符的解析
- **运算符优先级** (`operator_precedence`): 运算符优先级处理
- **一元运算符** (`unary_operators`): 一元运算符的解析
- **括号表达式** (`parenthesized_expression`): 括号改变优先级
- **函数调用** (`function_call`): 函数调用的解析
- **属性和方法访问** (`property_and_method_access`): 属性访问和方法调用
- **模板字符串** (`template_string`): 模板字符串的解析
- **解析错误** (`parse_errors`): 语法错误的处理
- **复杂表达式** (`complex_expressions`): 复杂表达式的解析
- **边界情况** (`edge_cases`): 边界情况的处理
- **解析器边界检查** (`parser_boundary_checks`): 索引越界等边界检查

### 4. test_node.cpp - AST 节点测试

测试各种 AST 节点的求值和功能：

#### 字面值节点 (literal_node)
- **求值** (`literal_node_evaluate`): 各种类型字面值的求值
- **各种类型** (`literal_node_various_types`): 数组、dict、null 等类型

#### 变量节点 (variable_node)
- **求值** (`variable_node_evaluate`): 变量存在和不存在的情况
- **访问器** (`variable_node_accessors_through_usage`): 通过使用测试访问器

#### 一元运算符节点 (unary_op_node)
- **负号** (`unary_op_neg`): 负号运算符
- **逻辑非** (`unary_op_not`): 逻辑非运算符
- **按位取反** (`unary_op_bit_not`): 按位取反运算符
- **字符串表示** (`unary_op_to_string`): to_string() 方法
- **访问器** (`unary_op_node_accessors_through_usage`): 通过使用测试访问器

#### 二元运算符节点 (binary_op_node)
- **算术运算** (`binary_op_arithmetic`): 加减乘除取模
- **比较运算** (`binary_op_comparison`): 各种比较运算符
- **逻辑短路** (`binary_op_logical_short_circuit`): && 和 || 的短路求值
- **位运算** (`binary_op_bitwise`): 位运算符
- **字符串连接** (`binary_op_string_concatenation`): 字符串连接
- **浮点数比较** (`binary_op_float_comparison`): 浮点数比较
- **所有运算符字符串表示** (`binary_op_all_operators_to_string`): 所有运算符的 to_string()
- **字符串表示** (`binary_op_to_string`): to_string() 方法
- **访问器** (`binary_op_node_accessors_through_usage`): 通过使用测试访问器

#### 条件节点 (conditional_node)
- **真分支** (`conditional_node_true_branch`): 条件为真时的求值
- **假分支** (`conditional_node_false_branch`): 条件为假时的求值
- **各种条件类型** (`conditional_node_various_conditions`): 0、非零、空字符串等
- **访问器** (`conditional_node_accessors_through_usage`): 通过使用测试访问器

#### 函数调用节点 (function_call_node)
- **无参数** (`function_call_node_no_args`): 无参数函数调用
- **有参数** (`function_call_node_with_args`): 有参数函数调用
- **未定义函数** (`function_call_node_undefined`): 未定义函数的错误处理
- **参数求值顺序** (`function_call_node_arg_evaluation_order`): 参数求值顺序
- **字符串表示** (`function_call_node_to_string`): to_string() 方法
- **访问器** (`function_call_node_accessors_through_usage`): 通过使用测试访问器

#### 属性访问节点 (property_access_node)
- **变量类型为 dict** (`property_access_variable_type_dict`): dict 类型属性访问
- **链式访问** (`property_access_chain`): 多级属性访问
- **变量未找到** (`property_access_variable_type_not_found`): 变量不存在的错误
- **链式访问未找到** (`property_access_chain_not_found`): 链式访问中属性不存在
- **链式访问非对象** (`property_access_chain_not_object`): 链式访问中中间节点不是对象
- **无效对象类型** (`property_access_invalid_object_type`): 不支持的对象类型
- **属性未找到** (`property_access_not_found`): 属性不存在的错误
- **dict 各种类型** (`property_access_dict_various_types`): dict 中各种类型的属性
- **字符串表示** (`property_access_to_string`): to_string() 方法
- **访问器** (`property_access_node_accessors_through_usage`): 通过使用测试访问器

#### 对象方法调用节点 (object_method_call_node)
- **函数未找到** (`object_method_call_node_function_not_found`): 方法不存在的错误
- **无效对象类型** (`object_method_call_node_invalid_object`): 不支持的对象类型
- **字符串表示** (`object_method_call_node_to_string`): to_string() 方法

#### 模板字符串节点 (template_string_node)
- **简单模板** (`template_string_node_simple`): 简单模板字符串
- **带表达式** (`template_string_node_with_expression`): 包含表达式的模板字符串
- **多个表达式** (`template_string_node_multiple_expressions`): 包含多个表达式的模板字符串
- **访问器** (`template_string_node_accessors_through_usage`): 通过使用测试访问器

#### 上下文相关测试
- **移动构造** (`context_move_constructor`): 上下文的移动构造函数
- **移动赋值** (`context_move_assignment`): 上下文的移动赋值运算符
- **带对象获取变量** (`context_get_variable_with_object`): 通过对象名获取变量
- **带对象获取变量未找到** (`context_get_variable_with_object_not_found`): 对象变量不存在
- **带对象获取变量对象不存在** (`context_get_variable_with_nonexistent_object`): 对象不存在
- **带对象判断变量存在** (`context_has_variable_with_object`): 通过对象名判断变量存在
- **带对象判断函数存在** (`context_has_function_with_object`): 通过对象名判断函数存在
- **带对象调用函数** (`context_invoke_with_object`): 通过对象名调用函数
- **从 dict 导入** (`context_import_from_dict`): 从 dict 导入变量
- **从 dict 导入非字符串键** (`context_import_from_dict_non_string_key`): 非字符串键的处理
- **注册函数返回 ID** (`context_register_function_returns_id`): 注册函数返回 ID
- **注册变量返回 ID** (`context_register_variable_returns_id`): 注册变量返回 ID
- **注册函数 nullptr** (`context_register_function_nullptr`): 注册 nullptr 函数的错误处理

#### 解析器错误测试
- **解析错误异常** (`parser_parse_error_exceptions`): 各种解析错误异常
- **条件表达式缺少冒号** (`parser_conditional_missing_colon`): 三元运算符缺少冒号
- **属性访问缺少名称** (`parser_property_access_missing_name`): 属性访问缺少属性名
- **函数调用缺少右括号** (`parser_function_call_missing_right_paren`): 函数调用缺少右括号
- **模板字符串未闭合** (`parser_template_string_unclosed`): 模板字符串未闭合

### 5. test_context.cpp - 上下文管理测试

测试上下文的作用域、继承和变量/函数管理：

- **作用域继承** (`ScopeInheritance`): 子作用域访问父作用域的变量和函数
- **多级作用域** (`MultiLevelScope`): 多级作用域的变量查找
- **变量遮蔽** (`VariableShadowing`): 子作用域变量遮蔽父作用域变量
- **引擎作用域** (`EngineWithScope`): 引擎与作用域的集成
- **从 dict 创建上下文** (`CreateContextWithDict`): 使用 dict 初始化上下文
- **设置父级** (`SetParent`): 动态设置父级上下文
- **函数注册** (`function_registration`): 函数注册和管理
- **变量注册** (`variable_registration`): 变量注册和管理
- **从 dict 导入** (`import_from_dict`): 从 dict 批量导入变量
- **上下文拷贝和移动** (`context_copy_and_move`): 上下文的拷贝和移动语义
- **上下文基类默认行为** (`context_base_default_behavior`): context_base 的默认实现
- **上下文基类拷贝和移动** (`context_base_copy_and_move`): context_base 的拷贝和移动构造函数/赋值运算符
- **注册对象** (`register_object`): 注册引擎对象到上下文，支持空对象检查
- **从对象获取变量** (`get_variable_from_object`): 通过对象符号访问对象属性
- **从对象检查变量存在** (`has_variable_from_object`): 通过对象符号检查属性是否存在
- **从对象检查函数存在** (`has_function_from_object`): 通过对象符号检查方法是否存在，包括对象不存在的情况
- **从对象调用函数** (`invoke_from_object`): 通过对象符号调用方法，包括对象不存在的情况
- **对象上下文获取对象** (`object_context_get_object`): object_context::get_object() 方法
- **从变量符号获取变量** (`get_variable_from_variable_symbol`): 通过变量符号（dict 类型）访问属性
- **从变量符号检查变量存在** (`has_variable_from_variable_symbol`): 通过变量符号（dict 类型）检查属性是否存在

### 6. test_object_expr.cpp - 对象表达式测试

测试引擎对象的属性访问和方法调用：

- **读取属性** (`test_read_properties`): 通过表达式读取对象属性（支持接口限定符）
- **调用方法** (`test_call_methods`): 通过表达式调用对象方法（支持接口限定符）
- **设置属性** (`test_set_properties`): 通过表达式设置对象属性
- **复杂表达式** (`test_complex_expressions`): 包含对象访问的复杂表达式
- **错误处理** (`test_error_handling`): 对象表达式相关的错误处理

### 7. test_gvariant_evaluate.cpp - GVariant 求值测试

测试表达式求值结果转换为 GVariant 类型：

- **基本类型** (`BasicTypes`): 整数、浮点数、字符串、布尔值的 GVariant 转换
- **算术运算** (`ArithmeticOperations`): 算术运算结果的 GVariant 转换
- **布尔运算** (`BooleanOperations`): 布尔运算结果的 GVariant 转换

### 8. builtin/ - 内置函数测试

#### test_math.cpp - 数学函数测试
- **abs** (`AbsFunction`): 绝对值函数
- **min** (`MinFunction`): 最小值函数
- **max** (`MaxFunction`): 最大值函数
- **pow** (`PowFunction`): 幂运算函数
- **sqrt** (`SqrtFunction`): 平方根函数
- **floor** (`FloorFunction`): 向下取整函数

#### test_string.cpp - 字符串函数测试
- **length** (`LengthFunction`): 字符串长度函数
- **concat** (`ConcatFunction`): 字符串连接函数
- **substring** (`SubstringFunction`): 子字符串函数
- **upper** (`UpperFunction`): 转大写函数
- **lower** (`LowerFunction`): 转小写函数
- **trim** (`TrimFunction`): 去除首尾空白字符函数

#### test_conversion.cpp - 类型转换函数测试
- **toString** (`ToStringFunction`): 转换为字符串
- **toBool** (`ToBoolFunction`): 转换为布尔值
- **toInt** (`ToIntFunction`): 转换为整数
- **toDouble** (`ToDoubleFunction`): 转换为浮点数

### 9. function/ - 函数相关测试

#### test_call.cpp - 函数调用测试
- **基本使用** (`BasicUsage`): 函数调用的基本用法
- **获取关联属性基本** (`GetRelatePropertiesBasic`): 获取关联属性的基本用法
- **获取关联属性带接口** (`GetRelatePropertiesWithInterface`): 带接口限定符的关联属性
- **获取关联属性混合语法** (`GetRelatePropertiesMixedSyntax`): 混合语法格式
- **判断关联属性带接口** (`IsRelatePropertyWithInterface`): 判断是否为关联属性
- **获取关联属性嵌套** (`GetRelatePropertiesNested`): 嵌套的关联属性
- **获取关联属性混合** (`GetRelatePropertiesMixed`): 混合类型的关联属性
- **获取关联属性空** (`GetRelatePropertiesEmpty`): 空关联属性
- **判断关联属性函数** (`IsRelatePropertyFunction`): 判断关联属性的函数
- **判断函数调用函数** (`IsFunctionCallFunction`): 判断函数调用的函数
- **复杂嵌套场景** (`ComplexNestedScenario`): 复杂的嵌套函数调用场景
- **错误处理和边界情况** (`ErrorHandlingAndEdgeCases`): 错误处理和边界情况

#### test_parser.cpp - 函数解析测试
- **属性解析** (`PropertyTest`): 基本属性、同步属性、引用属性的解析
- **函数调用解析** (`FunctionParserTest`): 各种函数调用格式的解析
- **关联属性测试** (`RelatePropertyTest`): variant 转换、空 variant、部分转换等
- **属性解析器测试** (`PropertyParserTest`): 不同属性类型、复杂属性名、错误情况
- **边界情况** (`ParseEdgeCases`): 解析器的边界情况处理
- **参数类型** (`ParseParameterTypes`): 各种参数类型的解析
- **嵌套深度** (`ParseNestedDepth`): 嵌套函数调用的深度处理
- **特殊字符** (`ParseSpecialCharacters`): 特殊字符的处理
- **带接口的属性** (`ParsePropertyWithInterface`): 带接口限定符的属性解析
- **引用对象** (`ParseRefObject`): 引用对象的解析

## 运行测试

### 使用 Meson 运行

```bash
# 在项目根目录下
cd builddir
meson test expr
```

### 运行特定测试文件

```bash
# 运行所有 expr 测试
meson test expr

# 运行特定测试用例
meson test expr --gtest_filter="expr_test.BasicArithmetic"
```

### 使用 GTest 直接运行

```bash
# 编译后直接运行测试程序
./builddir/tests/libmcpp_test --gtest_filter="expr_test.*"
```

## 测试覆盖范围

### 功能覆盖

- ✅ 词法分析（Lexer）
- ✅ 语法分析（Parser）
- ✅ AST 节点求值
- ✅ 上下文管理
- ✅ 变量和函数管理
- ✅ 内置函数
- ✅ 自定义函数
- ✅ 对象表达式
- ✅ 模板字符串
- ✅ GVariant 转换
- ✅ 错误处理

### 边界情况覆盖

- ✅ 大数处理
- ✅ 浮点精度
- ✅ 类型转换
- ✅ 空值和空集合
- ✅ 错误输入
- ✅ 边界值

### 代码路径覆盖

- ✅ 正常路径
- ✅ 错误路径
- ✅ 边界路径
- ✅ 异常处理路径

## 测试原则

1. **全面性**: 覆盖所有主要功能和边界情况
2. **独立性**: 每个测试用例独立，不依赖其他测试
3. **可读性**: 测试用例命名清晰，代码易于理解
4. **可维护性**: 测试代码结构清晰，易于维护和扩展

## 注意事项

1. 测试用例使用 Google Test (GTest) 框架
2. 所有测试用例都包含在 `tests/expr/` 目录下
3. 测试文件命名遵循 `test_*.cpp` 格式
4. 测试类命名遵循 `*_test` 格式
5. 测试用例命名使用 `snake_case` 格式

## 相关文档

- [表达式引擎模块文档](../../src/expr/README.md)
- [测试框架文档](https://google.github.io/googletest/)

