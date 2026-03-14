/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>

#include <mc/exception.h>
#include <mc/runtime/thread_list.h>

// 测试基本异常功能
TEST(ExceptionTest, BasicExceptionTest)
{
    // 创建一个基本异常
    mc::exception e(mc::unhandled_exception_code, "test_exception", "测试异常");

    // 检查异常属性
    EXPECT_EQ(e.code(), mc::unhandled_exception_code);
    EXPECT_EQ(e.name(), "test_exception");
    EXPECT_STREQ(e.what(), "测试异常");

    // 测试异常消息
    EXPECT_EQ(e.top_message(), "测试异常");

    // 测试字符串表示
    std::string str = e.to_string();
    EXPECT_TRUE(str.find("test_exception") != std::string::npos);
    EXPECT_TRUE(str.find("测试异常") != std::string::npos);
}

// 测试所有通用异常代码
TEST(ExceptionTest, AllGeneralExceptionCodesTest)
{
    // 定义测试用例结构体
    struct TestCase {
        int64_t       code;      // 异常代码
        const char*   name;      // 异常名称
        const char*   message;   // 异常消息
        mc::exception exception; // 创建异常的函数
    };

    // 定义测试用例数组
    std::vector<TestCase> test_cases = {
        // 未指定异常
        {mc::unknow_exception_code, "unknow_exception", "Unspecified exception",
         MC_MAKE_EXCEPTION(mc::exception, "Unspecified exception")},
        // 未处理异常
        {mc::unhandled_exception_code, "unhandled", "Unhandled exception",
         MC_MAKE_EXCEPTION(mc::unhandled_exception, "Unhandled exception")},
        // 超时异常
        {mc::timeout_exception_code, "timeout", "Operation timeout",
         MC_MAKE_EXCEPTION(mc::timeout_exception, "Operation timeout")},
        // 文件未找到异常
        {mc::file_not_found_exception_code, "file_not_found", "Configuration file not found",
         MC_MAKE_EXCEPTION(mc::file_not_found_exception, "Configuration file not found")},
        // 解析错误异常
        {mc::parse_error_exception_code, "parse_error", "JSON parsing failed",
         MC_MAKE_EXCEPTION(mc::parse_error_exception, "JSON parsing failed")},
        // 无效参数异常
        {mc::invalid_arg_exception_code, "invalid_arg", "Parameter cannot be empty",
         MC_MAKE_EXCEPTION(mc::invalid_arg_exception, "Parameter cannot be empty")},
        // 键未找到异常
        {mc::key_not_found_exception_code, "key_not_found", "Key 'config' does not exist",
         MC_MAKE_EXCEPTION(mc::key_not_found_exception, "Key 'config' does not exist")},
        // 类型转换异常
        {mc::bad_cast_exception_code, "bad_cast", "Unable to convert string to integer",
         MC_MAKE_EXCEPTION(mc::bad_cast_exception, "Unable to convert string to integer")},
        // 越界异常
        {mc::out_of_range_exception_code, "out_of_range", "Array index out of bounds",
         MC_MAKE_EXCEPTION(mc::out_of_range_exception, "Array index out of bounds")},
        // 取消操作异常
        {mc::canceled_exception_code, "canceled", "Operation canceled by user",
         MC_MAKE_EXCEPTION(mc::canceled_exception, "Operation canceled by user")},
        // 断言异常
        {mc::assert_exception_code, "assert", "Assertion condition not met",
         MC_MAKE_EXCEPTION(mc::assert_exception, "Assertion condition not met")},
        // 文件结束异常
        {mc::eof_exception_code, "eof", "End of file reached",
         MC_MAKE_EXCEPTION(mc::eof_exception, "End of file reached")},
        // 系统错误异常
        {mc::system_error_code, "system", "System call failed",
         MC_MAKE_EXCEPTION(mc::system_exception, "System call failed")},
        // 无效操作异常
        {mc::invalid_op_exception_code, "invalid_operation", "Unsupported operation",
         MC_MAKE_EXCEPTION(mc::invalid_op_exception, "Unsupported operation")},
        // 空可选值异常
        {mc::null_optional_code, "null_optional", "Accessed null optional value",
         MC_MAKE_EXCEPTION(mc::null_optional_exception, "Accessed null optional value")},
        // 溢出异常
        {mc::overflow_code, "overflow", "Numeric overflow",
         MC_MAKE_EXCEPTION(mc::overflow_exception, "Numeric overflow")},
        // 下溢异常
        {mc::underflow_code, "underflow", "Numeric underflow",
         MC_MAKE_EXCEPTION(mc::underflow_exception, "Numeric underflow")},
        // 除零异常
        {mc::divide_by_zero_code, "divide_by_zero", "Division by zero not allowed",
         MC_MAKE_EXCEPTION(mc::divide_by_zero_exception, "Division by zero not allowed")}};

    // 遍历测试用例数组执行测试
    for (const auto& test_case : test_cases) {
        SCOPED_TRACE(std::string("Testing exception: ") + test_case.name);

        auto e = test_case.exception;
        EXPECT_EQ(e.code(), test_case.code);
        EXPECT_EQ(e.name(), test_case.name);
        EXPECT_TRUE(e.to_string().find(test_case.message) != std::string::npos);
    }
}

