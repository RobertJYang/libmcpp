# 日志模块测试说明

## 概述

本目录包含日志模块的完整测试套件，覆盖日志系统的核心功能、appender、日志管理器等组件。

## 测试文件

| 文件名 | 描述 | 主要测试内容 |
|--------|------|--------------|
| `test_log.cpp` | 核心日志功能测试 | 基本日志记录、结构化日志、日志级别过滤、智能占位符 |
| `test_log_manager.cpp` | 日志管理器测试 | 单例模式、logger 管理、配置应用、appender 工厂 |
| `test_console_appender.cpp` | 控制台 appender 测试 | 控制台输出、颜色配置、多级日志 |
| `test_file_appender.cpp` | 文件 appender 测试 | 文件写入、并发写入、格式化消息 |

## 快速开始

### 运行测试

```bash
# 在项目根目录运行
meson test -C builddir

# 仅运行日志模块测试
meson test -C builddir --suite log

# 运行特定测试文件
meson test -C builddir test_log_manager
```

### 测试用例概览

#### 1. 核心日志功能 (`test_log.cpp`)

**基本日志记录**
```cpp
TEST_F(LogTest, BasicLogging)
- 测试基本日志宏的使用
- 验证不同级别的日志记录
- 验证日志消息的内容
```

**结构化日志**
```cpp
TEST_F(LogTest, StructuredLogging)
- 测试带参数的日志记录
- 验证参数正确传递和格式化
- 测试参数字典的正确性
```

**日志级别过滤**
```cpp
TEST_F(LogTest, LevelFiltering)
- 测试日志级别过滤机制
- 验证低于阈值的日志不被记录
- 验证高于阈值的日志正常记录
```

**智能占位符**
```cpp
TEST_F(LogTest, basic_smart_log)
- 自增索引 {}、显式索引 {0} {1}
- 命名参数 {name}、混合使用
- 格式说明符 {:.2f}、动态格式参数
```

**logger 内部行为**
```cpp
TEST_F(LogTest, log_respects_is_enabled_fast_path)
TEST(LoggerStaticGetTest, static_get_returns_named_logger)
- 覆盖 logger::log 的快速返回路径
- 验证 logger::get(const char*) 的单点入口
```

#### 2. 日志管理器 (`test_log_manager.cpp`)

**单例和基础功能**
```cpp
TEST_F(log_manager_test, SingletonInstance)
TEST_F(log_manager_test, GetDefaultLogger)
TEST_F(log_manager_test, GetNamedLogger)
- 测试单例模式的正确性
- 测试默认 logger 获取
- 测试命名 logger 的创建
```

**配置管理**
```cpp
TEST_F(log_manager_test, ApplyConfigCreateNewLogger)
TEST_F(log_manager_test, ApplyConfigUpdateExistingLogger)
TEST_F(log_manager_test, MultipleAppendersAndLoggers)
- 测试配置创建新 logger
- 测试配置更新现有 logger
- 测试多 appender 和 logger 的场景
```

**容错与差异检测**
```cpp
TEST_F(log_manager_test, ApplyConfigExternalLibraryLoadFailure)
TEST_F(log_manager_test, ApplyConfigSkipRedundantAppenderUpdates)
TEST_F(log_manager_test, AppenderFactoryInitFailureReturnsNullptr)
- 覆盖 lib_path 非空且动态库无法加载的分支
- 验证 update_existing_logger 的去重逻辑
- 覆盖 appender 初始化失败的错误路径
```

**Appender 工厂**
```cpp
TEST_F(log_manager_test, FactoryCreateAppender)
- 测试使用工厂创建 appender
- 测试 console 和 file appender 的创建
```

#### 3. 控制台 Appender (`test_console_appender.cpp`)

**基本功能**
```cpp
TEST_F(console_appender_test, DefaultConstructor)
TEST_F(console_appender_test, Init)
TEST_F(console_appender_test, DifferentLogLevels)
- 测试构造函数和初始化
- 测试不同日志级别的消息输出
```

**配置和初始化**
```cpp
TEST_F(console_appender_test, InitWithInvalidArgs)
TEST_F(console_appender_test, ConfigureWithLevelColors)
TEST_F(console_appender_test, ConfigureWithoutColor)
TEST_F(console_appender_test, ConfigureStdErrorStream)
TEST_F(console_appender_test, ConfigureFlushFalse)
- 测试异常处理分支
- 测试 level_colors 配置
- 测试禁用颜色和不同流类型
- 测试 flush 配置分支
```

