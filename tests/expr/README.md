# 表达式引擎测试

本目录维护 `mc::expr` 模块的单元测试。2025‑11 我们清理了重复和示例性质的用例（例如 `ParserInternalTest.*`、额外的 builtin 冒烟测试），确保只保留能直接验证公共 API 的测试套件。

## 测试优化说明

### 测试用例统计

- **测试文件总数**: 13 个
- **测试用例总数**: 约 238 个（已优化，删除了重复的测试用例）

## 目录概览

```
tests/expr/
├── test_expr.cpp              # 表达式求值端到端
├── test_lexer.cpp             # 词法分析
├── test_parser.cpp            # 语法分析
├── test_node.cpp              # AST 节点/visitor
├── test_context.cpp           # 作用域与对象上下文
├── test_object_expr.cpp       # 对象表达式
├── test_gvariant_evaluate.cpp # GVariant 互操作
├── builtin/                   # math/string/conversion 等内置模块
└── function/                  # 函数解析、relate property、func call
```

新增测试时请放入对应的功能目录，避免在 `test_expr.cpp` 中堆叠零散场景。

## 运行方式

```bash
# 在 builddir 中运行全部 expr 测试
meson test -C builddir --test-args="--gtest_filter=expr_*"

# 或直接运行测试可执行文件
./builddir/tests/libmcpp_test --gtest_filter="FunctionParserTest.*"
```

## 维护约定

1. **避免重复**：提交前确认现有文件是否已验证同一行为。
2. **聚焦核心**：只保留验证公共 API 的用例，示例/教程迁移到文档。
3. **可读性**：每个测试须包含中文注释说明场景与期望。
4. **同步文档**：目录或策略调整需要更新本 README 并在提交说明中注明原因。

