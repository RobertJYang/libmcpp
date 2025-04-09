# Log API文档

## 概述

`mc::log` 是一个灵活、高性能的日志系统，为应用程序提供结构化日志记录能力。该模块采用模块化设计，支持多种日志级别、多种输出目标和自定义格式化，针对资源受限场景进行了优化。

## 主要特性

- **多级日志**：支持trace、debug、info、warn、error、fatal等多种日志级别
- **结构化日志**：支持键值对方式的结构化日志记录，便于分析和处理
- **多目标输出**：支持控制台、文件等多种输出目标
- **可扩展的架构**：支持自定义appender，可扩展到各种输出目标
- **上下文信息**：自动记录文件名、行号、函数名等上下文信息
- **便捷的宏接口**：提供简洁易用的宏，简化日志记录

## 基本用法

### 记录日志

```cpp
// 使用全局日志宏（推荐）
tlog("详细调试信息");
dlog("调试信息: ${value}", ("value", 42));
ilog("用户 ${user} 登录成功", ("user", "admin"));
wlog("磁盘空间不足: ${available}GB", ("available", 1.2));
elog("操作失败: ${code}, ${message}", ("code", 404)("message", "资源不存在"));
flog("系统崩溃: ${reason}", ("reason", "内存访问错误"));
```

### 使用指定日志记录器

```cpp
// 获取指定名称的日志记录器
auto logger = mc::get_logger("database");

// 使用指定日志记录器记录日志
mc_ilog(logger, "数据库连接已建立: ${host}:${port}", 
       ("host", "localhost")("port", 3306));
```

### 结构化日志格式

```cpp
// 基本格式：格式字符串中的 ${key} 会被后面的键值对替换
ilog("处理请求: ${method} ${path}, 耗时: ${time}ms", 
    ("method", "GET")("path", "/api/users")("time", 45.8));

// 支持复合数据类型
std::vector<int> ids = {1, 2, 3};
ilog("用户ID列表: ${ids}", ("ids", ids));

// 支持嵌套数据结构
mc::dict user = {
    {"name", "张三"},
    {"age", 30},
    {"roles", mc::variants{"admin", "user"}}
};
ilog("用户信息: ${user}", ("user", user));
```

## 日志系统配置

### 配置日志级别

```cpp
// 设置默认日志记录器的级别
mc::log::default_logger().set_level(mc::log::level::debug);

// 获取并配置特定日志记录器
auto logger = mc::get_logger("network");
logger.set_level(mc::log::level::info);
```

### 添加日志追加器

```cpp
// 创建控制台追加器
auto console = mc::log::log_manager::instance().create_appender("console");
console->init({{"color", true}}); // 启用彩色输出

// 添加到默认日志记录器
mc::log::default_logger().add_appender(console);

// 创建文件追加器
auto file = mc::log::log_manager::instance().create_appender("file");
file->init({
    {"filename", "app.log"},
    {"max_size", 10 * 1024 * 1024}, // 10MB
    {"rotation", true}
});

// 添加到特定日志记录器
auto logger = mc::get_logger("database");
logger.add_appender(file);
```

## API 详细说明

### 日志级别

`mc::log::level` 枚举定义了支持的日志级别：

| 级别 | 描述 |
|------|------|
| `trace` | 最详细的跟踪信息 |
| `debug` | 调试信息 |
| `info` | 普通信息 |
| `warn` | 警告信息 |
| `error` | 错误信息 |
| `fatal` | 致命错误 |
| `off` | 关闭日志 |
| `all` | 所有级别 |

### 日志宏

#### 全局日志宏

| 宏 | 描述 |
|------|------|
| `tlog(format, args...)` | 记录 trace 级别日志 |
| `dlog(format, args...)` | 记录 debug 级别日志 |
| `ilog(format, args...)` | 记录 info 级别日志 |
| `wlog(format, args...)` | 记录 warn 级别日志 |
| `elog(format, args...)` | 记录 error 级别日志 |
| `flog(format, args...)` | 记录 fatal 级别日志 |

#### 指定日志记录器的宏

