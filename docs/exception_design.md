# MC库异常处理方案设计

## 1. 概述

MC库的异常处理方案旨在提供一个统一、可扩展的异常处理机制，使库的用户能够以一致的方式处理各种错误情况。该方案基于C++标准异常机制，并进行了扩展，以支持更丰富的错误信息和更灵活的异常处理方式。

## 2. 设计目标

- **统一性**：为MC库提供统一的异常处理机制
- **可扩展性**：允许用户定义自己的异常类型
- **信息丰富**：提供详细的错误信息，包括错误代码、错误消息、文件名、行号等
- **易用性**：提供简单易用的API和宏，简化异常的抛出和捕获
- **安全性**：确保异常处理不会导致资源泄漏
- **性能**：最小化异常处理对性能的影响

## 3. 核心组件

### 3.1 异常代码枚举

`exception_code`枚举定义了MC库中所有可能的异常类型代码，包括通用异常代码和特定模块异常代码。

```cpp
enum exception_code {
    // 通用异常代码
    general_error_code               = 0,  // 未指定异常代码
    external_error_code              = 1,  // 未处理的第三方异常
    // ...
    
    // MC库特定异常代码 (从100开始)
    variant_error_code               = 100, // 变体类型异常
    dict_error_code                  = 101, // 字典异常
    // ...
};
```

### 3.2 日志消息结构

`log_message`结构用于存储异常的详细信息，包括日志级别、消息内容、文件名、行号和时间戳。

```cpp
struct log_message {
    log_level                    m_level;
    std::string                  m_message;
    std::string                  m_file;
    uint32_t                     m_line;
    std::chrono::system_clock::time_point m_timestamp;
    
    // 构造函数...
};
```

### 3.3 异常基类

`exception`类是MC库所有异常的基类，继承自`std::exception`，并提供了更丰富的功能。

```cpp
class exception : public std::exception {
public:
    // 构造函数...
    
    // 获取异常信息的方法
    int64_t code() const noexcept;
    const char* name() const noexcept;
    virtual const char* what() const noexcept override;
    
    // 添加日志消息
    void append_log(log_message msg);
    
    // 获取异常信息的字符串表示
    std::string to_detail_string(log_level ll = log_level::all) const;
    std::string to_string(log_level ll = log_level::info) const;
    std::string top_message() const;
    
    // 动态异常处理
    virtual void dynamic_rethrow_exception() const;
    virtual std::shared_ptr<exception> dynamic_copy_exception() const;
    
protected:
    std::unique_ptr<detail::exception_impl> m_impl;
};
```

### 3.4 特殊异常类

- **未处理异常**：`unhandled_exception`类用于包装未处理的第三方异常
- **标准异常包装器**：`std_exception_wrapper`类用于包装标准库异常

### 3.5 异常工厂

`exception_factory`类用于注册和创建异常，支持动态异常类型识别和重新抛出。

```cpp
class exception_factory {
public:
    // 异常构建器...
    
    // 获取单例实例
    static exception_factory& instance();
    
    // 注册异常类型
    template<typename T>
    void register_exception();
    
    // 重新抛出异常
    void rethrow(const exception& e) const;
    
private:
    // 注册的异常映射表
    std::unordered_map<int64_t, base_exception_builder*> m_registered_exceptions;
};
```

### 3.6 辅助宏

MC库提供了一系列宏，简化异常的抛出和捕获：

- **MC_ASSERT**：断言宏，如果条件为假，则抛出断言异常
- **MC_THROW**：抛出指定类型的异常
- **MC_RETHROW_EXCEPTION**：捕获并重新抛出异常，添加上下文信息
- **MC_CAPTURE_AND_WRAP_EXCEPTION**：捕获并包装标准异常

## 4. 异常处理流程

### 4.1 抛出异常

1. 使用`MC_THROW`宏抛出异常，自动包含文件名和行号信息
2. 或者直接创建异常对象并抛出，可以添加更多自定义信息

### 4.2 捕获异常

1. 先捕获特定类型的异常，处理特定错误情况
2. 然后捕获`mc::exception`基类，处理所有MC库异常
3. 再捕获`std::exception`，处理标准库异常
4. 最后捕获`...`，处理所有未知异常