TEST(ExceptionTest, ThrowExceptionTest)
{
    try {
        MC_THROW(mc::bad_cast_exception, "Type conversion exception: expected type ${expected}, actual type ${actual}",
                 ("expected", "int")("actual", "bool"));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.code(), mc::bad_cast_exception_code);
        EXPECT_EQ(e.name(), "bad_cast");
        EXPECT_TRUE(e.to_string().find("int") != std::string::npos);
    }
}

// 测试异常的格式化消息
TEST(ExceptionTest, FormattedMessageTest)
{
    // 测试基本的格式化消息
    try {
        MC_THROW(mc::bad_cast_exception, "Type conversion exception: expected type ${expected}, actual type ${actual}",
                 ("expected", "int")("actual", "bool"));
        FAIL() << "应该抛出异常";
    } catch (const mc::bad_cast_exception& e) {
        EXPECT_EQ(e.code(), mc::exception_code::bad_cast_exception_code);
        EXPECT_EQ(e.name(), "bad_cast");
        std::string msg = e.to_string();
        EXPECT_TRUE(msg.find("int") != std::string::npos);
        EXPECT_TRUE(msg.find("bool") != std::string::npos);
    }

    // 测试复杂的格式化消息
    try {
        MC_THROW(mc::invalid_arg_exception, "参数验证失败: ${param} 的值 ${value} 不在有效范围 [${min}, ${max}] 内",
                 ("param", "age")("value", "150")("min", "0")("max", "120"));
        FAIL() << "应该抛出异常";
    } catch (const mc::invalid_arg_exception& e) {
        std::string msg = e.to_string();
        EXPECT_TRUE(msg.find("age") != std::string::npos);
        EXPECT_TRUE(msg.find("150") != std::string::npos);
        EXPECT_TRUE(msg.find("0") != std::string::npos);
        EXPECT_TRUE(msg.find("120") != std::string::npos);
    }
}

// 测试异常的嵌套和重抛
TEST(ExceptionTest, NestingAndRethrowTest)
{
    try {
        try {
            try {
                // 最内层抛出异常
                MC_THROW(mc::file_not_found_exception, "Configuration file ${file} does not exist",
                         ("file", "config.json"));
            } catch (const mc::file_not_found_exception& e) {
                // 中层捕获并添加上下文后重抛
                MC_RETHROW_EXCEPTION(e, "Failed to load configuration file: ${reason}", ("reason", e.what()));
            }
        } catch (const mc::exception& e) {
            // 外层捕获并添加更多上下文后重抛
            MC_RETHROW_EXCEPTION(e, "System initialization failed: ${reason}", ("reason", e.what()));
        }
        FAIL() << "应该抛出异常";
    } catch (const mc::exception& e) {
        std::string detail = e.to_detail_string();
        // 验证所有层级的消息都被保留
        EXPECT_TRUE(detail.find("config.json") != std::string::npos);
        EXPECT_TRUE(detail.find("Failed to load configuration file") != std::string::npos);
        EXPECT_TRUE(detail.find("System initialization failed") != std::string::npos);
    }
}

// 测试异常的日志级别过滤
TEST(ExceptionTest, LogLevelFilterTest)
{
    mc::exception e(mc::exception_code::unknow_exception_code, "test", "测试异常");

    // 添加不同级别的日志
    e.append_log(MC_LOG_MESSAGE(debug, "调试信息"));
    e.append_log(MC_LOG_MESSAGE(info, "普通信息"));
    e.append_log(MC_LOG_MESSAGE(warn, "警告信息"));
    e.append_log(MC_LOG_MESSAGE(error, "错误信息"));

    // 测试不同日志级别的过滤
    EXPECT_TRUE(e.to_detail_string(mc::log::level::error).find("调试信息") == std::string::npos);
    EXPECT_TRUE(e.to_detail_string(mc::log::level::error).find("普通信息") == std::string::npos);
    EXPECT_TRUE(e.to_detail_string(mc::log::level::error).find("警告信息") == std::string::npos);
    EXPECT_TRUE(e.to_detail_string(mc::log::level::error).find("错误信息") != std::string::npos);

    EXPECT_TRUE(e.to_detail_string(mc::log::level::warn).find("调试信息") == std::string::npos);
    EXPECT_TRUE(e.to_detail_string(mc::log::level::warn).find("普通信息") == std::string::npos);
    EXPECT_TRUE(e.to_detail_string(mc::log::level::warn).find("警告信息") != std::string::npos);
    EXPECT_TRUE(e.to_detail_string(mc::log::level::warn).find("错误信息") != std::string::npos);
}

