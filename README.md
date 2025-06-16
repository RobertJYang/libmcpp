# MC++ 现代C++基础库

[![构建状态](https://img.shields.io/badge/build-passing-brightgreen)](https://gitcode.com/zjp99/libmcpp)
[![版本](https://img.shields.io/badge/version-0.1.0-blue)](https://gitcode.com/zjp99/libmcpp)
[![许可证](https://img.shields.io/badge/license-Apache%202.0-green)](LICENSE)

## 概述

MC++ 是一个现代化的C++基础库，专为高性能、可扩展的应用程序设计。它提供了丰富的功能组件，帮助开发者构建健壮、高效的C++应用。

### 核心特性

- **反射系统**：运行时类型信息获取与序列化，支持嵌套类型和STL容器
- **异常处理**：带上下文追踪的异常体系，支持异常链和堆栈符号解析
- **日志系统**：多后端异步日志，支持自定义格式和上下文追踪
- **应用框架**：事件循环、插件系统、配置管理和资源管理
- **变体类型**：动态类型系统，与JSON无缝互操作
- **信号槽**：类型安全的信号槽实现，支持异步连接
- **Future/Promise**：异步编程模型，支持链式操作和异常传播

## 功能模块

```
+----------------+------------------------------------------------------+
| 模块           | 特性                                                 |
+----------------+------------------------------------------------------+
| reflect        | 类/枚举反射、对象序列化、嵌套类型支持、STL容器支持    |
| log            | 异步日志、多后端(Appender)、日志格式定制、上下文追踪 |
| exception      | 异常链、错误码映射、堆栈符号解析                     |
| application    | 事件循环、插件系统、配置管理                         |
| variant        | 动态类型系统、JSON互操作                            |
| filesystem     | 路径操作、文件IO、目录遍历                          |
| future         | 异步任务、链式操作、异常传播                        |
+----------------+------------------------------------------------------+
```

## 系统要求

- C++17 兼容的编译器 (GCC 7+, Clang 5+, MSVC 2019+)
- Boost 库 (1.66+)
- Meson 构建系统 (0.56+)

## 构建与安装

### 编译环境
```bash
# 编译环境： 安装 boost ninja
apt-get update && apt-get install -y libboost-all-dev

apt-get update
apt-get install -y libgtest-dev ninja-build

```
### 用 ninja 命令构建

```bash
# 初始化 meson 构建目录
meson setup builddir

# 进入构建目录
cd builddir

# 基本构建（在当前目录查找 build.ninja 文件并执行构建）
ninja

# 指定构建目录
ninja -C builddir

# 并行构建（指定并行任务数）
ninja -j 4

# 构建特定目标
ninja target_name

# 清理构建产物
ninja -t clean
```

### 从源码构建

```bash
# 克隆仓库
git clone https://github.com/yourusername/libmcpp.git
cd libmcpp

# 配置构建
meson setup builddir

# 编译
meson compile -C builddir

# 运行测试
meson test -C builddir

# 安装到系统
meson configure builddir --prefix=/usr/local
meson install -C builddir

# 编译性能优化说明

## 编译并发任务配置

### 性能测试数据
基于不同并发任务数的编译耗时测试结果：

| 并发任务数 | 编译耗时 | 性能提升 |
|------------|----------|----------|
| -j4        | 346s     | 基准值   |
| -j16       | 126s     | 2.7x     |
| -j24       | 106s     | 3.3x     |
| -j48       | 90s      | 3.8x     |
| -j128      | 38s      | 9.1x     |

### 编译命令示例
```bash
# 清理并重新构建（使用48核并发）
rm -rf builddir; meson setup builddir; time meson compile -C builddir -j 48

# 其他并发配置示例
rm -rf builddir; meson setup builddir; time meson compile -C builddir -j 128  # 最大并发
rm -rf builddir; meson setup builddir; time meson compile -C builddir -j 24   # 中等并发
rm -rf builddir; meson setup builddir; time meson compile -C builddir -j 4    # 低并发
```

### 并发任务机制说明
1. **编译并发原理**
   - Meson 构建系统支持多任务并行编译
   - 每个并发任务对应一个独立的编译进程
   - 任务数通过 `-j` 参数控制，表示同时运行的编译进程数

2. **性能影响因素**
   - CPU核心数：建议并发数不超过物理核心数
   - 内存容量：每个编译进程都需要一定内存
   - 磁盘I/O：大量并发可能导致磁盘I/O瓶颈
   - 网络带宽：如果使用远程依赖，网络可能成为瓶颈

3. **最佳实践建议**
   - 使用48核并发，在测试环境中表现最佳
   - 根据实际硬件配置调整并发数
   - 内存受限时适当降低并发数
   - 考虑使用 `-j$(nproc)` 自动匹配CPU核心数
```

### 使用包管理器安装

```bash
# Debian/Ubuntu
apt install libmcpp-dev

# Fedora/RHEL
dnf install libmcpp-devel

# macOS
brew install libmcpp
```

## 快速开始

### 反射系统

```cpp
#include <mc/reflect.h>

class User {
public:
    std::string name;
    int id;
    std::vector<std::string> tags;
};
MC_REFLECT(User, (name)(id)(tags))

// 序列化对象
User user{"admin", 1001, {"staff", "devops"}};
mc::variant var = user;
std::cout << "JSON: " << mc::json::to_string(var);

// 反序列化对象
std::string json = "{\"name\":\"guest\",\"id\":1002,\"tags\":[\"visitor\"]}";
User new_user = mc::json::from_string(json).as<User>();
```

### 日志系统

```cpp
#include <mc/log.h>

// 注册日志后端
MC_LOG_REGISTER_APPENDER(console, mc::log::ConsoleAppender);

int main() {
    // 配置日志系统
    mc::log::Logger::getInstance()
        .setAppender<mc::log::RollingFileAppender>("app.log", 10*1024*1024, 5)
        .setLevel(mc::log::Level::Debug);

    // 记录简单日志
    MC_LOG_INFO("系统初始化完成");
    
    // 记录带参数的结构化日志
    MC_LOG_INFO("用户活动") 
        << "user_id=" << 42 
        << "action=" << "login"
        << "tags=" << std::vector{"web", "secure"}
        << "timestamp=" << std::chrono::system_clock::now();
        
    return 0;
}
```

### 插件系统

```cpp
#include <mc/core/application.h>

// 定义插件
class MyPlugin : public mc::plugin_base<MyPlugin> {
public:
    static const char* plugin_name() { return "my_plugin"; }
    
    bool initialize() override {
        // 初始化逻辑
        return true;
    }
    
    void startup() override {
        // 启动逻辑
    }
    
    void shutdown() override {
        // 关闭逻辑
    }
};

// 应用程序入口
int main(int argc, char** argv) {
    try {
        // 初始化应用
        mc::application& app = mc::application::instance();
        app.set_version("1.0.0");
        
        // 注册插件
        app.register_plugin<MyPlugin>();
        
        // 初始化并启动
        app.initialize(argc, argv);
        app.startup();
        
        // 运行应用
        app.exec();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}
```

### 异常处理

```cpp
#include <mc/exception.h>

void database_operation() {
    try {
        // 数据库操作
        throw std::runtime_error("连接失败");
    } catch (const std::exception& e) {
        // 转换为带上下文的异常
        MC_THROW(mc::db_error, "数据库操作失败") 
            << mc::error_info("operation", "query")
            << mc::error_info("sql", "SELECT * FROM users")
            << mc::error_cause(e);
    }
}

int main() {
    try {
        database_operation();
    } catch (const mc::exception& e) {
        // 访问异常详情
        std::cerr << "错误: " << e.what() << std::endl;
        std::cerr << "错误码: " << e.code() << std::endl;
        std::cerr << "堆栈: " << e.backtrace() << std::endl;
        
        // 访问异常链
        if (auto cause = e.get_cause()) {
            std::cerr << "原因: " << cause->what() << std::endl;
        }
    }
    return 0;
}
```

## 性能特点

- **低开销反射**：优化的类型信息存储，最小化运行时开销
- **零拷贝序列化**：支持视图模式，避免不必要的内存复制
- **异步日志**：非阻塞日志记录，最小化对主线程的影响
- **内存池优化**：关键组件使用自定义内存分配器，减少内存碎片
- **编译时优化**：大量使用模板元编程和编译期计算

## 测试覆盖

测试框架验证了以下核心场景：

```
+--------------------------+-------------------------------------------+
| 测试目录                 | 覆盖场景                                  |
+--------------------------+-------------------------------------------+
| tests/reflect/           | 类反射、枚举转换、嵌套对象序列化          |
| tests/log/               | 日志级别过滤、多线程写入、日志轮转        |
| tests/exception/         | 异常传播链、错误码转换、多语言消息        |
| tests/variant/           | 类型转换、容器操作、JSON互操作性         |
| tests/application/       | 插件热加载、配置热更新、信号槽性能        |
+--------------------------+-------------------------------------------+
```

完整测试列表请运行 `meson test -C builddir --list`

## 文档导航

- [架构设计](docs/application_design.md) - 核心组件交互关系
- [异常设计](docs/exception_design.md) - 错误处理最佳实践
- [日志配置](docs/log_design.md) - 日志格式与Appender配置
- [反射使用](docs/reflect_usage.md) - 反射系统详细用法
- [示例代码](examples/) - 包含插件系统、异步任务等示例

## API参考

- [反射API](https://yourdomain.com/docs/api/reflect.html)
- [日志API](https://yourdomain.com/docs/api/log.html)
- [异常API](https://yourdomain.com/docs/api/exception.html)
- [应用框架API](https://yourdomain.com/docs/api/application.html)
- [变体类型API](https://yourdomain.com/docs/api/variant.html)

## 兼容性

| 平台          | 编译器                | 状态       |
|--------------|----------------------|------------|
| Linux        | GCC 7+, Clang 5+     | 完全支持    |
| macOS        | AppleClang 10+       | 完全支持    |
| Windows      | MSVC 2019+           | 基本支持    |
| FreeBSD      | Clang 6+             | 实验性支持  |

## 贡献指南

1. 代码风格遵循 `.clang-format` 配置
2. 新功能需包含：
   - 头文件文档（位于include/mc/）
   - 单元测试（位于tests/对应目录）
   - 使用示例（位于examples/）
3. 提交前运行完整测试集：
   ```bash
   meson test -C builddir --suite all
   ```
4. 重大变更需更新对应设计文档
5. 提交PR前请确保通过CI检查

## 常见问题

**Q: 如何在CMake项目中使用MC++库？**

A: 可以使用`find_package`查找已安装的MC++库：
```cmake
find_package(MCPP REQUIRED)
target_link_libraries(your_target PRIVATE MCPP::mcpp)
```

**Q: MC++库是否支持嵌入式平台？**

A: 是的，MC++库设计时考虑了资源受限环境，可以通过编译选项禁用部分功能以减小二进制大小。

**Q: 如何自定义日志格式？**

A: 可以通过实现自定义的`Formatter`类并注册到日志系统：
```cpp
mc::log::Logger::getInstance().setFormatter<MyCustomFormatter>();
```

## 许可证

Apache 2.0 许可证，详见 [LICENSE](LICENSE) 文件

## 联系方式

- 项目主页：https://gitcode.com/zjp99/libmcpp
- 问题报告：https://gitcode.com/zjp99/libmcpp/issues
- 邮件列表：3977743558@qq.com