| 宏 | 描述 |
|------|------|
| `mc_tlog(logger, format, args...)` | 使用指定记录器记录 trace 级别日志 |
| `mc_dlog(logger, format, args...)` | 使用指定记录器记录 debug 级别日志 |
| `mc_ilog(logger, format, args...)` | 使用指定记录器记录 info 级别日志 |
| `mc_wlog(logger, format, args...)` | 使用指定记录器记录 warn 级别日志 |
| `mc_elog(logger, format, args...)` | 使用指定记录器记录 error 级别日志 |
| `mc_flog(logger, format, args...)` | 使用指定记录器记录 fatal 级别日志 |

### 核心类

#### logger 类

| 方法 | 描述 |
|------|------|
| `static logger get(const char* name)` | 获取指定名称的日志记录器 |
| `logger(const std::string& name)` | 构造指定名称的日志记录器 |
| `void set_level(level lvl)` | 设置日志级别 |
| `level get_level() const` | 获取日志级别 |
| `bool is_enabled(level lvl) const` | 检查指定日志级别是否启用 |
| `void log(message msg)` | 记录日志消息 |
| `void add_appender(const appender_ptr& a)` | 添加日志追加器 |
| `bool remove_appender(const std::string& name)` | 删除指定名称的追加器 |
| `appender_ptr find_appender(const std::string& name) const` | 查找指定名称的追加器 |
| `const std::vector<appender_ptr>& get_appenders() const` | 获取所有追加器 |
| `void clear_appenders()` | 清除所有追加器 |

#### message 类

消息类表示一条日志消息，包含级别、格式、参数等信息：

| 方法 | 描述 |
|------|------|
| `message(level lvl, const std::string& format)` | 构造函数 |
| `level get_level() const` | 获取日志级别 |
| `const context& get_context() const` | 获取上下文信息 |
| `const std::string& get_message() const` | 获取格式化后的消息 |
| `const std::chrono::system_clock::time_point& get_timestamp() const` | 获取时间戳 |

#### appender 类

追加器接口类，定义了日志输出目标的基本接口：

| 方法 | 描述 |
|------|------|
| `virtual bool init(const variant& args)` | 初始化追加器 |
| `virtual void append(const message& msg)` | 追加日志消息 |
| `const std::string& get_name() const` | 获取追加器名称 |
| `void set_name(const std::string& name)` | 设置追加器名称 |

#### log_manager 类

日志管理器类，负责管理日志记录器和追加器：

| 方法 | 描述 |
|------|------|
| `static log_manager& instance()` | 获取单例实例 |
| `logger get_logger(const char* name)` | 获取指定名称的日志记录器 |
| `appender_ptr create_appender(const std::string& type)` | 创建指定类型的追加器 |
| `bool load_appender(const std::string& lib_path, const std::string& appender_name)` | 从动态库加载追加器 |
| `void load_config(const logging_config& config)` | 加载日志配置 |

## 内置追加器

| 追加器类型 | 描述 | 配置项 |
|-----------|---------|---------|
| `console` | 输出到控制台 | `color`: 是否启用彩色输出 |
| `file` | 输出到文件 | `filename`: 文件名<br>`max_size`: 最大文件大小<br>`rotation`: 是否启用轮转 |
| `rotating_file` | 按大小轮转文件 | `filename`: 文件名<br>`max_size`: 单个文件最大大小<br>`max_files`: 最大文件数 |
| `daily_file` | 按日期轮转文件 | `filename`: 文件名<br>`hour`: 轮转小时<br>`minute`: 轮转分钟 |
| `null` | 丢弃所有日志 | 无 |

## 最佳实践

1. **使用结构化日志**：优先使用键值对格式记录日志，而不是简单字符串拼接

2. **日志级别使用**：
   - 谨慎使用 `trace` 级别，仅用于非常详细的调试信息
   - `debug` 级别用于开发调试
   - `info` 级别用于记录正常操作
   - `warn` 级别用于潜在问题警告
   - `error` 级别用于错误但不影响程序继续运行
   - `fatal` 级别用于导致程序终止的严重错误

3. **性能考虑**：
   - 在高性能关键路径上，先检查日志级别是否启用再构造复杂日志消息
   - 对于频繁记录的日志点，考虑使用采样或限流

4. **分类记录**：使用不同名称的日志记录器对日志进行分类，便于管理和过滤

5. **状态跟踪**：在长时间操作的开始和结束都记录日志，便于跟踪操作状态和耗时 