// 测试异常的 setter 与消息转移
TEST(ExceptionTest, SettersAndTakeMessages)
{
    mc::exception ex(mc::exception_code::unknow_exception_code, "original", "原始异常");

    ex.set_code(1234);
    ex.set_name("custom.exception");

    ex.append_log(MC_LOG_MESSAGE(info, "第一条日志: ${value}", ("value", 1)));

    mc::log::messages extra_logs;
    extra_logs.emplace_back(MC_LOG_MESSAGE(warn, "第二条日志: ${value}", ("value", 2)));
    ex.append_log(std::move(extra_logs));

    EXPECT_EQ(ex.code(), 1234);
    EXPECT_EQ(ex.name(), "custom.exception");
    EXPECT_EQ(ex.messages().size(), 2);

    auto moved_logs = ex.take_messages();
    EXPECT_EQ(moved_logs.size(), 2);
    EXPECT_TRUE(ex.messages().empty());

    // 再次设置名称，确认 setter 工作正常
    ex.set_name("custom.exception.updated");
    EXPECT_EQ(ex.name(), "custom.exception.updated");
}

// 测试使用消息向量构造异常（右值引用）
TEST(ExceptionTest, ConstructWithMessageVectorRvalue)
{
    mc::log::messages logs;
    logs.emplace_back(MC_LOG_MESSAGE(info, "日志1", ("index", 1)));
    logs.emplace_back(MC_LOG_MESSAGE(warn, "日志2", ("index", 2)));

    mc::exception ex(std::move(logs), 2001, "vector.exception", "通过消息向量构造");

    EXPECT_EQ(ex.code(), 2001);
    EXPECT_EQ(ex.name(), "vector.exception");
    EXPECT_EQ(ex.messages().size(), 2);
    EXPECT_TRUE(logs.empty()); // 构造函数应接管消息所有权
}

// 测试使用消息向量构造异常（常量引用）
TEST(ExceptionTest, ConstructWithMessageVectorConstRef)
{
    mc::log::messages logs;
    logs.emplace_back(MC_LOG_MESSAGE(info, "常量日志", ("flag", true)));

    const auto    backup_size = logs.size();
    mc::exception ex(logs, 2002, "vector.const.exception", "常量引用构造");

    EXPECT_EQ(ex.messages().size(), backup_size);
    EXPECT_EQ(ex.top_message(), logs.back().get_message());
    EXPECT_EQ(logs.size(), backup_size); // 原始向量不应被修改
}

// 测试基础异常的动态接口
TEST(ExceptionTest, BaseClassDynamicOperations)
{
    mc::exception ex(mc::exception_code::unknow_exception_code, "base.exception", "基础异常");

    auto copied = ex.dynamic_copy_exception();
    ASSERT_TRUE(copied);
    EXPECT_NE(copied.get(), &ex);
    EXPECT_EQ(copied->name(), ex.name());

    EXPECT_THROW(ex.dynamic_rethrow_exception(), mc::exception);
}

// 测试未处理异常的消息向量构造函数
TEST(ExceptionTest, UnhandledExceptionFromMessages)
{
    mc::log::messages logs;
    logs.emplace_back(MC_LOG_MESSAGE(error, "未处理异常: ${code}", ("code", 500)));
    logs.emplace_back(MC_LOG_MESSAGE(error, "详细原因: ${msg}", ("msg", "unknown")));

    mc::unhandled_exception unhandled(logs);

    EXPECT_EQ(unhandled.code(), mc::unhandled_exception::code_value);
    EXPECT_EQ(unhandled.top_message(), logs.back().get_message());
    EXPECT_EQ(unhandled.messages().size(), logs.size());
}

// 测试标准异常包装器的内部异常访问
TEST(ExceptionTest, StdExceptionWrapperInnerAccess)
{
    try {
        throw std::runtime_error("std runtime");
    } catch (const std::exception& std_ex) {
        auto wrapper = mc::std_exception_wrapper::from_current_exception(std_ex);
        auto inner   = wrapper.get_inner_exception();
        ASSERT_TRUE(inner);

        bool caught = false;
        try {
            wrapper.dynamic_rethrow_exception();
        } catch (const std::exception& rethrown) {
            caught = true;
            EXPECT_STREQ(rethrown.what(), "std runtime");
        }
        EXPECT_TRUE(caught);
    }
}