### 4.3 重新抛出异常

1. 使用`MC_RETHROW_EXCEPTION`宏重新抛出异常，添加上下文信息
2. 或者使用异常工厂的`rethrow`方法重新抛出异常，保持原始类型

## 5. 最佳实践

### 5.1 定义自定义异常

```cpp
class my_exception : public mc::exception {
public:
    enum code_enum {
        code_value = MY_EXCEPTION_CODE, // 自定义异常代码
    };
    
    my_exception(mc::log_message&& msg = mc::log_message(mc::log_level::error, "我的异常"))
        : exception(std::move(msg), code_value, "my_exception", "我的异常描述")
    {
    }
    
    // 复制构造函数
    my_exception(const my_exception& e) : exception(e) {}
    
    // 移动构造函数
    my_exception(my_exception&& e) : exception(std::move(e)) {}
    
    // 从基类构造（用于异常工厂）
    explicit my_exception(const mc::exception& e) : exception(e) {
        // 可以在这里重置异常代码、名称和描述
    }
    
    // 动态异常处理方法
    virtual std::shared_ptr<mc::exception> dynamic_copy_exception() const override {
        return std::make_shared<my_exception>(*this);
    }
    
    virtual void dynamic_rethrow_exception() const override {
        throw *this;
    }
};
```

### 5.2 注册异常类型

```cpp
// 在程序初始化时注册异常类型
mc::exception_factory::instance().register_exception<my_exception>();
```

注册异常类型后，异常工厂可以根据异常代码创建相应类型的异常对象，这要求每个异常类都必须提供一个从基类`exception`构造的构造函数。

### 5.3 抛出异常

```cpp
// 使用宏抛出异常
MC_THROW(my_exception, "发生了错误");

// 或者直接创建异常对象
mc::log_message msg(mc::log_level::error, "详细错误信息", __FILE__, __LINE__);
throw my_exception(std::move(msg));
```

### 5.4 捕获和处理异常

```cpp
try {
    // 可能抛出异常的代码
} catch (const my_exception& e) {
    // 处理特定类型的异常
    std::cerr << "我的异常: " << e.to_string() << std::endl;
} catch (const mc::exception& e) {
    // 处理所有MC库异常
    std::cerr << "MC异常: " << e.to_detail_string() << std::endl;
} catch (const std::exception& e) {
    // 处理标准库异常
    std::cerr << "标准异常: " << e.what() << std::endl;
} catch (...) {
    // 处理所有未知异常
    std::cerr << "未知异常" << std::endl;
}
```

## 6. 与其他模块的集成

### 6.1 变体模块

变体模块应定义自己的异常类型，如`variant_exception`，继承自`mc::exception`，并使用`variant_error_code`作为异常代码。

### 6.2 字典模块

字典模块应定义自己的异常类型，如`dict_exception`，继承自`mc::exception`，并使用`dict_error_code`作为异常代码。

### 6.3 Future模块

Future模块应定义自己的异常类型，如`future_exception`，继承自`mc::exception`，并使用`future_error_code`作为异常代码。

## 7. 性能考虑

- 异常应该只用于真正的异常情况，不应该用于正常的控制流
- 异常处理可能会影响性能，特别是在性能关键的代码路径上
- 在性能关键的代码中，可以考虑使用返回错误代码的方式代替异常

## 8. 线程安全性

- 异常对象本身是线程安全的，可以在多线程环境中使用
- 异常工厂的`register_exception`方法不是线程安全的，应该在程序初始化时调用
- 异常工厂的`instance`和`rethrow`方法是线程安全的

## 9. 未来扩展

- 添加更多特定模块的异常类型
- 增强异常信息的收集和展示
- 提供异常处理的统计和监控功能
- 支持异常的序列化和反序列化

## 10. 总结

MC库的异常处理方案提供了一个统一、可扩展的异常处理机制，使库的用户能够以一致的方式处理各种错误情况。通过使用这个方案，可以提高代码的可靠性和可维护性，同时提供丰富的错误信息，帮助用户快速定位和解决问题。 