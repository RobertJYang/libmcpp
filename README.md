# MC 基础库

## 概述
mc 是一个现代C++基础库，提供：
- 反射系统：运行时类型信息获取与序列化（参见[reflect_usage.md](docs/reflect_usage.md)）
- 异常处理：带上下文追踪的异常体系（参见[exception_design.md](docs/exception_design.md)）
- 日志系统：多后端异步日志（参见[log_design.md](docs/log_design.md)）
- 核心组件：信号槽、事件循环、Future/Promise等

## 功能特性
```
+----------------+------------------------------------------------------+
| 模块           | 特性                                                 |
+----------------+------------------------------------------------------+
| reflect        | 类/枚举反射、对象序列化、嵌套类型支持、STL容器支持    |
| log            | 异步日志、多后端(Appender)、日志格式定制、上下文追踪 |
| exception      | 异常链、错误码映射、堆栈符号解析                     |
| application    | 事件循环、插件系统、配置管理                         |
| variant        | 动态类型系统、JSON互操作                            |
+----------------+------------------------------------------------------+

## 构建安装
```bash
# 依赖: meson >= 0.56
meson setup builddir
meson compile -C builddir

# 运行测试
meson test -C builddir

# 安装到系统
meson configure builddir --prefix=/usr/local
meson install -C builddir
```

## 快速开始
### 反射示例
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
```

### 日志示例
```cpp
#include <mc/log.h>

MC_LOG_REGISTER_APPENDER(console, mc::log::ConsoleAppender);

int main() {
    mc::log::Logger::getInstance()
        .setAppender<mc::log::RollingFileAppender>("app.log")
        .setLevel(mc::log::Level::Debug);

    MC_LOG_INFO("System initialized") 
        << "user_count=" << 42 
        << "feature_flags=[" << std::vector{"A/B测试", "灰度发布"} << "]";
}
```

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
完整测试列表请运行 `meson test -C builddir --list`
```

## 文档导航
- [架构设计](docs/application_design.md) - 核心组件交互关系
- [异常设计](docs/exception_design.md) - 错误处理最佳实践
- [日志配置](docs/log_design.md) - 日志格式与Appender配置
- [示例代码](examples/) - 包含插件系统、异步任务等示例

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

## 许可证
Apache 2.0 许可证，详见 LICENSE 文件