// 测试异常的动态复制和重抛
TEST(ExceptionTest, DynamicCopyAndRethrowTest)
{
    try {
        // 创建一个超时异常
        auto original = MC_MAKE_EXCEPTION(mc::timeout_exception, "Operation timeout: ${operation}",
                                          ("operation", "database query"));

        // 动态复制异常
        std::shared_ptr<mc::exception> copy = original.dynamic_copy_exception();

        // 验证复制的异常
        EXPECT_EQ(copy->code(), mc::exception_code::timeout_exception_code);
        EXPECT_EQ(copy->name(), "timeout");
        EXPECT_TRUE(copy->to_string().find("database query") != std::string::npos);

        // 动态重抛异常
        copy->dynamic_rethrow_exception();
        FAIL() << "应该重新抛出异常";
    } catch (const mc::timeout_exception& e) {
        // 验证重抛的异常保持了正确的类型和信息
        EXPECT_EQ(e.code(), mc::exception_code::timeout_exception_code);
        EXPECT_EQ(e.name(), "timeout");
        EXPECT_TRUE(e.to_string().find("database query") != std::string::npos);
    }
}

// 测试异常的线程安全性
TEST(ExceptionTest, ThreadSafetyTest)
{
    // 创建一个共享的异常对象
    mc::exception shared_exception(mc::exception_code::unknow_exception_code, "thread_test", "线程测试异常");

    // 创建多个线程，每个线程都添加日志消息
    const int                num_threads     = 10;
    const int                msgs_per_thread = 100;
    mc::runtime::thread_list threads;
    std::mutex               mutex;

    threads.start_threads(num_threads, [&shared_exception, &mutex](std::size_t i) {
        for (int j = 0; j < msgs_per_thread; ++j) {
            // 使用互斥锁保护共享对象
            std::lock_guard<std::mutex> lock(mutex);
            shared_exception.append_log(MC_LOG_MESSAGE(info, "线程 ${thread} 消息 ${msg}",
                                                       ("thread", std::to_string(i))("msg", std::to_string(j))));
        }
    });

    threads.join_all();

    // 验证所有日志消息都被正确添加
    std::string detail    = shared_exception.to_detail_string();
    int         msg_count = 0;
    for (int i = 0; i < num_threads; ++i) {
        for (int j = 0; j < msgs_per_thread; ++j) {
            std::string msg = "线程 " + std::to_string(i) + " 消息 " + std::to_string(j);
            if (detail.find(msg) != std::string::npos) {
                msg_count++;
            }
        }
    }
    EXPECT_EQ(msg_count, num_threads * msgs_per_thread);
}

// 测试异常的性能
TEST(ExceptionTest, DISABLED_PerformanceTest)
{
    const int iterations = 10000;

    // 测试异常创建性能
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto e = MC_MAKE_EXCEPTION(mc::timeout_exception, "Test message ${index}", ("index", std::to_string(i)));
            EXPECT_EQ(e.code(), mc::exception_code::timeout_exception_code);
        }
        auto end      = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "创建 " << iterations << " 个异常耗时: " << duration << " 毫秒" << std::endl;
    }

    // 测试异常抛出和捕获性能
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            try {
                MC_THROW(mc::timeout_exception, "Test message ${index}", ("index", std::to_string(i)));
            } catch (const mc::exception& e) {
                EXPECT_EQ(e.code(), mc::exception_code::timeout_exception_code);
            }
        }
        auto end      = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "抛出和捕获 " << iterations << " 个异常耗时: " << duration << " 毫秒" << std::endl;
    }
}

// 测试标准库异常的包装
TEST(ExceptionTest, StdExceptionWrapperTest)
{
    try {
        try {
            throw std::runtime_error("Standard library runtime error");
        } catch (const std::exception& e) {
            MC_THROW(mc::system_exception, "System error: ${error}", ("error", e.what()));
        }
        FAIL() << "应该抛出异常";
    } catch (const mc::system_exception& e) {
        EXPECT_EQ(e.code(), mc::exception_code::system_error_code);
        EXPECT_EQ(e.name(), "system");
        EXPECT_TRUE(e.to_string().find("Standard library runtime error") != std::string::npos);
    }
}

