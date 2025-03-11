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

#include <mc/exception.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>

// 测试基本异常功能
TEST(ExceptionTest, BasicExceptionTest) {
    // 创建一个基本异常
    mc::exception e(mc::general_error_code, "test_exception", "测试异常");
    
    // 检查异常属性
    EXPECT_EQ(e.code(), mc::general_error_code);
    EXPECT_STREQ(e.name(), "test_exception");
    EXPECT_STREQ(e.what(), "测试异常");
    
    // 测试异常消息
    EXPECT_EQ(e.top_message(), "测试异常");
    
    // 测试字符串表示
    std::string str = e.to_string();
    EXPECT_TRUE(str.find("test_exception") != std::string::npos);
    EXPECT_TRUE(str.find("测试异常") != std::string::npos);
}

// 测试带日志消息的异常
TEST(ExceptionTest, LogMessageTest) {
    // 创建带日志消息的异常
    mc::log_message msg(mc::log_level::error, "错误消息", "test_file.cpp", 42);
    mc::exception e(std::move(msg), mc::argument_error_code, "invalid_arg", "无效参数");
    
    // 检查异常属性
    EXPECT_EQ(e.code(), mc::argument_error_code);
    EXPECT_STREQ(e.name(), "invalid_arg");
    EXPECT_STREQ(e.what(), "无效参数");
    
    // 测试顶层消息
    EXPECT_EQ(e.top_message(), "错误消息");
    
    // 测试详细字符串表示
    std::string detail_str = e.to_detail_string();
    EXPECT_TRUE(detail_str.find("invalid_arg") != std::string::npos);
    EXPECT_TRUE(detail_str.find("无效参数") != std::string::npos);
    EXPECT_TRUE(detail_str.find("错误消息") != std::string::npos);
    EXPECT_TRUE(detail_str.find("test_file.cpp:42") != std::string::npos);
}

// 测试异常追加日志
TEST(ExceptionTest, AppendLogTest) {
    // 创建异常
    mc::exception e(mc::time_limit_error_code, "timeout", "操作超时");
    
    // 追加日志消息
    e.append_log(mc::log_message(mc::log_level::warn, "警告消息", "file1.cpp", 10));
    e.append_log(mc::log_message(mc::log_level::error, "错误消息", "file2.cpp", 20));
    
    // 测试顶层消息
    EXPECT_EQ(e.top_message(), "错误消息");
    
    // 测试详细字符串表示
    std::string detail_str = e.to_detail_string();
    EXPECT_TRUE(detail_str.find("警告消息") != std::string::npos);
    EXPECT_TRUE(detail_str.find("错误消息") != std::string::npos);
    EXPECT_TRUE(detail_str.find("file1.cpp:10") != std::string::npos);
    EXPECT_TRUE(detail_str.find("file2.cpp:20") != std::string::npos);
}

// 测试异常复制
TEST(ExceptionTest, CopyTest) {
    // 创建原始异常
    mc::exception original(mc::syntax_error_code, "parse_error", "解析错误");
    original.append_log(mc::log_message(mc::log_level::error, "JSON解析失败", "parser.cpp", 100));
    
    // 复制异常
    mc::exception copy = original;
    
    // 检查复制后的属性
    EXPECT_EQ(copy.code(), original.code());
    EXPECT_STREQ(copy.name(), original.name());
    EXPECT_STREQ(copy.what(), original.what());
    EXPECT_EQ(copy.top_message(), original.top_message());
    EXPECT_EQ(copy.to_detail_string(), original.to_detail_string());
}

// 测试异常动态复制
TEST(ExceptionTest, DynamicCopyTest) {
    // 创建原始异常
    mc::exception original(mc::missing_key_error_code, "key_not_found", "键未找到");
    original.append_log(mc::log_message(mc::log_level::error, "键'config'不存在", "config.cpp", 50));
    
    // 动态复制异常
    std::shared_ptr<mc::exception> copy = original.dynamic_copy_exception();
    
    // 检查复制后的属性
    EXPECT_EQ(copy->code(), original.code());
    EXPECT_STREQ(copy->name(), original.name());
    EXPECT_STREQ(copy->what(), original.what());
    EXPECT_EQ(copy->top_message(), original.top_message());
    EXPECT_EQ(copy->to_detail_string(), original.to_detail_string());
}