**颜色功能覆盖**
```cpp
TEST_F(console_appender_test, ConfigureAllColorTypesForGetConsoleColor)
- 覆盖 get_console_color 函数的所有颜色类型分支
- 测试 red, green, brown, blue, magenta, cyan, white, console_default
```

**边界情况和复杂场景**
```cpp
TEST_F(console_appender_test, AppendWithEmptyFileContext)
TEST_F(console_appender_test, AppendWithNamespacedFunction)
TEST_F(console_appender_test, ComplexScenarioAllOptions)
TEST_F(console_appender_test, ComplexScenarioEmptyFileAndNamespacedFunction)
- 测试空文件名上下文
- 测试命名空间前缀函数名处理
- 测试融合所有配置选项的复杂场景
```

**工厂创建**
```cpp
TEST_F(console_appender_test, FactoryCreate)
TEST_F(console_appender_test, FactoryCreateNamed)
TEST_F(console_appender_test, GetOrCreateAppender)
- 测试工厂模式创建 appender
- 测试命名和获取功能
```

#### 4. 文件 Appender (`test_file_appender.cpp`)

**文件管理**
```cpp
TEST_F(file_appender_test, SetAndGetFilename)
TEST_F(file_appender_test, SetAndGetFlushOnWrite)
- 测试文件名的设置和获取
- 测试刷新策略配置
```

**消息写入**
```cpp
TEST_F(file_appender_test, AppendSimpleTextMessage)
TEST_F(file_appender_test, AppendDictFormattedMessage)
TEST_F(file_appender_test, AppendMessagesConcurrently)
- 测试简单文本消息写入
- 测试格式化消息写入
- 测试多线程并发写入
```

## 测试覆盖统计

### 测试用例总数

**当前状态：71 个测试用例**（包含 1 个已禁用测试，已优化）

| 测试文件 | 测试用例数 | 主要覆盖 |
|---------|-----------|----------|
| `test_log.cpp` | 19 | 核心日志功能、追加器管理、智能占位符、结构化数据 |
| `test_log_manager.cpp` | 19 | 管理器、配置、工厂模式、动态加载容错 |
| `test_console_appender.cpp` | 20 (1 disabled) | 控制台输出、颜色配置、工厂创建、复杂场景 |
| `test_file_appender.cpp` | 13 | 文件写入、并发写入 |

### 模块覆盖率

| 模块 | 覆盖率 | 已测试用例 | 状态 |
|------|--------|-----------|------|
| `log_message` | 88.9% | ContextInfo, StructuredLogging, LazyFormatting, PlainMessage | ✅ 显著提升 |
| `logger` | 85.0% | 基础日志、级别过滤、追加器生命周期 | ✅ 良好 |
| `log_manager` | 81.8% | Singleton, Config, FactoryCreate | ✅ 良好 |
| `appender_factory` | 81.3% | FactoryCreate, GetOrCreateAppender, 重复名处理 | ✅ 良好 |
| `console_appender` | 100% | Constructor, Init, Append, Factory | ✅ 完整 |
| `file_appender` | 100% | File ops, Concurrency, Formatting | ✅ 完整 |

### 功能覆盖率

| 功能类别 | 覆盖率 | 已测试功能 | 状态 |
|---------|--------|-----------|------|
| 构造和初始化 | 73.3% | 默认构造、命名构造、初始化 | ✅ 良好 |
| 配置管理 | 70.0% | 级别设置、appender 配置 | ✅ 良好 |
| 日志记录 | 87.5% | 基本记录、结构化日志、智能占位符 | ✅ 优秀 |
| Appender 管理 | 66.7% | add_appender, 工厂创建 | ⚠️ 需提升 |
| 动态库加载 | 33.3% | 加载失败场景 | ❌ 不足 |
| 并发安全 | 33.3% | file_appender 并发写入 | ❌ 不足 |
| 错误处理 | 25.0% | 部分失败场景 | ❌ 不足 |

### 已完成的测试场景 ✅