// 测试异常的格式化输出
TEST(ExceptionTest, FormattedOutputTest)
{
    // 创建一个带有完整上下文信息的异常
    auto e = MC_MAKE_EXCEPTION(mc::invalid_arg_exception, "Parameter validation failed: ${param} = ${value}",
                               ("param", "count")("value", "-1"));

    e.append_log(MC_LOG_MESSAGE(debug, "Parameter details: ${detail}", ("detail", "must be greater than 0")));
    e.append_log(MC_LOG_MESSAGE(info, "Processing function: ${func}", ("func", "process_data")));
    e.append_log(MC_LOG_MESSAGE(warn, "Possible impact: ${impact}", ("impact", "incomplete data")));
    e.append_log(MC_LOG_MESSAGE(error, "Error code: ${code}", ("code", "E001")));

    // 测试简单格式输出
    std::string simple = e.to_string();
    EXPECT_TRUE(simple.find("Error code") != std::string::npos);
    EXPECT_TRUE(simple.find("E001") != std::string::npos);

    // 测试详细格式输出
    std::string detail = e.to_detail_string();
    EXPECT_TRUE(detail.find("must be greater than 0") != std::string::npos);
    EXPECT_TRUE(detail.find("process_data") != std::string::npos);
    EXPECT_TRUE(detail.find("incomplete data") != std::string::npos);
    EXPECT_TRUE(detail.find("E001") != std::string::npos);

    // 验证日志级别和时间戳格式
    EXPECT_TRUE(detail.find("DEBUG") != std::string::npos);
    EXPECT_TRUE(detail.find("INFO") != std::string::npos);
    EXPECT_TRUE(detail.find("WARN") != std::string::npos);
    EXPECT_TRUE(detail.find("ERROR") != std::string::npos);
    EXPECT_TRUE(detail.find("-") != std::string::npos); // 日期分隔符
    EXPECT_TRUE(detail.find(":") != std::string::npos); // 时间分隔符
}

/*
早期异常只支持 ${} 命名参数，为了使用简单我们将日志格式化与 sformat 保持一致，
即支持 {} 占位也支持 ${} 命名占位符，其中 {} 既可以索引占位也可以命名占位，${} 占位
只是兼容旧的写法，建议的异常法全部是 {} 占位符
*/
TEST(ExceptionTest, basic_smart_exception)
{
    // 测试自增索引 {}
    try {
        MC_THROW(mc::runtime_exception, "{} {}", "Hello", "World");
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "Hello World");
    }

    try {
        MC_THROW(mc::runtime_exception, "{1} {0}", "World", "Hello");
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "Hello World");
    }

    try {
        MC_THROW(mc::runtime_exception, "{name} {value}", ("name", "Answer"), ("value", 42));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "Answer 42");
    }
}

// 测试混合使用不同类型的占位符
TEST(ExceptionTest, mixed_placeholder_types)
{
    // 混合自增索引和命名参数
    try {
        MC_THROW(mc::runtime_exception, "{} is {age} years old", "Alice", ("age", 25));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "Alice is 25 years old");
    }

    // 混合显式索引和命名参数
    try {
        MC_THROW(mc::runtime_exception, "{0} has {balance:.2f} dollars", "Bob", ("balance", 123.456));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "Bob has 123.46 dollars");
    }

    // 混合所有类型
    try {
        MC_THROW(mc::runtime_exception, "{} {name} {2}", "Hello", ("name", "Test"), "World"); // 命名参数也会增加索引
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "Hello Test World");
    }
}

// 测试带格式说明符的智能占位符
TEST(ExceptionTest, format_specifications)
{
    // 自增索引带格式
    try {
        MC_THROW(mc::runtime_exception, "{:.2f} {}", 3.14159, "pi");
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "3.14 pi");
    }

    // 显式索引带格式
    try {
        MC_THROW(mc::runtime_exception, "{0:.2f} {1:>10}", 3.14159, "right");
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "3.14      right");
    }

    // 命名参数带格式
    try {
        MC_THROW(mc::runtime_exception, "{name:<10} {value:04d}", ("name", "Number"), ("value", 42));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "Number     0042");
    }
}

// 测试动态格式参数
TEST(ExceptionTest, dynamic_format_parameters)
{
    // 使用命名参数作为动态宽度和精度
    try {
        MC_THROW(mc::runtime_exception, "{value:{width}.{precision}f}", ("value", 3.14159), ("width", 10),
                 ("precision", 3));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "     3.142");
    }

    // 混合索引和命名参数的动态格式
    try {
        MC_THROW(mc::runtime_exception, "{0:{width}.2f}", 3.14159, ("width", 8));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "    3.14");
    }
}

