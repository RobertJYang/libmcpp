# Exception API文档

## 概述

`mc::exception` 是一个统一的异常处理系统，为整个库提供了一致、可扩展的错误处理机制。它基于C++标准异常机制进行了扩展，提供更丰富的错误信息和更灵活的异常处理方式。

## 主要特性

- **统一异常类层次结构**：所有异常继承自 `mc::exception` 基类
- **丰富的错误信息**：包括错误代码、错误消息、文件名、行号和时间戳
- **异常工厂**：支持动态异常类型识别和重新抛出
- **日志集成**：异常可以包含多条日志消息，提供详细的错误上下文
- **易用的宏**：提供简单直观的宏简化异常的抛出和处理

## 基本用法

### 抛出异常

```cpp
// 使用 MC_THROW 宏抛出预定义异常
MC_THROW(mc::timeout_exception, "连接超时: ${host}:${port}", ("host", "127.0.0.1")("port", "8080"));

// 使用条件断言
MC_ASSERT(ptr != nullptr, "指针不能为空");

// 捕获并重新抛出，添加更多上下文
try {
    // 可能抛出异常的代码
} catch (const std::exception& e) {
    MC_RETHROW_EXCEPTION(mc::system_exception, "文件操作失败: ${file}", ("file", file_path));
}
```

### 捕获和处理异常

```cpp
try {
    // 可能抛出异常的代码
} catch (const mc::timeout_exception& e) {
    // 处理特定类型的异常
    std::cerr << "超时异常: " << e.what() << std::endl;
} catch (const mc::exception& e) {
    // 处理所有 MC 库异常
    std::cerr << "MC异常: " << e.code() << " " << e.name() << ": " << e.what() << std::endl;
    
    // 打印详细信息
    std::cerr << e.to_detail_string() << std::endl;
} catch (const std::exception& e) {
    // 处理标准库异常
    std::cerr << "标准异常: " << e.what() << std::endl;
} catch (...) {
    // 处理所有其他未知异常
    std::cerr << "未知异常" << std::endl;
}
```

### 自定义异常类

可以使用 `MC_DEFINE_EXCEPTION_CLASS` 宏定义自定义异常类：

```cpp
// 定义自定义异常类
MC_DEFINE_EXCEPTION_CLASS(database_connection_exception, 200, "数据库连接失败", "db_connection")

// 注册异常类型到异常工厂
MC_REGISTER_EXCEPTION(database_connection_exception);

// 使用自定义异常
MC_THROW(database_connection_exception, "无法连接到数据库: ${server}", ("server", db_server));
```

## API 详细说明

### 异常代码枚举

`mc::exception_code` 定义了可能的异常类型代码：

| 代码 | 枚举常量 | 描述 |
|------|----------|------|
| 0 | `unknow_exception_code` | 未知异常代码 |
| 1 | `unhandled_exception_code` | 未处理的第三方异常 |
| 2 | `timeout_exception_code` | 超时异常 |
| 3 | `file_not_found_exception_code` | 文件未找到异常 |
| ... | ... | ... |
| 100 | `variant_error_code` | 变体类型异常 |
| 101 | `dict_error_code` | 字典异常 |
| ... | ... | ... |

### exception 基类

`mc::exception` 是所有异常的基类：

| 方法 | 描述 |
|------|------|
| `exception(mc::log::message&& msg, int64_t code, const std::string& name, const std::string& what)` | 构造函数 |
| `int64_t code() const noexcept` | 获取异常代码 |
| `const char* name() const noexcept` | 获取异常名称 |
| `const char* what() const noexcept` | 获取异常描述（继承自 std::exception）|
| `void append_log(mc::log::message msg)` | 添加日志消息 |
| `std::string to_detail_string(mc::log::level ll = mc::log::level::all) const` | 获取详细异常信息 |
| `std::string to_string(mc::log::level ll = mc::log::level::info) const` | 获取简要异常信息 |
| `const std::string& top_message() const` | 获取顶层异常消息 |
| `void dynamic_rethrow_exception() const` | 动态重新抛出异常 |
| `std::shared_ptr<exception> dynamic_copy_exception() const` | 动态复制异常 |

### 预定义异常类

库预定义了许多常用异常类：

| 异常类 | 异常代码 | 描述 |
|--------|----------|------|
| `timeout_exception` | `timeout_exception_code` | 操作超时 |
| `file_not_found_exception` | `file_not_found_exception_code` | 文件未找到 |
| `parse_error_exception` | `parse_error_exception_code` | 解析错误 |
| `invalid_arg_exception` | `invalid_arg_exception_code` | 无效参数 |
| `key_not_found_exception` | `key_not_found_exception_code` | 键未找到 |
| `bad_cast_exception` | `bad_cast_exception_code` | 类型转换错误 |
| `out_of_range_exception` | `out_of_range_exception_code` | 索引越界 |
| ... | ... | ... |

### 特殊异常类

| 异常类 | 描述 |
|--------|------|
| `unhandled_exception` | 包装未处理的第三方异常 |
| `std_exception_wrapper` | 包装标准库异常 |

### 异常工厂

`mc::exception_factory` 用于注册和创建异常：

| 方法 | 描述 |
|------|------|
| `static exception_factory& instance()` | 获取单例实例 |
| `template<typename T> void register_exception()` | 注册异常类型 |
| `void rethrow(const exception& e) const` | 重新抛出异常 |

### 辅助宏

| 宏 | 描述 |
|------|------|
| `MC_THROW(exception_class, message, ...)` | 抛出指定类型的异常 |
| `MC_ASSERT(condition, message, ...)` | 条件断言，如果条件为假则抛出断言异常 |
| `MC_DEFINE_EXCEPTION_CLASS(class_name, code_enum_value, default_msg, class_name_str)` | 定义自定义异常类 |
| `MC_REGISTER_EXCEPTION(exception_class)` | 注册异常类到异常工厂 |
| `MC_RETHROW_EXCEPTION(exception_class, message, ...)` | 捕获并重新抛出异常，添加上下文信息 |
| `MC_CAPTURE_AND_WRAP_EXCEPTION(e, message, ...)` | 捕获并包装标准异常 |

## 最佳实践

1. **使用预定义异常**：优先使用库提供的预定义异常类，而不是自定义异常

2. **提供详细错误信息**：抛出异常时提供充分的上下文信息，包括具体的错误原因和相关参数

3. **使用异常工厂**：对于自定义异常类，使用异常工厂进行注册，以支持动态异常处理

4. **异常粒度**：根据需要使用适当粒度的异常处理，既不要过于笼统，也不要过于细化

5. **异常传播**：适当使用重新抛出机制，在保留原始异常信息的同时添加上下文信息

6. **资源安全**：确保在异常处理过程中正确释放资源，推荐使用 RAII 模式 