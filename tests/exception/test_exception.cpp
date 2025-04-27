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

#include <chrono>
#include <functional>
#include <gtest/gtest.h>
#include <mc/exception.h>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// 测试基本异常功能
TEST(ExceptionTest, BasicExceptionTest) {
    // 创建一个基本异常
    mc::exception e(mc::unhandled_exception_code, "test_exception", "测试异常");

    // 检查异常属性
    EXPECT_EQ(e.code(), mc::unhandled_exception_code);
    EXPECT_STREQ(e.name(), "test_exception");
    EXPECT_STREQ(e.what(), "测试异常");

    // 测试异常消息
    EXPECT_EQ(e.top_message(), "测试异常");

    // 测试字符串表示
    std::string str = e.to_string();
    EXPECT_TRUE(str.find("test_exception") != std::string::npos);
    EXPECT_TRUE(str.find("测试异常") != std::string::npos);
}

// 测试所有通用异常代码
TEST(ExceptionTest, AllGeneralExceptionCodesTest) {
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
        {mc::unknow_exception_code, "unknow_exception", "未指定异常",
         MC_MAKE_EXCEPTION(mc::exception, "未指定异常")},
        // 未处理异常
        {mc::unhandled_exception_code, "unhandled", "未处理异常",
         MC_MAKE_EXCEPTION(mc::unhandled_exception, "未处理异常")},
        // 超时异常
        {mc::timeout_exception_code, "timeout", "操作超时",
         MC_MAKE_EXCEPTION(mc::timeout_exception, "操作超时")},
        // 文件未找到异常
        {mc::file_not_found_exception_code, "file_not_found", "找不到配置文件",
         MC_MAKE_EXCEPTION(mc::file_not_found_exception, "找不到配置文件")},
        // 解析错误异常
        {mc::parse_error_exception_code, "parse_error", "JSON解析失败",
         MC_MAKE_EXCEPTION(mc::parse_error_exception, "JSON解析失败")},
        // 无效参数异常
        {mc::invalid_arg_exception_code, "invalid_arg", "参数不能为空",
         MC_MAKE_EXCEPTION(mc::invalid_arg_exception, "参数不能为空")},
        // 键未找到异常
        {mc::key_not_found_exception_code, "key_not_found", "键'config'不存在",
         MC_MAKE_EXCEPTION(mc::key_not_found_exception, "键'config'不存在")},
        // 类型转换异常
        {mc::bad_cast_exception_code, "bad_cast", "无法将字符串转换为整数",
         MC_MAKE_EXCEPTION(mc::bad_cast_exception, "无法将字符串转换为整数")},
        // 越界异常
        {mc::out_of_range_exception_code, "out_of_range", "数组索引越界",
         MC_MAKE_EXCEPTION(mc::out_of_range_exception, "数组索引越界")},
        // 取消操作异常
        {mc::canceled_exception_code, "canceled", "操作被用户取消",
         MC_MAKE_EXCEPTION(mc::canceled_exception, "操作被用户取消")},
        // 断言异常
        {mc::assert_exception_code, "assert", "断言条件不满足",
         MC_MAKE_EXCEPTION(mc::assert_exception, "断言条件不满足")},
        // 文件结束异常
        {mc::eof_exception_code, "eof", "已到达文件末尾",
         MC_MAKE_EXCEPTION(mc::eof_exception, "已到达文件末尾")},
        // 系统错误异常
        {mc::system_error_code, "system", "系统调用失败",
         MC_MAKE_EXCEPTION(mc::system_exception, "系统调用失败")},
        // 无效操作异常
        {mc::invalid_op_exception_code, "invalid_operation", "不支持的操作",
         MC_MAKE_EXCEPTION(mc::invalid_op_exception, "不支持的操作")},
        // 空可选值异常
        {mc::null_optional_code, "null_optional", "访问了空的可选值",
         MC_MAKE_EXCEPTION(mc::null_optional_exception, "访问了空的可选值")},
        // 溢出异常
        {mc::overflow_code, "overflow", "数值溢出",
         MC_MAKE_EXCEPTION(mc::overflow_exception, "数值溢出")},
        // 下溢异常
        {mc::underflow_code, "underflow", "数值下溢",
         MC_MAKE_EXCEPTION(mc::underflow_exception, "数值下溢")},
        // 除零异常
        {mc::divide_by_zero_code, "divide_by_zero", "除数不能为零",
         MC_MAKE_EXCEPTION(mc::divide_by_zero_exception, "除数不能为零")}};

    // 遍历测试用例数组执行测试
    for (const auto& test_case : test_cases) {
        SCOPED_TRACE(std::string("Testing exception: ") + test_case.name);

        auto e = test_case.exception;
        EXPECT_EQ(e.code(), test_case.code);
        EXPECT_STREQ(e.name(), test_case.name);
        EXPECT_TRUE(e.to_string().find(test_case.message) != std::string::npos);
    }
}