// 测试边界情况
TEST(ExceptionTest, edge_cases)
{
    // 单个字符的命名参数
    try {
        MC_THROW(mc::runtime_exception, "{x} {y}", ("x", 10), ("y", 20));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "10 20");
    }

    // 长命名参数
    try {
        MC_THROW(mc::runtime_exception, "{very_long_parameter_name}", ("very_long_parameter_name", "value"));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "value");
    }

    // 提供了3个参数但格式化字符串只用了一部分
    try {
        MC_THROW_UNSAFE(mc::runtime_exception, "{2}", "first", "second", "target_value");
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "target_value");
    }

    // 格式化字符串不是字面值常量而是变量
    const char* fmt = "{x} {y}";
    try {
        MC_THROW_UNSAFE(mc::runtime_exception, fmt, ("x", 10), ("y", 20));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "10 20");
    }
}

// 测试嵌套大括号
TEST(ExceptionTest, nested_braces)
{
    // 命名参数中的嵌套大括号格式
    try {
        MC_THROW(mc::runtime_exception, "{value:{width}.{precision}f}", ("value", 123.456), ("width", 12),
                 ("precision", 2));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "      123.46");
    }

    // 索引参数中的嵌套大括号格式
    try {
        MC_THROW(mc::runtime_exception, "{0:{1}.{2}f}", 123.456, 12, 2);
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "      123.46");
    }
}

// 测试转义字符
TEST(ExceptionTest, escaped_braces)
{
    // 转义的大括号与智能占位符混合
    try {
        MC_THROW(mc::runtime_exception, "{{}} {name} {{}}", ("name", "test"));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "{} test {}");
    }

    try {
        MC_THROW(mc::runtime_exception, "{{0}} {0} {{name}}", "value");
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.top_message(), "{0} value {name}");
    }
}

// 复杂场景：异常消息的复杂格式化
TEST(ExceptionTest, ComplexExceptionMessageFormatting)
{
    // 多层嵌套的格式化参数
    try {
        MC_THROW(mc::runtime_exception,
                 "操作失败: 用户${user}在${time}尝试${action}, "
                 "参数: ${param1}=${value1}, ${param2}=${value2}",
                 ("user", "admin")("time", "2024-01-01 12:00:00")("action", "update")("param1", "id")("value1", 123)(
                     "param2", "status")("value2", "active"));
    } catch (const mc::exception& e) {
        std::string msg = e.top_message();
        EXPECT_NE(msg.find("admin"), std::string::npos);
        EXPECT_NE(msg.find("2024-01-01"), std::string::npos);
        EXPECT_NE(msg.find("update"), std::string::npos);
        EXPECT_NE(msg.find("123"), std::string::npos);
        EXPECT_NE(msg.find("active"), std::string::npos);
    }

    // 混合索引和命名参数
    try {
        MC_THROW(mc::runtime_exception, "{0} {name} {1} {value}", "prefix", "suffix", ("name", "middle")("value", 42));
    } catch (const mc::exception& e) {
        std::string msg = e.top_message();
        EXPECT_NE(msg.find("prefix"), std::string::npos);
        EXPECT_NE(msg.find("middle"), std::string::npos);
        EXPECT_NE(msg.find("suffix"), std::string::npos);
        EXPECT_NE(msg.find("42"), std::string::npos);
    }
}

// 复杂场景：未处理异常的包装
TEST(ExceptionTest, ComplexUnhandledExceptionWrapper)
{
    // 包装标准异常
    try {
        throw std::logic_error("逻辑错误");
    } catch (const std::exception& std_ex) {
        mc::unhandled_exception unhandled(MC_LOG_MESSAGE(error, "未处理的异常: ${error}", ("error", std_ex.what())),
                                          std::current_exception());

        EXPECT_EQ(unhandled.name(), "unhandled");
        EXPECT_EQ(unhandled.code(), mc::unhandled_exception_code);
        EXPECT_NE(std::string(unhandled.to_string()).find("逻辑错误"), std::string::npos);

        // 验证可以获取内部异常
        auto inner = unhandled.get_inner_exception();
        ASSERT_TRUE(inner);
        try {
            std::rethrow_exception(inner);
        } catch (const std::logic_error& e) {
            EXPECT_STREQ(e.what(), "逻辑错误");
        }
    }
}

// 测试 exception(int64_t, const std::string&, const std::string&) 构造函数
TEST(ExceptionTest, ExceptionCodeNameWhatConstructor)
{
    mc::exception ex(1234, "test_exception", "测试异常消息");
    EXPECT_EQ(ex.code(), 1234);
    EXPECT_EQ(ex.name(), "test_exception");
    EXPECT_TRUE(ex.to_string().find("测试异常消息") != std::string::npos);
}