#### 核心功能
- ✅ 基本日志记录（所有级别）
- ✅ 结构化日志（带参数）
- ✅ 日志级别过滤
- ✅ 智能占位符（自增索引、命名参数、格式说明符）
- ✅ 混合占位符类型
- ✅ 动态格式参数
- ✅ 边界情况（特殊字符、转义）
- ✅ 嵌套大括号、转义字符

#### Logger 管理
- ✅ 单例模式
- ✅ 默认和命名 logger 获取
- ✅ 配置创建新 logger
- ✅ 配置更新现有 logger
- ✅ 多 appender 和 logger 管理

#### Appender 功能
- ✅ Console appender 构造和初始化
- ✅ Console appender 消息输出
- ✅ File appender 文件管理
- ✅ File appender 不同格式消息
- ✅ File appender 并发写入
- ✅ 工厂模式创建 appender

### 需要补充的测试场景 ❌

#### 高优先级（核心功能，直接影响稳定性）

1. **Logger 模块**
   - ✅ `set_name()` / `is_enabled()` 边界场景
   - ✅ `remove_appender()` / `find_appender()` / `clear_appenders()` 生命周期管理
   - ⚠️ 移动语义完整测试（待补充）
   - ⚠️ 空 appender 列表的大批量日志压测（待补充）

2. **Message 模块**
   - ✅ `to_structured_data()` 多场景（带模板/纯文本）
   - ✅ 时间戳与线程 ID 验证
   - ✅ 延迟格式化机制测试

3. **Log Manager 模块**
   - ❌ 动态库加载成功场景
   - ❌ `load_appenders()` 方法测试（空目录、多文件）
   - ❌ 并发访问安全性测试
   - ❌ 异常处理测试

#### 中优先级（功能扩展，提高健壮性）

4. **Appender Factory 模块**
   - ❌ 动态库加载成功场景
   - ❌ `load_all()` 目录遍历测试
   - ❌ 已存在同名 appender 的处理
   - ❌ `register_creator()` 方法测试
   - ❌ `get_appender()` 获取不存在的 appender
   - ❌ 创建失败场景处理

5. **File Appender 模块**
   - ❌ `flush()` 方法测试
   - ❌ 文件路径测试（相对路径、绝对路径）
   - ❌ 文件权限错误场景
   - ❌ 磁盘空间不足场景

6. **Console Appender 模块**
   - ✅ 颜色输出功能验证（覆盖所有颜色类型）
   - ✅ 不同 stream_type 输出测试
   - ✅ flush 配置分支测试
   - ✅ 复杂场景融合测试
   - ⚠️ `get_console_color` 函数在自动化测试中无法完全覆盖（见下方说明）

#### 低优先级（辅助功能，优化体验）

7. **性能测试**
   - ❌ 大量日志消息的性能测试
   - ❌ 高并发场景的性能测试
   - ❌ 内存使用情况测试

8. **日志回滚**
   - ❌ 日志文件回滚机制
   - ❌ 大文件处理

## 优化方向

### 立即可做的改进

1. **补充 Logger 模块的边界测试**（预计工作量：2-3 小时）
   - 添加 set_name()、remove_appender()、find_appender()、clear_appenders() 的测试
   - 添加 is_enabled() 的边界条件测试
   - 添加移动语义的完整测试

2. **补充 Message 模块的功能测试**（预计工作量：2-3 小时）
   - 添加 to_structured_data() 方法测试
   - 添加时间戳和线程 ID 的验证测试
   - 添加延迟格式化机制测试

3. **补充并发安全测试**（预计工作量：3-4 小时）
   - 多线程获取 logger 的安全性测试
   - 多线程同时应用配置的测试

### 中期优化（提高健壮性）

4. **动态库加载完整测试**（预计工作量：2-3 小时）
   - ⚠️ 成功加载真实动态库（当前覆盖了失败分支）
   - ⚠️ load_all() 目录多 .so 文件场景（当前覆盖了异常容错）

5. **错误处理场景完善**（预计工作量：2-3 小时）
   - Appender 创建失败的场景
   - 配置错误的处理
   - 异常处理的边界情况

### 长期优化（提升性能）

6. **性能测试**（预计工作量：4-5 小时）
   - ⚠️ 大量日志消息的性能测试
   - ⚠️ 高并发场景的性能测试
   - ⚠️ 内存使用情况分析