TEST(ExceptionTest, ThrowExceptionTest) {
    try {
        MC_THROW(mc::bad_cast_exception, "类型转换异常: 期望类型 ${expected}, 实际类型 ${actual}",
                 ("expected", "int")("actual", "bool"));
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.code(), mc::bad_cast_exception_code);
        EXPECT_STREQ(e.name(), "bad_cast");
        EXPECT_TRUE(e.to_string().find("int") != std::string::npos);
    }
}

// 测试异常的格式化消息
TEST(ExceptionTest, FormattedMessageTest) {
    // 测试基本的格式化消息
    try {
        MC_THROW(mc::bad_cast_exception, "类型转换异常: 期望类型 ${expected}, 实际类型 ${actual}",
                 ("expected", "int")("actual", "bool"));
        FAIL() << "应该抛出异常";
    } catch (const mc::bad_cast_exception& e) {
        EXPECT_EQ(e.code(), mc::exception_code::bad_cast_exception_code);
        EXPECT_STREQ(e.name(), "bad_cast");
        std::string msg = e.to_string();
        EXPECT_TRUE(msg.find("int") != std::string::npos);
        EXPECT_TRUE(msg.find("bool") != std::string::npos);
    }

    // 测试复杂的格式化消息
    try {
        MC_THROW(mc::invalid_arg_exception,
                 "参数验证失败: ${param} 的值 ${value} 不在有效范围 [${min}, ${max}] 内",
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
TEST(ExceptionTest, NestingAndRethrowTest) {
    try {
        try {
            try {
                // 最内层抛出异常
                MC_THROW(mc::file_not_found_exception, "配置文件 ${file} 不存在",
                         ("file", "config.json"));
            } catch (const mc::file_not_found_exception& e) {
                // 中层捕获并添加上下文后重抛
                MC_RETHROW_EXCEPTION(e, "加载配置文件失败: ${reason}", ("reason", e.what()));
            }
        } catch (const mc::exception& e) {
            // 外层捕获并添加更多上下文后重抛
            MC_RETHROW_EXCEPTION(e, "系统初始化失败: ${reason}", ("reason", e.what()));
        }
        FAIL() << "应该抛出异常";
    } catch (const mc::exception& e) {
        std::string detail = e.to_detail_string();
        // 验证所有层级的消息都被保留
        EXPECT_TRUE(detail.find("config.json") != std::string::npos);
        EXPECT_TRUE(detail.find("加载配置文件失败") != std::string::npos);
        EXPECT_TRUE(detail.find("系统初始化失败") != std::string::npos);
    }
}

// 测试异常的日志级别过滤
TEST(ExceptionTest, LogLevelFilterTest) {
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

// 测试异常的动态复制和重抛
TEST(ExceptionTest, DynamicCopyAndRethrowTest) {
    // 注册异常类型
    MC_REGISTER_EXCEPTION(mc::timeout_exception);
    MC_REGISTER_EXCEPTION(mc::invalid_arg_exception);

    try {
        // 创建一个超时异常
        auto original = MC_MAKE_EXCEPTION(mc::timeout_exception, "操作超时: ${operation}",
                                          ("operation", "数据库查询"));

        // 动态复制异常
        std::shared_ptr<mc::exception> copy = original.dynamic_copy_exception();

        // 验证复制的异常
        EXPECT_EQ(copy->code(), mc::exception_code::timeout_exception_code);
        EXPECT_STREQ(copy->name(), "timeout");
        EXPECT_TRUE(copy->to_string().find("数据库查询") != std::string::npos);

        // 动态重抛异常
        copy->dynamic_rethrow_exception();
        FAIL() << "应该重新抛出异常";
    } catch (const mc::timeout_exception& e) {
        // 验证重抛的异常保持了正确的类型和信息
        EXPECT_EQ(e.code(), mc::exception_code::timeout_exception_code);
        EXPECT_STREQ(e.name(), "timeout");
        EXPECT_TRUE(e.to_string().find("数据库查询") != std::string::npos);
    }
}

// 测试异常的线程安全性
TEST(ExceptionTest, ThreadSafetyTest) {
    // 创建一个共享的异常对象
    mc::exception shared_exception(mc::exception_code::unknow_exception_code, "thread_test",
                                   "线程测试异常");

    // 创建多个线程，每个线程都添加日志消息
    const int                num_threads     = 10;
    const int                msgs_per_thread = 100;
    std::vector<std::thread> threads;
    std::mutex               mutex;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&shared_exception, &mutex, i]() {
            for (int j = 0; j < msgs_per_thread; ++j) {
                // 使用互斥锁保护共享对象
                std::lock_guard<std::mutex> lock(mutex);
                shared_exception.append_log(
                    MC_LOG_MESSAGE(info, "线程 ${thread} 消息 ${msg}",
                                   ("thread", std::to_string(i))("msg", std::to_string(j))));
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

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
TEST(ExceptionTest, PerformanceTest) {
    const int iterations = 10000;

    // 测试异常创建性能
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto e = MC_MAKE_EXCEPTION(mc::timeout_exception, "测试消息 ${index}",
                                       ("index", std::to_string(i)));
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
                MC_THROW(mc::timeout_exception, "测试消息 ${index}", ("index", std::to_string(i)));
            } catch (const mc::exception& e) {
                EXPECT_EQ(e.code(), mc::exception_code::timeout_exception_code);
            }
        }
        auto end      = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "抛出和捕获 " << iterations << " 个异常耗时: " << duration << " 毫秒"
                  << std::endl;
    }
}

// 测试标准库异常的包装
TEST(ExceptionTest, StdExceptionWrapperTest) {
    try {
        try {
            throw std::runtime_error("标准库运行时错误");
        } catch (const std::exception& e) {
            MC_THROW(mc::system_exception, "系统错误: ${error}", ("error", e.what()));
        }
        FAIL() << "应该抛出异常";
    } catch (const mc::system_exception& e) {
        EXPECT_EQ(e.code(), mc::exception_code::system_error_code);
        EXPECT_STREQ(e.name(), "system");
        EXPECT_TRUE(e.to_string().find("标准库运行时错误") != std::string::npos);
    }
}

// 测试异常的格式化输出
TEST(ExceptionTest, FormattedOutputTest) {
    // 创建一个带有完整上下文信息的异常
    auto e = MC_MAKE_EXCEPTION(mc::invalid_arg_exception, "参数验证失败: ${param} = ${value}",
                               ("param", "count")("value", "-1"));

    e.append_log(MC_LOG_MESSAGE(debug, "参数详情: ${detail}", ("detail", "必须大于0")));
    e.append_log(MC_LOG_MESSAGE(info, "处理函数: ${func}", ("func", "process_data")));
    e.append_log(MC_LOG_MESSAGE(warn, "可能影响: ${impact}", ("impact", "数据不完整")));
    e.append_log(MC_LOG_MESSAGE(error, "错误代码: ${code}", ("code", "E001")));

    // 测试简单格式输出
    std::string simple = e.to_string();
    EXPECT_TRUE(simple.find("错误代码") != std::string::npos);
    EXPECT_TRUE(simple.find("E001") != std::string::npos);

    // 测试详细格式输出
    std::string detail = e.to_detail_string();
    EXPECT_TRUE(detail.find("必须大于0") != std::string::npos);
    EXPECT_TRUE(detail.find("process_data") != std::string::npos);
    EXPECT_TRUE(detail.find("数据不完整") != std::string::npos);
    EXPECT_TRUE(detail.find("E001") != std::string::npos);

    // 验证日志级别和时间戳格式
    EXPECT_TRUE(detail.find("DEBUG") != std::string::npos);
    EXPECT_TRUE(detail.find("INFO") != std::string::npos);
    EXPECT_TRUE(detail.find("WARN") != std::string::npos);
    EXPECT_TRUE(detail.find("ERROR") != std::string::npos);
    EXPECT_TRUE(detail.find("-") != std::string::npos); // 日期分隔符
    EXPECT_TRUE(detail.find(":") != std::string::npos); // 时间分隔符
}
