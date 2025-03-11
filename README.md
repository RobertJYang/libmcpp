# MC 日志系统

MC 日志系统是一个功能完善的日志框架，支持日志前后端分离、结构化日志、动态加载日志后端等特性。

## 特性

- **日志前后端分离**：前端用于代码中记录日志，后端负责日志的实际输出
- **支持多种日志对象**：可以创建不同的日志对象，用于不同的日志分类
- **可插拔的日志后端**：支持从动态库加载日志后端，后端可以独立实现
- **结构化日志**：支持结构化的日志写法，使用 `dict` 类型存储参数

## 快速开始

### 基本用法

```cpp
#include <mc/log/log_macros.h>

// 使用默认日志记录器输出普通日志
ilog("这是一条信息日志");
dlog("这是一条调试日志，包含数字: " << 42);
elog("这是一条错误日志");

// 使用默认日志记录器输出结构化日志
wilog("用户 ${user} 登录成功，IP: ${ip}", 
    ("user", "admin")("ip", "192.168.1.1"));
```

### 使用自定义日志记录器

```cpp
#include <mc/log/log_manager.h>
#include <mc/log/log_macros.h>

// 创建自定义日志记录器
auto app_logger = mc::log::log_manager::instance().get_logger("app");

// 使用自定义日志记录器输出普通日志
mc_ilog(app_logger, "这是一条使用自定义日志记录器的信息日志");

// 使用自定义日志记录器输出结构化日志
mc_wilog(app_logger, "用户 ${user} 登录成功，IP: ${ip}", 
    ("user", "admin")("ip", "192.168.1.1"));
```

### 配置日志记录器

```cpp
#include <mc/log/log_manager.h>
#include <mc/log/console_appender.h>
#include <mc/log/file_log_backend.h>

// 获取日志管理器
auto& log_manager = mc::log::log_manager::instance();

// 创建日志记录器
auto app_logger = log_manager.get_logger("app");

// 添加控制台追加器
auto console = std::make_shared<mc::log::console_appender>();
app_logger.add_appender(console);

// 创建文件日志后端
auto file_backend = std::make_shared<mc::log::file_log_backend>();
file_backend->init("app.log");

// 将后端包装为追加器并添加到日志记录器
auto file_adapter = log_manager.create_backend_adapter(file_backend);
app_logger.add_appender(file_adapter);

// 设置日志级别
app_logger.set_level(mc::log::level::trace);
```

### 动态加载日志后端

```cpp
#include <mc/log/log_manager.h>

// 获取日志管理器
auto& log_manager = mc::log::log_manager::instance();

// 动态加载日志后端
auto dynamic_backend = log_manager.load_backend("libmylogbackend.so", "config.json");
if (dynamic_backend) {
    // 将后端包装为追加器并添加到日志记录器
    auto dynamic_adapter = log_manager.create_backend_adapter(dynamic_backend);
    log_manager.get_logger().add_appender(dynamic_adapter);
}
```

## 日志宏

### 普通日志宏

| 宏名 | 日志级别 | 描述 |
|------|----------|------|
| `tlog(...)` | TRACE | 跟踪级别日志 |
| `dlog(...)` | DEBUG | 调试级别日志 |
| `ilog(...)` | INFO | 信息级别日志 |
| `wlog(...)` | WARN | 警告级别日志 |
| `elog(...)` | ERROR | 错误级别日志 |
| `flog(...)` | FATAL | 致命级别日志 |

### 结构化日志宏

| 宏名 | 日志级别 | 描述 |
|------|----------|------|
| `wtlog(FORMAT, ...)` | TRACE | 跟踪级别结构化日志 |
| `wdlog(FORMAT, ...)` | DEBUG | 调试级别结构化日志 |
| `wilog(FORMAT, ...)` | INFO | 信息级别结构化日志 |
| `wwlog(FORMAT, ...)` | WARN | 警告级别结构化日志 |
| `welog(FORMAT, ...)` | ERROR | 错误级别结构化日志 |
| `wflog(FORMAT, ...)` | FATAL | 致命级别结构化日志 |

### 指定日志记录器的日志宏

| 宏名 | 日志级别 | 描述 |
|------|----------|------|
| `mc_tlog(LOGGER, ...)` | TRACE | 跟踪级别日志 |
| `mc_dlog(LOGGER, ...)` | DEBUG | 调试级别日志 |
| `mc_ilog(LOGGER, ...)` | INFO | 信息级别日志 |
| `mc_wlog(LOGGER, ...)` | WARN | 警告级别日志 |
| `mc_elog(LOGGER, ...)` | ERROR | 错误级别日志 |
| `mc_flog(LOGGER, ...)` | FATAL | 致命级别日志 |

### 指定日志记录器的结构化日志宏

| 宏名 | 日志级别 | 描述 |
|------|----------|------|
| `mc_wtlog(LOGGER, FORMAT, ...)` | TRACE | 跟踪级别结构化日志 |
| `mc_wdlog(LOGGER, FORMAT, ...)` | DEBUG | 调试级别结构化日志 |
| `mc_wilog(LOGGER, FORMAT, ...)` | INFO | 信息级别结构化日志 |
| `mc_wwlog(LOGGER, FORMAT, ...)` | WARN | 警告级别结构化日志 |
| `mc_welog(LOGGER, FORMAT, ...)` | ERROR | 错误级别结构化日志 |
| `mc_wflog(LOGGER, FORMAT, ...)` | FATAL | 致命级别结构化日志 |

## 结构化日志示例

结构化日志使用 `${key}` 作为占位符，使用 `("key", value)` 的方式提供参数：

```cpp
// 简单参数
wilog("用户 ${user} 登录成功，IP: ${ip}", 
    ("user", "admin")("ip", "192.168.1.1"));

// 变量参数
std::string error_msg = "连接超时";
welog("无法连接到服务器: ${url}，错误: ${error}", 
    ("url", "https://example.com")("error", error_msg));

// 复杂数据
mc::dict user_info = {
    {"id", 12345},
    {"name", "张三"},
    {"roles", mc::variants{"admin", "user"}},
    {"active", true}
};

wilog("用户信息: ${info}", 
    ("info", user_info.as_string()));
``` 