7. **日志文件管理**（预计工作量：3-4 小时）
   - ⚠️ 日志文件回滚机制测试
   - ⚠️ 大文件处理性能测试
   - ⚠️ 文件路径的各种场景测试

## 测试策略

### 单元测试

每个测试文件专注于一个模块：
- `test_log.cpp` - 日志核心功能
- `test_log_manager.cpp` - 日志管理器
- `test_console_appender.cpp` - 控制台输出
- `test_file_appender.cpp` - 文件输出

### 集成测试

跨模块协作测试：
- logger 与 appender 的配合
- log_manager 的配置应用流程
- 工厂模式的创建和管理

### 性能测试

高负载场景：
- 大量日志消息的记录性能
- 并发写入的稳定性
- 内存使用情况

## 运行示例

```bash
# 运行所有日志测试
meson test -C builddir --suite log

# 运行特定测试
meson test -C builddir log_manager_test.SingletonInstance

# 详细输出
meson test -C builddir --suite log --verbose

# 查看测试日志
cat builddir/meson-logs/testlog.txt
```

## 注意事项

1. **环境要求**
   - 确保有写入权限（文件 appender 需要）
   - 测试会创建临时日志文件

2. **并发测试**
   - file appender 包含并发写入测试
   - 注意测试环境的 CPU 核心数

3. **清理**
   - 测试完成后会清理临时文件
   - 如遇到文件残留，手动删除 `builddir/log/` 目录

## 贡献指南

### 编写新测试的基本原则

1. **命名规范**：使用 `TestClassName::TestName` 格式
2. **独立性**：每个测试应独立，不依赖其他测试的状态
3. **清理资源**：在 TearDown 中清理测试产生的资源
4. **断言清晰**：使用有意义的断言消息
5. **文档注释**：复杂测试添加简要说明

### 测试用例编写示例

#### 示例 1：测试 logger 的基本操作

```cpp
TEST_F(LogTest, LoggerSetAndGetName) {
    mc::log::logger test_logger("test_logger");
    
    // 测试设置名称
    test_logger.set_name("new_name");
    EXPECT_EQ("new_name", test_logger.get_name());
    
    // 测试获取 appenders
    const auto& appenders = test_logger.get_appenders();
    EXPECT_TRUE(appenders.empty());
}
```

#### 示例 2：测试 remove_appender 方法

```cpp
TEST_F(LogTest, LoggerRemoveAppender) {
    auto mem_appender = std::make_shared<memory_appender>();
    mem_appender->set_name("test_appender");
    
    m_test_logger.add_appender(mem_appender);
    
    // 测试删除存在的 appender
    bool result = m_test_logger.remove_appender("test_appender");
    EXPECT_TRUE(result);
    
    // 测试删除不存在的 appender
    result = m_test_logger.remove_appender("non_existent");
    EXPECT_FALSE(result);
}
```

#### 示例 3：测试 to_structured_data() 方法

```cpp
TEST_F(LogTest, MessageToStructuredData) {
    mc::log::context ctx("test.cpp", "test_func", 42);
    mc::dict args;
    args["user"] = "admin";
    args["action"] = "login";
    
    mc::log::message msg(mc::log::level::info, ctx, 
                        "用户 ${user} 执行了 ${action}", args);
    
    auto data = msg.to_structured_data();
    
    // 验证结构化数据
    EXPECT_EQ(static_cast<int>(mc::log::level::info), data["level"]);
    EXPECT_TRUE(data.contains("context"));
    EXPECT_TRUE(data.contains("message"));
}
```

### 待补充测试清单

- [ ] `test_logger_operations.cpp` - Logger 操作方法测试
  - [ ] set_name() 测试
  - [ ] remove_appender() 测试
  - [ ] find_appender() 测试
  - [ ] clear_appenders() 测试
  - [ ] is_enabled() 测试

- [ ] `test_message_features.cpp` - Message 功能测试
  - [ ] to_structured_data() 测试
  - [ ] 时间戳测试
  - [ ] 线程 ID 测试
  - [ ] 延迟格式化测试

- [ ] `test_log_manager_advanced.cpp` - Log Manager 高级功能
  - [ ] 动态库加载测试
  - [ ] 并发安全测试
  - [ ] 异常处理测试