// 测试未处理异常
TEST(ExceptionTest, UnhandledExceptionTest) {
    try {
        // 创建一个标准库异常
        throw std::runtime_error("运行时错误");
    } catch (...) {
        // 捕获并包装为未处理异常
        mc::unhandled_exception e(mc::log_message(mc::log_level::error, "捕获到未处理异常"));
        
        // 检查异常属性
        EXPECT_EQ(e.code(), mc::external_error_code);
        EXPECT_STREQ(e.name(), "unhandled");
        EXPECT_TRUE(e.get_inner_exception() != nullptr);
        
        // 测试字符串表示
        std::string str = e.to_string();
        EXPECT_TRUE(str.find("unhandled") != std::string::npos);
        EXPECT_TRUE(str.find("捕获到未处理异常") != std::string::npos);
    }
}

// 测试标准异常包装
TEST(ExceptionTest, StdExceptionWrapperTest) {
    try {
        // 创建一个标准库异常
        throw std::invalid_argument("无效的参数值");
    } catch (const std::exception& e) {
        // 包装为标准异常包装器
        mc::std_exception_wrapper wrapper = mc::std_exception_wrapper::from_current_exception(e);
        
        // 检查异常属性
        EXPECT_EQ(wrapper.code(), mc::system_error_code);
        EXPECT_STREQ(wrapper.name(), "std_exception");
        EXPECT_TRUE(wrapper.get_inner_exception() != nullptr);
        
        // 测试字符串表示
        std::string str = wrapper.to_string();
        EXPECT_TRUE(str.find("std_exception") != std::string::npos);
        EXPECT_TRUE(str.find("无效的参数值") != std::string::npos);
    }
}

// 测试异常宏
TEST(ExceptionTest, MacroTest) {
    // 测试断言宏
    try {
        bool condition = false;
        MC_ASSERT(condition, "条件检查失败");
        FAIL() << "断言应该抛出异常";
    } catch (const mc::assert_exception& e) {
        EXPECT_EQ(e.code(), mc::condition_error_code);
        EXPECT_STREQ(e.name(), "assert");
        EXPECT_TRUE(e.to_string().find("条件检查失败") != std::string::npos);
    }
    
    // 测试抛出异常宏
    try {
        MC_THROW(mc::timeout_exception, "操作超时了");
        FAIL() << "应该抛出超时异常";
    } catch (const mc::timeout_exception& e) {
        EXPECT_EQ(e.code(), mc::time_limit_error_code);
        EXPECT_STREQ(e.name(), "timeout");
        EXPECT_TRUE(e.to_string().find("操作超时了") != std::string::npos);
    }
    
    // 测试重新抛出异常宏
    try {
        try {
            MC_THROW(mc::invalid_arg_exception, "参数无效");
        } catch (mc::invalid_arg_exception& e) {
            MC_RETHROW_EXCEPTION(e, "在处理请求时");
        }
        FAIL() << "应该重新抛出异常";
    } catch (const mc::invalid_arg_exception& e) {
        EXPECT_EQ(e.code(), mc::argument_error_code);
        EXPECT_STREQ(e.name(), "invalid_arg");
        
        // 检查日志消息
        std::string detail_str = e.to_detail_string();
        EXPECT_TRUE(detail_str.find("参数无效") != std::string::npos);
        EXPECT_TRUE(detail_str.find("在处理请求时") != std::string::npos);
    }
    
    // 测试捕获并包装标准异常宏
    try {
        try {
            throw std::out_of_range("索引越界");
        } MC_CAPTURE_AND_WRAP_EXCEPTION("处理数组时出错");
        FAIL() << "应该包装并重新抛出异常";
    } catch (const mc::std_exception_wrapper& e) {
        EXPECT_EQ(e.code(), mc::system_error_code);
        EXPECT_TRUE(e.to_string().find("处理数组时出错") != std::string::npos);
        EXPECT_TRUE(e.to_string().find("索引越界") != std::string::npos);
    }
}