// 测试 exception(mc::log::message&&, ...) 构造函数
TEST(ExceptionTest, ExceptionMessageRvalueConstructor)
{
    auto          msg = MC_LOG_MESSAGE(info, "测试日志消息", ("key", "value"));
    mc::exception ex(std::move(msg), 2001, "test", "测试");
    EXPECT_EQ(ex.code(), 2001);
    EXPECT_EQ(ex.name(), "test");
    EXPECT_EQ(ex.messages().size(), 1);
}

// 测试 exception(const mc::log::messages&, ...) 构造函数
TEST(ExceptionTest, ExceptionMessagesConstRefConstructor)
{
    mc::log::messages msgs;
    msgs.emplace_back(MC_LOG_MESSAGE(info, "日志1", ("index", 1)));
    msgs.emplace_back(MC_LOG_MESSAGE(warn, "日志2", ("index", 2)));

    const auto    backup_size = msgs.size();
    mc::exception ex(msgs, 2002, "test", "测试");
    EXPECT_EQ(ex.code(), 2002);
    EXPECT_EQ(ex.name(), "test");
    EXPECT_EQ(ex.messages().size(), backup_size);
    EXPECT_EQ(msgs.size(), backup_size); // 原始向量不应被修改
}

// 测试 exception(const exception&) 复制构造函数
TEST(ExceptionTest, ExceptionCopyConstructor)
{
    mc::exception original(1001, "original", "原始异常");
    original.append_log(MC_LOG_MESSAGE(info, "原始日志", ("id", 1)));

    mc::exception copy(original);
    EXPECT_EQ(copy.code(), original.code());
    EXPECT_EQ(copy.name(), original.name());
    EXPECT_EQ(copy.messages().size(), original.messages().size());
    EXPECT_NE(copy.messages().data(), original.messages().data()); // 应该是深拷贝
}

// 测试 append_log 中 msgs.empty() 的路径
TEST(ExceptionTest, AppendLogEmptyMessages)
{
    mc::exception     ex(1002, "test", "测试");
    mc::log::messages empty_msgs;
    ex.append_log(empty_msgs);
    EXPECT_EQ(ex.messages().size(), 0);
}

// 测试 to_detail_string 的详细实现
TEST(ExceptionTest, ToDetailStringDetailed)
{
    mc::exception ex(1003, "detail_test", "详细测试");
    ex.append_log(MC_LOG_MESSAGE(debug, "调试信息", ("level", "debug")));
    ex.append_log(MC_LOG_MESSAGE(info, "普通信息", ("level", "info")));
    ex.append_log(MC_LOG_MESSAGE(warn, "警告信息", ("level", "warn")));
    ex.append_log(MC_LOG_MESSAGE(error, "错误信息", ("level", "error")));
    ex.append_log(MC_LOG_MESSAGE(notice, "通知信息", ("level", "notice")));

    std::string detail = ex.to_detail_string(mc::log::level::debug);
    EXPECT_NE(detail.find("1003"), std::string::npos);
    EXPECT_NE(detail.find("detail_test"), std::string::npos);
    EXPECT_NE(detail.find("详细测试"), std::string::npos);
    EXPECT_NE(detail.find("DEBUG"), std::string::npos);
    EXPECT_NE(detail.find("INFO"), std::string::npos);
    EXPECT_NE(detail.find("WARN"), std::string::npos);
    EXPECT_NE(detail.find("ERROR"), std::string::npos);
    EXPECT_NE(detail.find("NOTICE"), std::string::npos);
}

// 测试 to_string 方法
TEST(ExceptionTest, ToStringMethod)
{
    mc::exception ex(1004, "to_string_test", "to_string 测试");
    ex.append_log(MC_LOG_MESSAGE(info, "日志消息", ("key", "value")));

    std::string str = ex.to_string();
    EXPECT_NE(str.find("to_string_test"), std::string::npos);
    EXPECT_NE(str.find("to_string 测试"), std::string::npos);
}

// 测试 to_string 方法带日志级别过滤
TEST(ExceptionTest, ToStringWithLogLevel)
{
    mc::exception ex(1005, "level_test", "级别测试");
    ex.append_log(MC_LOG_MESSAGE(debug, "调试", ("id", 1)));
    ex.append_log(MC_LOG_MESSAGE(info, "信息", ("id", 2)));
    ex.append_log(MC_LOG_MESSAGE(warn, "警告", ("id", 3)));

    std::string str_warn = ex.to_string(mc::log::level::warn);
    EXPECT_NE(str_warn.find("警告"), std::string::npos);
    EXPECT_EQ(str_warn.find("调试"), std::string::npos);
    EXPECT_EQ(str_warn.find("信息"), std::string::npos);
}

