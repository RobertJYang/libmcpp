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
#include <functional>

// 测试MC_ASSERT宏
TEST(ExceptionMacroTest, AssertMacroTest) {
    // 测试条件为真的情况
    bool no_throw = true;
    try {
        MC_ASSERT(true, "这个断言不应该触发");
        no_throw = true;
    } catch (...) {
        no_throw = false;
    }
    EXPECT_TRUE(no_throw) << "条件为真时，断言不应该抛出异常";
    
    // 测试条件为假的情况
    try {
        MC_ASSERT(false, "断言失败测试");
        FAIL() << "条件为假时，断言应该抛出异常";
    } catch (const mc::assert_exception& e) {
        EXPECT_EQ(e.code(), mc::condition_error_code);
        EXPECT_STREQ(e.name(), "assert");
        EXPECT_TRUE(e.to_string().find("断言失败测试") != std::string::npos);
    } catch (...) {
        FAIL() << "断言应该抛出assert_exception类型的异常";
    }
}

// 测试MC_THROW宏
TEST(ExceptionMacroTest, ThrowMacroTest) {
    try {
        MC_THROW(mc::timeout_exception, "超时测试");
        FAIL() << "MC_THROW应该抛出异常";
    } catch (const mc::timeout_exception& e) {
        EXPECT_EQ(e.code(), mc::time_limit_error_code);
        EXPECT_STREQ(e.name(), "timeout");
        EXPECT_TRUE(e.to_string().find("超时测试") != std::string::npos);
        EXPECT_FALSE(e.to_string().find(__FILE__) == std::string::npos) << "异常信息应该包含文件名";
    } catch (...) {
        FAIL() << "MC_THROW应该抛出指定类型的异常";
    }
}

// 测试MC_RETHROW_EXCEPTION宏
TEST(ExceptionMacroTest, RethrowMacroTest) {
    try {
        try {
            MC_THROW(mc::invalid_arg_exception, "原始参数错误");
        } catch (mc::invalid_arg_exception& e) {
            // 添加上下文信息并重新抛出
            MC_RETHROW_EXCEPTION(e, "重新抛出的错误");
        }
        FAIL() << "MC_RETHROW_EXCEPTION应该重新抛出异常";
    } catch (const mc::invalid_arg_exception& e) {
        EXPECT_EQ(e.code(), mc::argument_error_code);
        EXPECT_STREQ(e.name(), "invalid_arg");
        
        // 检查日志消息
        std::string detail = e.to_detail_string();
        EXPECT_TRUE(detail.find("原始参数错误") != std::string::npos);
        EXPECT_TRUE(detail.find("重新抛出的错误") != std::string::npos);
    } catch (...) {
        FAIL() << "MC_RETHROW_EXCEPTION应该重新抛出相同类型的异常";
    }
}

// 测试MC_CAPTURE_AND_WRAP_EXCEPTION宏
TEST(ExceptionMacroTest, CaptureAndWrapMacroTest) {
    try {
        try {
            throw std::out_of_range("索引越界");
        } MC_CAPTURE_AND_WRAP_EXCEPTION("处理数组时出错");
        FAIL() << "MC_CAPTURE_AND_WRAP_EXCEPTION应该包装并重新抛出异常";
    } catch (const mc::std_exception_wrapper& e) {
        EXPECT_EQ(e.code(), mc::system_error_code);
        std::string str = e.to_string();
        EXPECT_TRUE(str.find("处理数组时出错") != std::string::npos);
        EXPECT_TRUE(str.find("索引越界") != std::string::npos);
        
        // 检查内部异常
        std::exception_ptr inner = e.get_inner_exception();
        EXPECT_TRUE(inner != nullptr);
    } catch (...) {
        FAIL() << "MC_CAPTURE_AND_WRAP_EXCEPTION应该抛出std_exception_wrapper类型的异常";
    }
}

// 测试宏的嵌套使用
TEST(ExceptionMacroTest, NestedMacroTest) {
    // 定义一个可能抛出异常的函数
    auto throw_func = []() {
        MC_THROW(mc::key_not_found_exception, "键不存在");
    };
    
    // 定义一个捕获并重新抛出异常的函数
    auto catch_and_rethrow = [&throw_func]() {
        try {
            throw_func();
        } catch (mc::key_not_found_exception& e) {
            MC_RETHROW_EXCEPTION(e, "在处理字典时");
        }
    };
    
    // 测试嵌套使用
    try {
        try {
            catch_and_rethrow();
        } catch (mc::exception& e) {
            // 添加更多上下文信息
            e.append_log(mc::log_message(mc::log_level::warn, "外层处理", __FILE__, __LINE__));
            throw e;
        }
    } catch (const mc::exception& e) {
        // 验证所有日志消息都被保留
        std::string detail = e.to_detail_string();
        EXPECT_TRUE(detail.find("键不存在") != std::string::npos);
        EXPECT_TRUE(detail.find("在处理字典时") != std::string::npos);
        EXPECT_TRUE(detail.find("外层处理") != std::string::npos);
    }
}

// 测试宏的性能
TEST(ExceptionMacroTest, MacroPerformanceTest) {
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        try {
            if (i % 2 == 0) {
                MC_ASSERT(i >= 0, "断言测试");
                // 断言成功，不会抛出异常
            } else {
                MC_THROW(mc::timeout_exception, "超时测试");
                FAIL() << "MC_THROW应该抛出异常";
            }
        } catch (const mc::exception& e) {
            // 捕获异常并继续
            EXPECT_FALSE(e.to_string().empty());
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // 输出性能信息
    std::cout << "执行 " << iterations << " 次异常宏操作耗时: " << duration << " 毫秒" << std::endl;
} 