// 测试异常工厂
TEST(ExceptionTest, ExceptionFactoryTest) {
    // 注册异常类型
    mc::exception_factory::instance().register_exception<mc::timeout_exception>();
    mc::exception_factory::instance().register_exception<mc::invalid_arg_exception>();
    
    // 创建一个异常
    mc::timeout_exception original(mc::log_message(mc::log_level::error, "操作超时"));
    
    // 通过异常工厂重新抛出
    try {
        mc::exception_factory::instance().rethrow(original);
        FAIL() << "应该重新抛出异常";
    } catch (const mc::timeout_exception& e) {
        // 应该捕获到正确类型的异常
        EXPECT_EQ(e.code(), mc::time_limit_error_code);
        EXPECT_STREQ(e.name(), "timeout");
    } catch (...) {
        FAIL() << "捕获到了错误类型的异常";
    }
}

// 测试特定模块异常
class variant_exception : public mc::exception {
public:
    enum code_enum {
        code_value = mc::variant_error_code,
    };
    
    variant_exception(mc::log_message&& msg = mc::log_message(mc::log_level::error, "变体类型错误"))
        : exception(std::move(msg), mc::variant_error_code, "variant_error", "变体类型错误")
    {
    }
    
    variant_exception(const variant_exception& e) : exception(e) {}
    variant_exception(variant_exception&& e) : exception(std::move(e)) {}
    
    // 从基类构造
    explicit variant_exception(const mc::exception& e) : exception(e) {
        // 不直接访问m_impl，而是在测试中使用基类的功能
    }
    
    virtual std::shared_ptr<mc::exception> dynamic_copy_exception() const override {
        return std::make_shared<variant_exception>(*this);
    }
    
    virtual void dynamic_rethrow_exception() const override {
        throw *this;
    }
};

TEST(ExceptionTest, ModuleSpecificExceptionTest) {
    // 测试变体异常
    try {
        throw variant_exception(mc::log_message(mc::log_level::error, "无法转换为整数类型"));
        FAIL() << "应该抛出变体异常";
    } catch (const variant_exception& e) {
        EXPECT_EQ(e.code(), mc::variant_error_code);
        EXPECT_STREQ(e.name(), "variant_error");
        EXPECT_TRUE(e.to_string().find("无法转换为整数类型") != std::string::npos);
    }
}

// 测试异常工厂的动态创建和重新抛出
TEST(ExceptionTest, ExceptionFactoryDynamicTest) {
    // 创建一个异常对象
    mc::exception original(mc::variant_error_code, "test_variant", "测试变体异常");
    
    // 注册变体异常类型
    mc::exception_factory::instance().register_exception<variant_exception>();
    
    // 通过异常工厂重新抛出
    try {
        mc::exception_factory::instance().rethrow(original);
        FAIL() << "应该重新抛出异常";
    } catch (const variant_exception& e) {
        // 应该捕获到变体异常类型
        EXPECT_EQ(e.code(), mc::variant_error_code);
        EXPECT_STREQ(e.name(), "test_variant"); // 名称应该被重置为变体异常的名称
    } catch (const mc::exception& e) {
        FAIL() << "捕获到了错误类型的异常: " << e.to_string();
    }
}