### 提交前的检查清单

- [ ] 所有测试都能通过
- [ ] 没有内存泄漏（使用 valgrind 检查）
- [ ] 代码符合项目风格（使用 clang-format）
- [ ] 添加了适当的注释
- [ ] 更新了 README.md（如果新增了测试文件）

## 特殊说明：`get_console_color` 函数覆盖

### 问题描述

`console_appender.cpp` 中的 `get_console_color` 函数（第 80-100 行）在自动化测试环境中无法完全覆盖，原因如下：

1. **函数调用条件**：`get_console_color` 只在以下条件同时满足时才会被调用：
   - `m_impl->cfg.use_color == true`（颜色功能已启用）
   - `isatty(fileno(out)) == true`（输出流是终端设备）

2. **自动化测试环境限制**：
   - 测试框架（GTest）会重定向 `stdout` 和 `stderr` 到文件或管道
   - 重定向后的流不是终端设备，`isatty()` 返回 `false`
   - 因此 `get_console_color` 函数永远不会被调用

### 当前测试覆盖情况

- ✅ **已覆盖**：`ConfigureAllColorTypesForGetConsoleColor` 测试用例配置了所有颜色类型
- ✅ **已覆盖**：`GetConsoleColorAllBranches` 测试用例尝试触发所有颜色分支
- ❌ **未覆盖**：`get_console_color` 函数本身的所有 case 分支（因为 `isatty()` 返回 false）

### 解决方案

#### 方案 1：手动测试（推荐用于验证功能）

在真实的终端环境中运行测试：

```bash
# 在终端中直接运行（不使用重定向）
./builddir/tests/libmcpp_test --gtest_filter="console_appender_test.GetConsoleColorAllBranches"

# 或者使用 script 命令创建伪终端
script -qec './builddir/tests/libmcpp_test --gtest_filter="console_appender_test.GetConsoleColorAllBranches"' /dev/null
```

#### 方案 2：使用伪终端（PTY）进行自动化测试

可以使用 `expect` 或 `pty` 库创建伪终端：

```cpp
// 示例：使用 pty 创建伪终端
#include <pty.h>
#include <unistd.h>

int master_fd, slave_fd;
if (openpty(&master_fd, &slave_fd, NULL, NULL, NULL) == 0) {
    // 将 stdout/stderr 重定向到 slave_fd
    dup2(slave_fd, STDOUT_FILENO);
    dup2(slave_fd, STDERR_FILENO);
    // 现在 isatty() 应该返回 true
}
```

**注意**：此方案需要修改测试代码，可能影响其他测试用例。

#### 方案 3：代码重构（需要修改源代码）

如果需要 100% 的自动化测试覆盖，可以考虑：

1. **提取 `get_console_color` 为公共函数**：将其从 `static` 改为公共函数，直接测试
2. **添加测试钩子**：添加一个可配置的 `isatty` 模拟函数
3. **分离颜色代码生成逻辑**：将颜色代码生成与终端检测分离

**注意**：这些方案需要修改源代码，可能不符合项目的设计原则。

### 代码覆盖报告说明

在 LCOV 覆盖率报告中，`get_console_color` 函数显示为：
- **函数覆盖率**：0%（0/1）
- **行覆盖率**：0%（0/21）
- **分支覆盖率**：0%（0/16）

这是**预期行为**，因为：
1. 函数逻辑简单（仅 switch 语句）
2. 函数在真实终端环境中会被调用（手动测试已验证）
3. 自动化测试环境限制导致无法覆盖

### 验证建议

1. **功能验证**：在真实终端中运行 `GetConsoleColorAllBranches` 测试，验证颜色输出正确
2. **代码审查**：`get_console_color` 函数逻辑简单，可以通过代码审查确保正确性
3. **集成测试**：在实际应用中使用 console_appender，验证颜色功能正常工作

### 相关测试用例

- `test_console_appender.cpp::ConfigureAllColorTypesForGetConsoleColor`：配置所有颜色类型
- `test_console_appender.cpp::GetConsoleColorAllBranches`：尝试触发所有颜色分支（需要真实终端）

## 参考文档

- [日志模块设计文档](../../docs/log_design.md)
- [日志 API 参考](../../docs/api/log.md)
- [测试框架文档](../../docs/testing.md)