// 测试 dynamic_rethrow_exception
TEST(ExceptionTest, DynamicRethrowException)
{
    mc::exception ex(1006, "rethrow_test", "重抛测试");
    try {
        ex.dynamic_rethrow_exception();
        FAIL() << "应该抛出异常";
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.code(), 1006);
        EXPECT_EQ(e.name(), "rethrow_test");
    }
}

// 注意：UnhandledExceptionMessageRvalueConstructor 的功能已在 ComplexUnhandledExceptionWrapper
// 测试中覆盖，因此不再重复添加

// 测试 unhandled_exception(const exception&) 构造函数
TEST(ExceptionTest, UnhandledExceptionFromException)
{
    mc::exception           original(2001, "original", "原始异常");
    mc::unhandled_exception unhandled(original);
    EXPECT_EQ(unhandled.code(), mc::unhandled_exception_code);
    EXPECT_EQ(unhandled.name(), "unhandled");
}

// 测试 unhandled_exception::dynamic_rethrow_exception 有内部异常的情况
TEST(ExceptionTest, UnhandledExceptionDynamicRethrowWithInner)
{
    try {
        throw std::logic_error("逻辑错误");
    } catch (const std::exception&) {
        auto                    msg = MC_LOG_MESSAGE(error, "未处理", ("type", "logic_error"));
        mc::unhandled_exception unhandled(std::move(msg), std::current_exception());
        try {
            unhandled.dynamic_rethrow_exception();
            FAIL() << "应该重新抛出内部异常";
        } catch (const std::logic_error& e) {
            EXPECT_STREQ(e.what(), "逻辑错误");
        }
    }
}

// 测试 unhandled_exception::dynamic_rethrow_exception 无内部异常的情况
TEST(ExceptionTest, UnhandledExceptionDynamicRethrowWithoutInner)
{
    mc::exception           original(2002, "original", "原始");
    mc::unhandled_exception unhandled(original);
    try {
        unhandled.dynamic_rethrow_exception();
        FAIL() << "应该抛出异常";
    } catch (const mc::unhandled_exception& e) {
        EXPECT_EQ(e.code(), mc::unhandled_exception_code);
    }
}

// 测试 unhandled_exception::dynamic_copy_exception
TEST(ExceptionTest, UnhandledExceptionDynamicCopy)
{
    try {
        throw std::runtime_error("运行时错误");
    } catch (const std::exception&) {
        auto                    msg = MC_LOG_MESSAGE(error, "未处理", ("type", "runtime_error"));
        mc::unhandled_exception original(std::move(msg), std::current_exception());
        auto                    copy = original.dynamic_copy_exception();
        ASSERT_TRUE(copy);
        EXPECT_EQ(copy->code(), mc::unhandled_exception_code);
        EXPECT_EQ(copy->name(), "unhandled");
    }
}

// 注意：StdExceptionWrapperFromCurrentException 和 StdExceptionWrapperDynamicRethrowWithInner
// 的功能已在 StdExceptionWrapperInnerAccess 测试中覆盖，因此不再重复添加

// 测试 std_exception_wrapper::dynamic_rethrow_exception 无内部异常的情况
TEST(ExceptionTest, StdExceptionWrapperDynamicRethrowWithoutInner)
{
    mc::log::message          msg(mc::log::level::error, "测试");
    mc::std_exception_wrapper wrapper(std::move(msg), nullptr, "test", "测试");
    try {
        wrapper.dynamic_rethrow_exception();
        FAIL() << "应该抛出异常";
    } catch (const mc::std_exception_wrapper& e) {
        EXPECT_EQ(e.name(), "test");
    }
}

// 测试 std_exception_wrapper::to_detail_string
TEST(ExceptionTest, StdExceptionWrapperToDetailString)
{
    try {
        throw std::bad_alloc();
    } catch (const std::exception& e) {
        auto        wrapper = mc::std_exception_wrapper::from_current_exception(e);
        std::string detail  = wrapper.to_detail_string(mc::log::level::error);
        EXPECT_NE(detail.find("std_exception"), std::string::npos);
        EXPECT_NE(detail.find("内部异常"), std::string::npos);
    }
}

// 测试 std_exception_wrapper::to_detail_string 捕获未知异常
TEST(ExceptionTest, StdExceptionWrapperToDetailStringUnknownException)
{
    try {
        throw 42; // 抛出非标准异常
    } catch (...) {
        mc::log::message          msg(mc::log::level::error, "未知异常");
        mc::std_exception_wrapper wrapper(std::move(msg), std::current_exception(), "test", "测试");
        std::string               detail = wrapper.to_detail_string(mc::log::level::error);
        EXPECT_NE(detail.find("内部异常: 未知类型"), std::string::npos);
    }
}