// 测试异常的嵌套捕获和重新抛出
TEST(ExceptionTest, NestedExceptionTest) {
    try {
        try {
            try {
                // 最内层抛出变体异常
                throw variant_exception(mc::log_message(mc::log_level::error, "内层异常"));
            } catch (const variant_exception& e) {
                // 中层捕获并添加上下文信息
                mc::exception middle = e;
                middle.append_log(mc::log_message(mc::log_level::warn, "中层处理", "middle.cpp", 50));
                throw middle;
            }
        } catch (const mc::exception& e) {
            // 外层捕获并添加更多上下文信息
            mc::exception outer = e;
            outer.append_log(mc::log_message(mc::log_level::info, "外层处理", "outer.cpp", 100));
            throw outer;
        }
    } catch (const mc::exception& e) {
        // 验证所有日志消息都被保留
        std::string detail = e.to_detail_string();
        EXPECT_TRUE(detail.find("内层异常") != std::string::npos);
        EXPECT_TRUE(detail.find("中层处理") != std::string::npos);
        EXPECT_TRUE(detail.find("外层处理") != std::string::npos);
        EXPECT_TRUE(detail.find("middle.cpp:50") != std::string::npos);
        EXPECT_TRUE(detail.find("outer.cpp:100") != std::string::npos);
    }
}

// 测试异常的性能
TEST(ExceptionTest, PerformanceTest) {
    // 测试创建和销毁异常的性能
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        mc::exception e(mc::general_error_code, "perf_test", "性能测试异常");
        e.append_log(mc::log_message(mc::log_level::info, "性能测试日志"));
        std::string s = e.to_string();
        EXPECT_FALSE(s.empty());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // 输出性能信息，但不做硬性断言（因为不同环境性能不同）
    std::cout << "创建和处理 " << iterations << " 个异常耗时: " << duration << " 毫秒" << std::endl;
}

// 测试异常与标准库异常的互操作性
TEST(ExceptionTest, InteroperabilityTest) {
    // 测试从标准库异常转换到MC异常
    try {
        try {
            throw std::runtime_error("标准库运行时错误");
        } catch (const std::exception& e) {
            mc::std_exception_wrapper wrapper(
                mc::log_message(mc::log_level::error, "包装标准异常", "interop.cpp", 42),
                std::current_exception()
            );
            throw wrapper;
        }
    } catch (const mc::std_exception_wrapper& e) {
        EXPECT_EQ(e.code(), mc::system_error_code);
        std::string detail = e.to_detail_string();
        EXPECT_TRUE(detail.find("标准库运行时错误") != std::string::npos);
        EXPECT_TRUE(detail.find("包装标准异常") != std::string::npos);
        EXPECT_TRUE(detail.find("interop.cpp:42") != std::string::npos);
        
        // 测试获取内部异常
        std::exception_ptr inner = e.get_inner_exception();
        EXPECT_TRUE(inner != nullptr);
        
        // 尝试重新抛出内部异常
        bool caught_inner = false;
        try {
            std::rethrow_exception(inner);
        } catch (const std::runtime_error& re) {
            EXPECT_STREQ(re.what(), "标准库运行时错误");
            caught_inner = true;
        } catch (...) {
            FAIL() << "捕获到了错误类型的内部异常";
        }
        EXPECT_TRUE(caught_inner);
    }
}

// 测试异常的线程安全性
TEST(ExceptionTest, ThreadSafetyTest) {
    // 创建一个共享的异常对象
    mc::exception shared_exception(mc::general_error_code, "thread_test", "线程测试异常");
    
    // 创建多个线程，每个线程都添加日志消息
    const int num_threads = 10;
    const int msgs_per_thread = 100;
    std::vector<std::thread> threads;
    std::mutex mutex; // 用于保护共享异常对象
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&shared_exception, &mutex, i, msgs_per_thread]() {
            for (int j = 0; j < msgs_per_thread; ++j) {
                std::string msg = "线程 " + std::to_string(i) + " 消息 " + std::to_string(j);
                mc::log_message log_msg(mc::log_level::info, msg, "thread.cpp", i * 100 + j);
                
                // 使用互斥锁保护共享对象
                std::lock_guard<std::mutex> lock(mutex);
                shared_exception.append_log(std::move(log_msg));
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证所有日志消息都被添加
    std::string detail = shared_exception.to_detail_string();
    int msg_count = 0;
    
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